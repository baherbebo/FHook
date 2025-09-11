//
// Created by Administrator on 2025/9/10.
//

#ifndef FHOOKAGENT_FREG_ALLOCATOR_H
#define FHOOKAGENT_FREG_ALLOCATOR_H

#include "../dexter/slicer/instrumentation.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <algorithm>

namespace feadre {

    // 单次分配的租约：单寄存器与宽寄存器起点
    class FRegLease {
    public:
        std::vector<int> singles;   // 单寄存器 v
        std::vector<int> wides;     // 宽寄存器起点 v（代表对 (v, v+1)）
        std::string owner;          // 归属分组
        bool tail_only = false;     // 是否只从“新段”分配
    };

    class FRegAllocator {
    public:
        ~FRegAllocator() = default;

        explicit FRegAllocator(lir::CodeIr *code_ir);

        dex::u2 TotalRegs() const;

        dex::u2 InsCount() const;

        int Locals() const;

        dex::u2 EnsureLocals(dex::u2 need_locals);

        int GrowLocals(dex::u2 add, uint16_t num_22c);

        void LockOldBase(int base);

        // 全局禁用（对单/宽均生效）
        void AddGlobalForbid(const std::vector<int> &forbidden);


        std::vector<int> GetRegs22c(int count) const;

        // 任意可用段分配
        std::vector<int> AllocSingles(const std::string &owner,
                                      int count,
                                      bool prefer_low_index = true,
                                      const std::vector<int> &extra_forbid = {});

        // 只从新段分配
        std::vector<int> AllocSinglesTail(const std::string &owner,
                                          int count,
                                          const std::vector<int> &extra_forbid = {});

        std::vector<int> AllocWides(const std::string &owner,
                                    int count,
                                    bool prefer_low_index = true,
                                    const std::vector<int> &extra_forbid = {});

        std::vector<int> AllocWidesTail(const std::string &owner,
                                        int count,
                                        const std::vector<int> &extra_forbid = {});

        // 释放
        void ReleaseByOwner(const std::string &owner);

        void ReleaseLease(const FRegLease &lease);

        // 快照/回滚（仅占用，不含锁定/全局禁用）
        struct Snapshot {
            std::unordered_set<int> used_single;
            std::unordered_set<int> used_wide_starts;
            std::unordered_map<std::string, std::unordered_set<int>> owner_singles;
            std::unordered_map<std::string, std::unordered_set<int>> owner_wides;

            // 按 owner 的持久禁用（随 owner 生命周期）
            std::unordered_map<std::string, std::unordered_set<int>> owner_forbid_singles;
            std::unordered_map<std::string, std::unordered_set<int>> owner_forbid_wides;
        };

        Snapshot TakeSnapshot() const;

        void RestoreSnapshot(const Snapshot &s);

        // 快速清理（只清占用，不清锁定与禁用）
        void ClearUsedSingles();

        void ClearUsedWides();

        void ClearAllUsed();

    private:
        lir::CodeIr *code_ir_ = nullptr;

        int num_reg_non_param_orig_ = 0;

        std::unordered_set<int> regs_22c_;// 保留的22c寄存器（<16）

        // 状态
        int lock_old_base_ = 0;  // [0, lock_old_base_) 视为不可用（仅本分配器）

        // 全局禁用（所有 owner / 所有分配均生效）
        std::unordered_set<int> global_forbid_;

        // 全局占用：单寄存器 / 宽起点
        std::unordered_set<int> used_single_;
        std::unordered_set<int> used_wide_start_;

        // 按 owner 记录的“已占用”
        std::unordered_map<std::string, std::unordered_set<int>> owner_singles_;
        std::unordered_map<std::string, std::unordered_set<int>> owner_wides_;

        // 按 owner 的“持久禁用”（你要求的：extra_forbid 自动加入并持久，直到 ReleaseByOwner）
        std::unordered_map<std::string, std::unordered_set<int>> owner_forbid_singles_;
        std::unordered_map<std::string, std::unordered_set<int>> owner_forbid_wides_;

    private:
        static inline void CheckIr(const lir::CodeIr *ir);

        // forbid mask（1=不可用）
        std::vector<uint8_t> MakeForbidMaskSingles(const std::vector<int> &extra) const;

        std::vector<uint8_t> MakeForbidMaskWides(const std::vector<int> &extra) const;

        // 实际分配
        std::vector<int> AllocSinglesImpl(const std::string &owner,
                                          int count,
                                          bool prefer_low_index,
                                          bool tail_only,
                                          const std::vector<int> &extra_forbid);

        std::vector<int> AllocWidesImpl(const std::string &owner,
                                        int count,
                                        bool prefer_low_index,
                                        bool tail_only,
                                        const std::vector<int> &extra_forbid);
    };

} // namespace feadre

#endif //FHOOKAGENT_FREG_ALLOCATOR_H
