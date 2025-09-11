#include "../FGlobal_flib.h"

#define TAG FEADRE_TAG("freg_allocator")

#include "freg_allocator.h"

using namespace feadre;

// ---------- 工具 ----------
static inline std::vector<int> to_sorted_vec(const std::unordered_set<int> &s) {
    std::vector<int> v;
    v.reserve(s.size());
    for (int x: s) v.push_back(x);
    std::sort(v.begin(), v.end());
    return v;
}

static inline std::vector<int> locked_old_range(int base) {
    std::vector<int> v;
    if (base <= 0) return v;
    v.resize((size_t) base);
    for (int i = 0; i < base; ++i) v[(size_t) i] = i;
    return v;
}

static inline void sort_unique(std::vector<int> &v) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
}

// ===== 基础检查 =====
/**
 * 基础检查
 * @param ir CodeIr 指针
 */
inline void FRegAllocator::CheckIr(const lir::CodeIr *ir) {
    SLICER_CHECK(ir && ir->ir_method && ir->ir_method->code);
}

/**
 * 构造
 * @param code_ir CodeIr 指针
 * @return
 */
FRegAllocator::FRegAllocator(lir::CodeIr *code_ir) : code_ir_(code_ir) {
    CheckIr(code_ir_);
}

/**
 * @return 总寄存器数量
 */
dex::u2 FRegAllocator::TotalRegs() const {
    CheckIr(code_ir_);
    return code_ir_->ir_method->code->registers;
}

/**
 * @return 参数寄存器数量（高位）
 */
dex::u2 FRegAllocator::InsCount() const {
    CheckIr(code_ir_);
    return code_ir_->ir_method->code->ins_count;
}

/**
 * @return 本地寄存器数量（低位）
 */
int FRegAllocator::Locals() const {
    CheckIr(code_ir_);
    const auto *code = code_ir_->ir_method->code;
    return static_cast<int>(code->registers - code->ins_count);
}

/**
 * 确保至少有 need_locals 个本地寄存器；不足则扩容
 * @param need_locals 目标本地寄存器数
 * @return 本次新增寄存器数量
 */
dex::u2 FRegAllocator::EnsureLocals(dex::u2 need_locals) {
    CheckIr(code_ir_);
    auto *code = code_ir_->ir_method->code;
    const dex::u2 orig_total = code->registers;
    const dex::u2 ins_size = code->ins_count;
    const dex::u2 orig_locals = static_cast<dex::u2>(orig_total - ins_size);
    if (orig_locals >= need_locals) {
        LOGI("[EnsureLocals] total=%u ins=%u locals=%u need=%u -> add=0",
             code->registers, ins_size, orig_locals, need_locals);
        return 0;
    }
    const dex::u2 add = static_cast<dex::u2>(need_locals - orig_locals);
    code->registers = static_cast<dex::u2>(orig_total + add);
    LOGI("[EnsureLocals] total:%u->%u ins=%u locals:%u->%u add=%u",
         orig_total, code->registers, ins_size, orig_locals, need_locals, add);
    return add;
}

/**
 * 在现有基础上增加本地寄存器
 * @param add 要增加的数量
 * @param num_22c 要预留的22c指令 寄存器数量
 * @return 旧段大小（扩容前的本地寄存器个数）
 */
