//
// Created by Administrator on 2025/9/2.
//

#ifndef FHOOK_FIR_TOOLS_H
#define FHOOK_FIR_TOOLS_H


#include "../dexter/slicer/code_ir.h"

namespace fir_tools {

    void cre_null_reg(lir::CodeIr *code_ir, int reg1,
                      slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void EmitConstToReg(lir::CodeIr *code_ir,
                        slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point,
                        dex::u4 reg_det,
                        dex::s4 value);

    void unbox_scalar_value(slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point,
                            lir::CodeIr *code_ir,
                            ir::Type *type,
                            dex::u4 reg_src,   // 存放 Object 的寄存器 (例如 tmp_obj_reg)
                            dex::u4 reg_dst);

    void box_scalar_value(slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point,
                          lir::CodeIr *code_ir,
                          ir::Type *type,
                          dex::u4 reg_src,
                          dex::u4 reg_dst);

    void cre_return_code(lir::CodeIr *code_ir,
                         ir::Type *return_type,
                         int reg_return,
                         slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void restore_reg_params4type(lir::CodeIr *code_ir,
                                 dex::u4 reg_args,            // enter 返回的 Object[] 寄存器 (e.g. v0)
                                 dex::u4 base_param_reg,     // 参数区起始寄存器 (num_reg_non_param)
                                 dex::u4 reg_tmp_obj,       // 临时寄存器存 aget-object 结果 (e.g. v1)
                                 dex::u4 reg_tmp_idx,// 临时寄存器做数组索引 (e.g. v2)
                                 slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point
    );

    void restore_reg_params4shift(lir::CodeIr *code_ir,
                                  slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point,
                                  dex::u4 shift);

    slicer::IntrusiveList<lir::Instruction>::Iterator
    find_return_code(lir::CodeIr *code_ir);

    slicer::IntrusiveList<lir::Instruction>::Iterator
    find_first_code(lir::CodeIr *code_ir);

    dex::u2 req_reg(lir::CodeIr *code_ir, dex::u2 regs_count);

    int find_return_register(lir::CodeIr *code_ir, bool *is_wide_out = nullptr);

    void clear_original_instructions(lir::CodeIr *code_ir);
};


#endif //FHOOK_FIR_TOOLS_H
