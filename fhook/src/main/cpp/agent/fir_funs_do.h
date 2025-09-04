//
// Created by Administrator on 2025/9/2.
//

#ifndef FHOOK_FIR_FUNS_DO_H
#define FHOOK_FIR_FUNS_DO_H

#include "../dexter/slicer/code_ir.h"
#include "../dexter/slicer/dex_ir_builder.h"

namespace fir_funs_do {
    /** ----------------- 静态函数区 ------------------- */
    void do_ClassLoader_getSystemClassLoader(
            lir::Method *f_ClassLoader_getSystemClassLoader,
            lir::CodeIr *code_ir,
            dex::u2 reg_tmp_return,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void do_Thread_currentThread(
            lir::Method *f_Thread_currentThread,
            lir::CodeIr *code_ir,
            dex::u2 reg_tmp_return,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void do_Class_forName(
            lir::Method *f_Class_forName,
            lir::CodeIr *code_ir,
            dex::u2 reg1, dex::u2 reg_tmp_return,
            std::string &name_class,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point
    );

    bool do_THook_onExit(
            lir::Method *f_THook_onExit,
            lir::CodeIr *code_ir,
            int reg1_arg,  // onExit object 的参数寄存器（如 v4）
            int reg_tmp_return, // onExit 返回值 object 可以相同
            int reg2_tmp_long, // 宽寄存器
            uint64_t method_id, // uint64_t
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void do_Log_d(
            lir::Method *f_Log_d,
            lir::CodeIr *code_ir,
            dex::u2 reg1, dex::u2 reg2,
            std::string &tag,
            std::string &text,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point
    );


    void do_class_getDeclaredMethod(
            lir::Method *f_class_getDeclaredMethod,
            lir::CodeIr *code_ir,
            dex::u2 reg1, // class对象
            dex::u2 reg2, // 方法名
            dex::u2 reg3, // 方法参数
            dex::u2 reg_return,
            std::string &name_fun,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    bool do_THook_onEnter(
            lir::Method *f_THook_onEnter,
            lir::CodeIr *code_ir,
            dex::u2 reg1_arg, // Object[]
            dex::u2 reg2_tmp_long, // long 必须宽寄存器
            dex::u2 reg_return,
            uint64_t method_id, // uint64_t
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point
    );

    void do_HookBridge_replace_fun(
            lir::Method *f_HookBridge_replace_fun,
            lir::CodeIr *code_ir,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point
    );

    void do_Class_forName4loader(
            lir::Method *f_Class_forName4loader,
            lir::CodeIr *code_ir,
            dex::u2 reg1, dex::u2 reg2, dex::u2 reg3,
            dex::u2 reg_return,
            std::string &name_class,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void do_ClassLoader_loadClass(
            lir::Method *f_classloader_loadClass,
            lir::CodeIr *code_ir,
            dex::u2 reg1, dex::u2 reg2,
            dex::u2 reg_return,
            std::string &name_class,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    /** ----------------- 对象函数区 ------------------- */
    void do_classloader_loadClass(
            lir::Method *f_classloader_loadClass,
            lir::CodeIr *code_ir,
            dex::u2 reg1, dex::u2 reg2,
            dex::u2 reg_return,
            std::string name_class,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void do_thread_getContextClassLoader(
            lir::Method *f_thread_getContextClassLoader,
            lir::CodeIr *code_ir,
            dex::u2 reg1,
            dex::u2 reg_return,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void do_class_getDeclaredMethod(
            lir::Method *f_class_getDeclaredMethod,
            lir::CodeIr *code_ir,
            dex::u2 reg1, // class对象
            dex::u2 reg2, // 方法名
            dex::u2 reg3, // 方法参数
            dex::u2 reg_return,
            std::string &name_fun,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    bool do_apploader_static_fun(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp, dex::u2 reg2_tmp, dex::u2 reg3_tmp,//额外临时寄存器
            int reg_method_arg, // 可以为null 用于存储方法参数类型数组（Class[]）
            int reg_do_args,// 可以为null 原始参数数组（Object[]）
            dex::u2 reg_return, // 可重复
            std::string &name_class,
            std::string &name_fun,
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

    int cre_arr_do_args4onExit(
            lir::CodeIr *code_ir,
            int reg_return,         // 原方法返回值所在寄存器 清空则没有返回值
            bool is_wide_return,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void cre_arr_object0(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp_idx, // array_size 也是索引
            dex::u2 reg2_tmp_value, // value
            dex::u2 reg3_arr, // array 这个 object[] 对象
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void cre_arr_object1(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp_idx, // 索引寄存器（也临时承接 String/Long）
            dex::u2 reg2_value, // 需要打包的第一个参数     临时（用作 aput 索引=1 等）
            dex::u2 reg3_arr, // 外层 Object[] 数组寄存器（输出）
            uint64_t method_id, // 要写入的 methodId  打包的第二个参数
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void cre_arr_do_args4onEnter(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp, // array_size 也是索引
            dex::u2 reg2_tmp, // value  array 这个 object[object[]] 再包一层对象
            dex::u2 reg_arr, // array 这个 object[] 对象
            uint64_t method_id, // 要写入的 methodId
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);
};


#endif //FHOOK_FIR_FUNS_DO_H