int FRegAllocator::GrowLocals(dex::u2 add, uint16_t num_22c) {
    CHECK_BOOL_EXPR(add < num_22c, "添加数不能 < num_22c");

    CheckIr(code_ir_);
    auto *code = code_ir_->ir_method->code;
    const dex::u2 orig_total = code->registers;
    const dex::u2 ins_size = code->ins_count;
    const dex::u2 orig_locals = static_cast<dex::u2>(orig_total - ins_size);

    num_reg_non_param_orig_ = static_cast<int>(orig_locals);

    if (add == 0) {
        LOGI("[GrowLocals] +0 total= %u，ins= %u，locals= %u", orig_total, ins_size, orig_locals);
        return static_cast<int>(orig_locals);
    }

    code->registers = static_cast<dex::u2>(orig_total + add);
    const dex::u2 new_locals = static_cast<dex::u2>(code->registers - ins_size);
    LOGI("[GrowLocals] add= %u，total:%u->%u ins= %u，locals:%u->%u",
         add,num_22c, orig_total, code->registers, ins_size, orig_locals, new_locals);

    regs_22c_.clear();
    int need = (int) num_22c;

    //  若旧本地 <16，优先从“新增段里的低位”塞入，直到 16 之前
    if (orig_locals < 16 && add > 0) {
        int can_take = std::min(need, std::max(0, 16 - (int) orig_locals));
        can_take = std::min(can_take, (int) add);
        for (int i = 0; i < can_take; ++i) regs_22c_.insert((int) orig_locals + i);
        need -= can_take;
    }

    // 不够再从旧段 [0..15] 里倒序补（避免占用更靠前的 0、1 可留给其它 22c 指令）
    for (int v = 15; need > 0 && v >= 0; --v) {
        if (v < (int) orig_locals) {
            regs_22c_.insert(v);
            --need;
        }
    }

    if (need > 0) {
        LOGW("[GrowLocals] num_22c need= %u， got= %zu (locals_before= %u)", num_22c,
             regs_22c_.size(), orig_locals);
    } else {
        LOGD("[GrowLocals] num_22c= %u，regs= %s", num_22c, VEC_CSTR(to_sorted_vec(regs_22c_)));
    }

    return num_reg_non_param_orig_; // 旧段大小
}

/**
 * 锁定旧段：[0, base) 不可用（仅对本分配器生效）
 * @param base 旧段大小
 * @return
 */
void FRegAllocator::LockOldBase(int base) {
    lock_old_base_ = std::max(0, base);
    LOGD("[LockOldBase] base=%d locked=%s", lock_old_base_,
         VEC_CSTR(locked_old_range(lock_old_base_)));
}

/**
 * 添加全局禁用位（对单/宽均生效）
 * @param forbidden 需要禁用的 v 列表
 * @return
 */
void FRegAllocator::AddGlobalForbid(const std::vector<int> &forbidden) {
    int locals = Locals();
    std::vector<int> added;
    for (int v: forbidden) {
        if (v >= 0 && v < locals) {
            if (global_forbid_.insert(v).second) added.push_back(v);
        }
    }
    sort_unique(added);
    LOGD("[AddGlobalForbid] add=%s all=%s", VEC_CSTR(added),
         VEC_CSTR(to_sorted_vec(global_forbid_)));
}

std::vector<int> FRegAllocator::GetRegs22c(int count) const {
    std::vector<int> out;
    int sz = (int) regs_22c_.size();
    if (count < 0 || count > sz) {
        LOGE("[GetRegs22c] count=%d > size=%d", count, sz);
        return out;
    }

    auto vec = to_sorted_vec(regs_22c_);
    out.assign(vec.begin(), vec.begin() + count);
    LOGD("[GetRegs22c] count=%d out=%s all=%s", count, VEC_CSTR(out), VEC_CSTR(vec));
    return out;
}

/**
 * 构建单寄存器 forbid mask（含锁旧段/全局禁用/已占用/宽对占两格/本次额外）
 * @param extra 本次额外禁用
 * @return mask（1=不可用）
 */
std::vector<uint8_t> FRegAllocator::MakeForbidMaskSingles(const std::vector<int> &extra) const {
    const int locals = Locals();
    std::vector<uint8_t> mask(locals > 0 ? (size_t) locals : 0, 0);
    auto mark = [&](int v) { if (v >= 0 && v < locals) mask[(size_t) v] = 1; };
//    for (int v = 0; v < lock_old_base_; ++v) mark(v);                 // 锁旧段
    for (int v: global_forbid_) mark(v);                             // 全局禁用
    for (int v: used_single_) mark(v);                             // 已占用单

    for (int v: used_wide_start_) {
        mark(v);
        mark(v + 1);
    }          // 已占用宽（两格）
    for (int v: regs_22c_) mark(v);                  // 22c 预留位禁止普通分配
    for (int v: extra) mark(v);                                      // 额外
    return mask;
}

/**
 * 构建宽寄存器 forbid mask（含锁旧段/全局禁用/已占用/宽对两格/本次额外）
 * @param extra 本次额外禁用（对起点与起点+1 生效）
 * @return mask（1=不可用）
 */
