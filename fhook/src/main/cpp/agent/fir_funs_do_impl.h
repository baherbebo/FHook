//
// Created by Administrator on 2025/9/8.
//

#ifndef FHOOK_FIR_FUNS_DO_IMPL_H
#define FHOOK_FIR_FUNS_DO_IMPL_H

#include "../dexter/slicer/code_ir.h"
#include "../dexter/slicer/dex_ir_builder.h"

namespace fir_impl {
    bool do_sysloader_hook_funs_A(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp, dex::u2 reg2_tmp, dex::u2 reg3_tmp,//额外临时寄存器
            int reg_method_args, // 可以为null 用于存储方法参数类型数组（Class[]）
            int reg_do_args,// 可以为null 原始参数数组（Object[]）
            dex::u2 reg_return, // 可重复
            std::string &name_class,
            std::string &name_fun,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    bool do_sysloader_hook_funs_B_Exit(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp, dex::u2 reg2_tmp, dex::u2 reg3_tmp, dex::u2 reg4_tmp,
            int reg_do_args,// Object[Object[arg0,arg1...],Long]
            dex::u2 reg_return, // 可重复
            std::string &name_class,
            std::string &name_fun,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    bool do_sysloader_hook_funs_B_Enter(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp, dex::u2 reg2_tmp, dex::u2 reg3_tmp, dex::u2 reg4_tmp,
            int reg_do_args,// Object[Object[arg0,arg1...],Long]
            dex::u2 reg_return, // 可重复
            std::string &name_class,
            std::string &name_fun,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    bool do_sysloader_hook_funs_C(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp, dex::u2 reg2_tmp, dex::u2 reg3_tmp, dex::u2 reg4_tmp,
            int reg_do_args,                 // Object[]{ rawArgs, Long(methodId) }
            dex::u2 reg_return,              // out: Object[] (newArgs)
            std::string &name_class,         // "top.feadre.fhook.FHook"
            std::string &name_fun,           //  这里分区是走哪一个
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);


    /** ----------------- 参数区 ------------------- */
    void cre_arr_class_args4onEnter(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp, // array_size 也是索引
            dex::u2 reg2_tmp, // value
            dex::u2 reg3_arr, // array 这个 object[] 对象
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void cre_arr_class_args4onExit(
            lir::CodeIr *code_ir,
            dex::u2 reg1, // index 与 array_size 复用
            dex::u2 reg2, // value 临时（存放 Class 对象）
            dex::u2 reg3_arr, // array 目标寄存器（Class[]）
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void cre_arr_do_args4onEnter(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp_idx, // 这里避开了宽 array_size 也是索引
            dex::u2 reg2_tmp_value, // value
            dex::u2 reg3_arr, // array 这个 object[] 对象
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    bool cre_arr_do_args4onExit(
            lir::CodeIr *code_ir,
            int reg_return_orig,         // 原方法返回值所在寄存器 传入 清空则没有返回值
            dex::u2 reg_do_arg,  // 这个是返回值 必需要一个正常的寄存器，可以相等，需要外面判断完成是不是宽
            bool is_wide_return,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);
}

#endif //FHOOK_FIR_FUNS_DO_IMPL_H
