
#include "../FGlobal_flib.h"

#define TAG FEADRE_TAG("freg_manager")

#include "freg_manager.h"
#include <algorithm>

// 基础健壮性检查
static inline void check_code_ir(const lir::CodeIr *code_ir) {
    SLICER_CHECK(code_ir && code_ir->ir_method && code_ir->ir_method->code);
}

// 把“本次不可用”的 v 列表转成遮罩（越界会自动忽略）
static std::vector<uint8_t> make_forbid_mask(const lir::CodeIr *code_ir,
                                             const std::vector<int> &forbidden_v) {
    check_code_ir(code_ir);
    const auto code = code_ir->ir_method->code;
    const int locals = static_cast<int>(code->registers - code->ins_count);
    std::vector<uint8_t> mask(std::max(0, locals), 0);

    for (int v: forbidden_v) {
        if (v >= 0 && v < locals) mask[static_cast<size_t>(v)] = 1;
    }
    return mask;
}

// ===== 基础查询 =====
int FRegManager::Locals(const lir::CodeIr *code_ir) {
    check_code_ir(code_ir);
    const auto code = code_ir->ir_method->code;
    return static_cast<int>(code->registers - code->ins_count);
}

dex::u2 FRegManager::TotalRegs(const lir::CodeIr *code_ir) {
    check_code_ir(code_ir);
    return code_ir->ir_method->code->registers;
}

dex::u2 FRegManager::InsCount(const lir::CodeIr *code_ir) {
    check_code_ir(code_ir);
    return code_ir->ir_method->code->ins_count;
}

// ===== 扩容 =====
dex::u2 FRegManager::ReqLocalsRegs4Need(lir::CodeIr *code_ir, dex::u2 need_locals) {
    check_code_ir(code_ir);
    auto *code = code_ir->ir_method->code;

    const dex::u2 orig_total = code->registers;
    const dex::u2 ins_size = code->ins_count;
    const dex::u2 orig_locals = static_cast<dex::u2>(orig_total - ins_size);

    if (orig_locals >= need_locals) {
        LOGI("[ReqLocalsRegs4Need] 无需扩容 总=%u, 参数=%u, 本地=%u (need=%u)",
             code->registers, ins_size, orig_locals, need_locals);
        return 0;  // 本次新增=0
    }

    const dex::u2 add = static_cast<dex::u2>(need_locals - orig_locals);
    code->registers = static_cast<dex::u2>(code->registers + add);

    LOGI("[ReqLocalsRegs4Need] 扩容：%u -> %u, 参数=%u, 本地=%u -> %u (+%u)",
         orig_total, code->registers, ins_size, orig_locals, need_locals, add);
    return add; // 返回本次新增的寄存器数量
}

/// ---------------------- 锁定 +N start ----------------------


// ===== 新增：在现有基础上 +n，并锁定旧寄存器只用新段 =====
int FRegManager::ReqLocalsRegs4Num(lir::CodeIr *code_ir, dex::u2 add) {
    check_code_ir(code_ir);
    auto *code = code_ir->ir_method->code;

    const dex::u2 orig_total = code->registers;
    const dex::u2 ins_size = code->ins_count;
    const dex::u2 orig_locals = static_cast<dex::u2>(orig_total - ins_size);

    if (add == 0) {
        LOGI("[ReqLocalsRegs4Num] +0，无需扩容，总=%u, 参数=%u, 本地=%u",
             orig_total, ins_size, orig_locals);
        return static_cast<int>(orig_locals);
    }

    code->registers = static_cast<dex::u2>(orig_total + add);
    const dex::u2 new_locals = static_cast<dex::u2>(code->registers - ins_size);

    LOGI("[ReqLocalsRegs4Num] 本地 +%u：总 %u -> %u, 参数=%u, 本地 %u -> %u (base=%u)",
         add, orig_total, code->registers, ins_size, orig_locals, new_locals, orig_locals);

    // 返回“新段起始 v 下标”，后续只从 [base, new_locals) 里分配
    return static_cast<int>(orig_locals);
}

/**
 * 锁定旧段
 * @param base
 * @return
 */
std::vector<int> FRegManager::LockOldRegs(int base) {
    if (base <= 0) return {};
    std::vector<int> v;
    v.reserve(static_cast<size_t>(base));
    for (int i = 0; i < base; ++i) v.push_back(i);
    return v;
}

/**
 * 重置“禁止”列表为 [0, base)]
 * @param forbidden_v
 * @param base
 */
void FRegManager::ResetForbid2Old(std::vector<int> &forbidden_v, int base) {
    forbidden_v.clear();
    if (base <= 0) return;
    forbidden_v.reserve(static_cast<size_t>(base));
    for (int i = 0; i < base; ++i) forbidden_v.push_back(i);
    LOGD("[ResetForbid2Old] base=%d, forbid=%s", base, VEC_CSTR(forbidden_v));
}