std::vector<uint8_t> FRegAllocator::MakeForbidMaskWides(const std::vector<int> &extra) const {
    const int locals = Locals();
    std::vector<uint8_t> mask(locals > 0 ? (size_t) locals : 0, 0);
    auto mark = [&](int v) { if (v >= 0 && v < locals) mask[(size_t) v] = 1; };
//    for (int v = 0; v < lock_old_base_; ++v) mark(v);                 // 锁旧段
    for (int v: global_forbid_) mark(v);                             // 全局禁用
    for (int v: used_single_) mark(v);                             // 已占用单
    for (int v: used_wide_start_) {
        mark(v);
        mark(v + 1);
    }          // 已占用宽（两格）
    for (int v: regs_22c_) mark(v);     // 若 v+1 在 22c 中，下面的双格判断也会挡住
    for (int v: extra) mark(v);                                      // 额外
    return mask;
}

/**
 * 任意可用段分配 宽寄存器；extra_forbid 会被持久记入该 owner 的禁用表
 * @param owner 分组
 * @param count 数量
 * @param prefer_low_index 是否优先低位
 * @param extra_forbid 额外禁用（会持久）
 * @return 租约
 */
std::vector<int> FRegAllocator::AllocWides(const std::string &owner,
                                           int count,
                                           bool prefer_low_index,
                                           const std::vector<int> &extra_forbid) {
    return AllocWidesImpl(owner, count, prefer_low_index, false, extra_forbid);
}

/**
 * 只从新段分配 宽寄存器；extra_forbid 会被持久记入该 owner 的禁用表
 * @param owner 分组
 * @param count 数量
 * @param extra_forbid 额外禁用（会持久）
 * @return 租约
 */
std::vector<int> FRegAllocator::AllocWidesTail(const std::string &owner,
                                               int count,
                                               const std::vector<int> &extra_forbid) {
    return AllocWidesImpl(owner, count, true, true, extra_forbid);
}

/**
 * 宽寄存器分配实现；将 extra_forbid 加入 owner 的持久禁用，再参与计算
 * @param owner 分组
 * @param count 数量
 * @param prefer_low_index 是否优先低位
 * @param tail_only 是否只用新段
 * @param extra_forbid 额外禁用（会持久）
 * @return 租约
 */
std::vector<int> FRegAllocator::AllocWidesImpl(const std::string &owner,
                                               int count,
                                               bool prefer_low_index,
                                               bool tail_only,
                                               const std::vector<int> &extra_forbid) {

    FRegLease lease;
    lease.owner = owner;
    lease.tail_only = tail_only;
    const int locals = Locals();

    if (count <= 0 || locals <= 1) {
        LOGE("[AllocWides] owner=%s cnt=%d locals=%d -> no-op", owner.c_str(), count, locals);
        return lease.wides;
    }

    // 1) 将本次 extra_forbid 记入 owner 的持久禁用（针对宽对：禁用的是起点位）
    auto &forbid_set = owner_forbid_wides_[owner];
    for (int v: extra_forbid) if (v >= 0 && v < locals) forbid_set.insert(v);

    // 2) 参与计算的“额外禁用” = owner 持久禁用 ∪ 本次 extra
    std::vector<int> extra(forbid_set.begin(), forbid_set.end());
    for (int v: extra_forbid) if (v >= 0 && v < locals) extra.push_back(v);
    sort_unique(extra);

    // 3) 打日志（单行）
    LOGD("[AllocWides] owner=%s cnt=%d tail=%d prefLow=%d locals=%d lockBase=%d glob=%s usedS=%s usedW=%s forbid=%s",
         owner.c_str(), count, tail_only ? 1 : 0, prefer_low_index ? 1 : 0, locals, lock_old_base_,
         VEC_CSTR(to_sorted_vec(global_forbid_)),
         VEC_CSTR(to_sorted_vec(used_single_)),
         VEC_CSTR(to_sorted_vec(used_wide_start_)),
         VEC_CSTR(extra));

    auto used = MakeForbidMaskWides(extra);

    auto try_scan = [&](int from, int to, int step) {
        for (int v = from; v != to && (int) lease.wides.size() < count; v += step) {
            if (v == 0x0F) continue;                 // 避免跨 4bit 边界 (15,16)
            if (v < 0 || v + 1 >= locals) continue;
            if (tail_only && (v < lock_old_base_ || v + 1 < lock_old_base_)) continue;
            if (!used[(size_t) v] && !used[(size_t) (v + 1)]) {
                lease.wides.push_back(v);
                used[(size_t) v] = 1;
                used[(size_t) (v + 1)] = 1;
            }
        }
    };

    if (prefer_low_index) try_scan(0, locals - 1, +1);
    else try_scan(locals - 2, -1, -1);

    // 占用登记 + 结果日志
    for (int v: lease.wides) {
        used_wide_start_.insert(v);
        owner_wides_[owner].insert(v);
    }
    LOGD("[AllocWides] owner=%s got=%s/%d", owner.c_str(), VEC_CSTR(lease.wides), count);

    return lease.wides;
}

