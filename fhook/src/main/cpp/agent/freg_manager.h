#pragma once

#include <vector>
#include <cstdint>
#include "../dexter/slicer/code_ir.h"

#define CHECK_ALLOC_OR_RET(VAR, COUNT, RETVAL, /*fmt*/...)      \
    do {                                                        \
        if ((int)(VAR).size() < (COUNT)) {                      \
            LOGE(__VA_ARGS__);                                  \
            return (RETVAL);                                    \
        }                                                       \
    } while (0)

/**
 * FRegManager：极简寄存器管理工具（仅本地 v 区）
 * 只保留以下能力：
 *  - Locals      ：返回本地寄存器个数（v 区数量 = registers - ins_count）
 *  - TotalRegs   ：返回总寄存器个数（registers）
 *  - InsCount    ：返回参数寄存器个数（ins_count）
 *  - ReqLocalsRegs4Need：按“目标本地寄存器数”扩容，返回本次新增的寄存器数量
 *  - AllocV      ：给定“本次不能用”的 v 索引，分配 N 个可用普通寄存器（返回 v 索引）
 *  - AllocWide   ：给定“本次不能用”的 v 索引，分配 N 对可用宽寄存器（返回每对的起点 v）
 *
 * 约定：
 *  - 所有传入/返回的寄存器号，都是 **v 区本地寄存器索引**（0,1,2...）；
 *  - 宽寄存器返回值是“起点 v”，代表 (v, v+1) 这一对。
 */
class FRegManager {
public:


    // ===== 基础查询 =====
    /** @return 本地寄存器个数（= registers - ins_count） */
    static int Locals(const lir::CodeIr *code_ir);

    /** @return 总寄存器个数（registers） */
    static dex::u2 TotalRegs(const lir::CodeIr *code_ir);

    /** @return 参数寄存器个数（ins_count） */
    static dex::u2 InsCount(const lir::CodeIr *code_ir);

    // ===== 扩容 =====

    /**
     * 让“本地寄存器(v 区)”至少有 need_locals 个；若不足则增加 registers。
     * @param need_locals 目标 v 区个数（最终至少达到该值）
     * @return 本次“新增的寄存器数量”（0 表示未扩容）
     */
    static dex::u2 ReqLocalsRegs4Need(lir::CodeIr *code_ir, dex::u2 need_locals);


    /// ---------------------- 锁定 +N start ----------------------
    /**
     * 在当前本地寄存器(v 区)基础上增加 add 个
     * @param code_ir
     * @param add
     * @return 扩容前的 locals 值（也就是“新段起始 v 下标”），便于后续只用新段
     */
    static int ReqLocalsRegs4Num(lir::CodeIr *code_ir, dex::u2 add);

    // 只锁住旧槽：[0, base) —— 返回一份新的黑名单
    static std::vector<int> LockOldRegs(int base);

    static std::vector<int> AllocVFromTailWith(lir::CodeIr *code_ir,
                                        int num_reg_non_param_orig,
                                        const std::vector<int> &extra_forbid,
                                        int count,
                                        const char *text);

    static std::vector<int> AllocWideFromTailWith(lir::CodeIr *code_ir,
                                           int num_reg_non_param_orig,
                                           const std::vector<int> &extra_forbid,
                                           int count,
                                           const char *text);


    /// ---------------------- 锁定 +N 完成 ----------------------


    static std::vector<int> AllocV(lir::CodeIr *code_ir,
                                   const std::vector<int> &forbidden_v,
                                   int count,
                                   const char *text,
                                   bool prefer_low_index = true);


    static std::vector<int> AllocWide(lir::CodeIr *code_ir,
                                      const std::vector<int> &forbidden_v,
                                      int count,
                                      const char *text,
                                      bool prefer_low_index = true);
    // ===== 分配 =====
    /**
     * 分配 count 个“普通寄存器”(v 索引)；低位优先（可通过 prefer_low_index 控制）。
     * 仅基于“本次不能用”的黑名单进行避让；不会改动 registers。
     *
     * @param forbidden_v 本次不可使用的 v 索引列表（会自动忽略 <0 / >=locals 的项）
     * @param count       期望分配的数量
     * @param prefer_low_index true=低位优先；false=高位优先
     * @return 实际分配到的 v 索引列表；数量可能小于 count（不够用时会 LOGW）
     */
    static std::vector<int> AllocV(lir::CodeIr *code_ir,
                                   const std::vector<int> &forbidden_v,
                                   int count,
                                   bool prefer_low_index = true);


    /**
     * 分配 count 个“宽寄存器起点”(v 索引，代表 (v, v+1) 对)；低位优先（可控制）。
     * 同样仅基于“本次不能用”的黑名单进行避让。
     *
     * @param forbidden_v 本次不可使用的 v 索引列表（若包含 v，则 (v, v+1) 不可用）
     * @param count       期望分配的“宽对”数量
     * @param prefer_low_index true=低位优先；false=高位优先
     * @return 实际分配到的“宽起点 v”列表；数量可能小于 count（不够用时会 LOGW）
     */
    static std::vector<int> AllocWide(lir::CodeIr *code_ir,
                                      const std::vector<int> &forbidden_v,
                                      int count,
                                      bool prefer_low_index = true);

};