static inline std::vector<int> make_forbid_prefix_range(int upto_exclusive) {
    std::vector<int> f;
    if (upto_exclusive <= 0) return f;
    f.reserve(static_cast<size_t>(upto_exclusive));
    for (int i = 0; i < upto_exclusive; ++i) f.push_back(i);
    return f;
}

/**
 *
 * @param code_ir
 * @param base
 * @param count
 * @param text
 * @return
 */
std::vector<int> FRegManager::AllocVFromTail(lir::CodeIr *code_ir,
                                             int base, int count,
                                             const char *text) {
    // 锁死 [0, base) 的旧 v 槽，只从尾部分配；低位优先即可（因为 base 之后本身就是新段）
    auto forbid = make_forbid_prefix_range(base);
    return AllocV(code_ir, forbid, count, text, /*prefer_low_index=*/true);
}

std::vector<int> FRegManager::AllocWideFromTail(lir::CodeIr *code_ir,
                                                int base, int count,
                                                const char *text) {
    auto forbid = make_forbid_prefix_range(base);
    return AllocWide(code_ir, forbid, count, text, /*prefer_low_index=*/true);
}

/// ---------------------- 锁定 +N 完成 ----------------------



/// ---------------------- 锁定 +N 完成 ----------------------



// ===== 分配：普通寄存器 =====
/**
 * std::vector<int> forbid = {2, 3, 5};
 * auto vs = FRegManager::AllocV(code_ir, forbid, 3, true);
 * {0,1,4}
 * @param code_ir
 * @param forbidden_v
 * @param count
 * @param prefer_low_index
 * @return
 *   (int)vs.size() < count 判断为失败
 */
std::vector<int> FRegManager::AllocV(lir::CodeIr *code_ir,
                                     const std::vector<int> &forbidden_v,
                                     int count,
                                     const char *text,
                                     bool prefer_low_index) {

    check_code_ir(code_ir);
    std::vector<int> out;
    if (count <= 0) return out;

    const int locals = Locals(code_ir);
    if (locals <= 0) {
        LOGE("[AllocV] %s 本地寄存器为 0，无法分配", text);
        return out;
    }

    auto used = make_forbid_mask(code_ir, forbidden_v);

    auto try_scan = [&](int from, int to, int step) {
        for (int v = from; v != to && (int) out.size() < count; v += step) {
            if (v < 0 || v >= locals) continue;
            if (!used[(size_t) v]) {
                out.push_back(v);
                used[(size_t) v] = 1; // 立即占用，避免重复给相同的 v
            }
        }
    };

    if (prefer_low_index) try_scan(0, locals, +1);
    else try_scan(locals - 1, -1, -1);

    if ((int) out.size() < count) {
        LOGW("[AllocV] %s 仅分配到 %d/%d 个 (locals=%d), forbid=%s",
             text, (int) out.size(), count, locals, VEC_CSTR(forbidden_v));
    } else {
        LOGD("[AllocV] %s 成功分配 %d 个, starts=%s", text, count, VEC_CSTR(out));
    }
    return out;
}

std::vector<int> FRegManager::AllocV(lir::CodeIr *code_ir,
                                     const std::vector<int> &forbidden_v,
                                     int count,
                                     bool prefer_low_index) {
    return AllocV(code_ir, forbidden_v, count, "", prefer_low_index);
}


// ===== 分配：宽寄存器起点 (v, v+1) =====
std::vector<int> FRegManager::AllocWide(lir::CodeIr *code_ir,
                                        const std::vector<int> &forbidden_v,
                                        int count,
                                        const char *text,
                                        bool prefer_low_index) {
    check_code_ir(code_ir);
    std::vector<int> out;
    if (count <= 0) return out;

    const int locals = Locals(code_ir);
    if (locals <= 1) {
        LOGE("[AllocWide] %s 本地寄存器不足 2，无法分配宽寄存器对", text);
        return out;
    }

    auto used = make_forbid_mask(code_ir, forbidden_v);

    auto try_scan = [&](int from, int to, int step) {
        for (int v = from; v != to && (int) out.size() < count; v += step) {
            if (v < 0 || v + 1 >= locals) continue;
            if (!used[(size_t) v] && !used[(size_t) (v + 1)]) {
                out.push_back(v);
                used[(size_t) v] = 1;
                used[(size_t) (v + 1)] = 1; // 占两格
            }
        }
    };

    if (prefer_low_index) try_scan(0, locals - 1, +1);
    else try_scan(locals - 2, -1, -1);

    if ((int) out.size() < count) {
        LOGW("[AllocWide] %s 仅分配到 %d/%d 对 (locals=%d), forbid=%s",
             text, (int) out.size(), count, locals, VEC_CSTR(forbidden_v));
    } else {
        LOGD("[AllocWide] %s 成功分配 %d 对, starts=%s", text, count, VEC_CSTR(out));
    }
    return out;
}

std::vector<int> FRegManager::AllocWide(lir::CodeIr *code_ir,
                                        const std::vector<int> &forbidden_v,
                                        int count,
                                        bool prefer_low_index) {
    return AllocWide(code_ir, forbidden_v, count, "", prefer_low_index);
}