/**
 * 任意可用段分配 单寄存器；extra_forbid 会被持久记入该 owner 的禁用表
 * @param owner 分组
 * @param count 数量
 * @param prefer_low_index 是否优先低位
 * @param extra_forbid 额外禁用（会持久）
 * @return 租约
 */
std::vector<int> FRegAllocator::AllocSingles(const std::string &owner,
                                             int count,
                                             bool prefer_low_index,
                                             const std::vector<int> &extra_forbid) {
    return AllocSinglesImpl(owner, count, prefer_low_index, false, extra_forbid);
}

/**
 * 只从新段分配 单寄存器；extra_forbid 会被持久记入该 owner 的禁用表
 * @param owner 分组
 * @param count 数量
 * @param extra_forbid 额外禁用（会持久）
 * @return 租约
 */
std::vector<int> FRegAllocator::AllocSinglesTail(const std::string &owner,
                                                 int count,
                                                 const std::vector<int> &extra_forbid) {
    return AllocSinglesImpl(owner, count, true, true, extra_forbid);
}

/**
 * 单寄存器分配实现；将 extra_forbid 加入 owner 的持久禁用，再参与计算
 * @param owner 分组
 * @param count 数量
 * @param prefer_low_index 是否优先低位
 * @param tail_only 是否只用新段
 * @param extra_forbid 额外禁用（会持久）
 * @return 租约
 */
std::vector<int> FRegAllocator::AllocSinglesImpl(const std::string &owner,
                                                 int count,
                                                 bool prefer_low_index,
                                                 bool tail_only,
                                                 const std::vector<int> &extra_forbid) {

    FRegLease lease;
    lease.owner = owner;
    lease.tail_only = tail_only;
    const int locals = Locals();

    if (count <= 0 || locals <= 0) {
        LOGE("[AllocSingles] owner=%s cnt=%d locals=%d -> no-op", owner.c_str(), count, locals);
        return lease.singles;
    }

    // 1) 将本次 extra_forbid 记入 owner 的持久禁用
    auto &forbid_set = owner_forbid_singles_[owner];
    for (int v: extra_forbid) if (v >= 0 && v < locals) forbid_set.insert(v);

    // 2) 参与计算的“额外禁用” = owner 持久禁用 ∪ 本次 extra
    std::vector<int> extra(forbid_set.begin(), forbid_set.end());
    for (int v: extra_forbid) if (v >= 0 && v < locals) extra.push_back(v);
    sort_unique(extra);

    // 3) 打日志（单行）
    LOGD("[AllocSingles] owner=%s cnt=%d tail=%d prefLow=%d locals=%d lockBase=%d glob=%s usedS=%s usedW=%s forbid=%s",
         owner.c_str(), count, tail_only ? 1 : 0, prefer_low_index ? 1 : 0, locals, lock_old_base_,
         VEC_CSTR(to_sorted_vec(global_forbid_)),
         VEC_CSTR(to_sorted_vec(used_single_)),
         VEC_CSTR(to_sorted_vec(used_wide_start_)),
         VEC_CSTR(extra));

    auto used = MakeForbidMaskSingles(extra);

    auto try_scan = [&](int from, int to, int step) {
        for (int v = from; v != to && (int) lease.singles.size() < count; v += step) {
            if (v < 0 || v >= locals) continue;
            if (tail_only && v < lock_old_base_) continue;
            if (!used[(size_t) v]) {
                lease.singles.push_back(v);
                used[(size_t) v] = 1;
            }
        }
    };

    if (prefer_low_index) try_scan(0, locals, +1);
    else try_scan(locals - 1, -1, -1);

    // 占用登记 + 结果日志
    for (int v: lease.singles) {
        used_single_.insert(v);
        owner_singles_[owner].insert(v);
    }
    LOGD("[AllocSingles] owner=%s got=%s/%d", owner.c_str(), VEC_CSTR(lease.singles), count);

    return lease.singles;
}


/**
 * 按 owner 释放历史所有分配及其持久禁用
 * @param owner 分组
 * @return
 */
void FRegAllocator::ReleaseByOwner(const std::string &owner) {
    auto itS = owner_singles_.find(owner);
    if (itS != owner_singles_.end()) {
        for (int v: itS->second) used_single_.erase(v);
        owner_singles_.erase(itS);
    }
    auto itW = owner_wides_.find(owner);
    if (itW != owner_wides_.end()) {
        for (int v: itW->second) used_wide_start_.erase(v);
        owner_wides_.erase(itW);
    }
    owner_forbid_singles_.erase(owner);
    owner_forbid_wides_.erase(owner);
    LOGD("[ReleaseByOwner] owner=%s done", owner.c_str());
}

/**
 * 按租约释放（不动该 owner 的持久禁用）
 * @param lease 租约
 * @return
 */
void FRegAllocator::ReleaseLease(const FRegLease &lease) {
    for (int v: lease.singles) {
        used_single_.erase(v);
        auto it = owner_singles_.find(lease.owner);
        if (it != owner_singles_.end()) it->second.erase(v);
    }
    for (int v: lease.wides) {
        used_wide_start_.erase(v);
        auto it = owner_wides_.find(lease.owner);
        if (it != owner_wides_.end()) it->second.erase(v);
    }
    LOGD("[ReleaseLease] owner=%s singles=%s wides=%s", lease.owner.c_str(),
         VEC_CSTR(lease.singles), VEC_CSTR(lease.wides));
}

/**
 * 保存占用快照（含 owner 级持久禁用）
 * @return 快照
 */
FRegAllocator::Snapshot FRegAllocator::TakeSnapshot() const {
    Snapshot s;
    s.used_single = used_single_;
    s.used_wide_starts = used_wide_start_;
    s.owner_singles = owner_singles_;
    s.owner_wides = owner_wides_;
    s.owner_forbid_singles = owner_forbid_singles_;
    s.owner_forbid_wides = owner_forbid_wides_;
    LOGD("[TakeSnapshot] singles=%zu wides=%zu", s.used_single.size(), s.used_wide_starts.size());
    return s;
}

/**
 * 恢复占用快照（含 owner 级持久禁用）
 * @param s 快照
 * @return
 */
void FRegAllocator::RestoreSnapshot(const Snapshot &s) {
    used_single_ = s.used_single;
    used_wide_start_ = s.used_wide_starts;
    owner_singles_ = s.owner_singles;
    owner_wides_ = s.owner_wides;
    owner_forbid_singles_ = s.owner_forbid_singles;
    owner_forbid_wides_ = s.owner_forbid_wides;
    LOGD("[RestoreSnapshot] done");
}

/**
 * 清除“已占用”的单寄存器（不动锁定/全局禁用/owner 禁用）
 * @return
 */
void FRegAllocator::ClearUsedSingles() {
    used_single_.clear();
    owner_singles_.clear();
    LOGD("[ClearUsedSingles] done");
}

/**
 * 清除“已占用”的宽寄存器（不动锁定/全局禁用/owner 禁用）
 * @return
 */
void FRegAllocator::ClearUsedWides() {
    used_wide_start_.clear();
    owner_wides_.clear();
    LOGD("[ClearUsedWides] done");
}

/**
 * 清除所有“已占用”的寄存器（不动锁定/全局禁用/owner 禁用）
 * @return
 */
void FRegAllocator::ClearAllUsed() {
    ClearUsedSingles();
    ClearUsedWides();
    LOGD("[ClearAllUsed] done");
}
