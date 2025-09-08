//
// Created by Administrator on 2025/9/2.
//

#ifndef FHOOK_FIR_FUNS_DO_H
#define FHOOK_FIR_FUNS_DO_H

#include "../dexter/slicer/code_ir.h"
#include "../dexter/slicer/dex_ir_builder.h"

namespace fir_funs_do {
    /** ----------------- 静态函数区 ------------------- */
    void do_static_0p(lir::Method *method,
                      lir::CodeIr *code_ir,
                      dex::u2 reg_return,
                      slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);


    void do_static_1p(lir::Method *method,
                      lir::CodeIr *code_ir,
                      dex::u2 reg1_tmp,
                      dex::u2 reg_return,
                      slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);


    void do_Class_forName_1p(
            lir::Method *f_Class_forName,
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp,
            dex::u2 reg_return,
            std::string &name_class,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);


    void do_Log_d(
            lir::Method *f_Log_d,
            lir::CodeIr *code_ir,
            dex::u2 reg1, dex::u2 reg2,
            std::string &tag,
            std::string &text,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);


    void do_MethodType_methodType_2p(
            lir::Method *f_MethodType_methodType,
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp,
            dex::u2 reg_method_args, // ptypes = new Class[2]; ptypes[0]=Object[].class; ptypes[1]=Long.TYPE;
            dex::u2 reg_return, // 可以重复
            std::string &rtype_name,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);


    void do_class_getDeclaredMethod(
            lir::Method *f_class_getDeclaredMethod,
            lir::CodeIr *code_ir,
            dex::u2 reg_class, // class对象
            dex::u2 reg1_tmp, // 方法名
            dex::u2 reg_class_arr, // 方法参数
            dex::u2 reg_return,
            std::string &name_fun,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);


    bool do_frame_hfun(
            lir::Method *f_THook_onEnter,
            lir::CodeIr *code_ir,
            dex::u2 reg_arg, // Object[]
            dex::u2 reg_tmp_long, // long 必须宽寄存器
            dex::u2 reg_return,
            uint64_t method_id, // uint64_t
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point
    );



    void do_Class_forName_3p(
            lir::Method *f_Class_forName4loader,
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp, dex::u2 reg2_tmp, dex::u2 reg3_classloader,
            dex::u2 reg_return,
            std::string &name_class,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);


    /** ----------------- 对象函数区 ------------------- */
    void do_classloader_loadClass(
            lir::Method *f_classloader_loadClass,
            lir::CodeIr *code_ir,
            dex::u2 reg_class, dex::u2 reg2_tmp,
            dex::u2 reg_return,
            std::string &name_class,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void do_thread_getContextClassLoader(
            lir::Method *f_thread_getContextClassLoader,
            lir::CodeIr *code_ir,
            dex::u2 reg_thread,
            dex::u2 reg_return,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void do_method_invoke(lir::Method *f_method_invoke,
                          lir::CodeIr *code_ir,
                          dex::u2 reg_method, // method 对象
                          dex::u2 reg_obj, // 实例对象 静态函数为 null
                          dex::u2 reg_do_args, // 执行参数
                          dex::u2 reg_return, // 可重复
                          slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void do_methodHandle_invokeWithArguments(
            lir::Method *f_methodHandle_invokeWithArguments,
            lir::CodeIr *code_ir,
            dex::u2 reg_mh, // classloader 对象
            dex::u2 reg_do_args, // Object[]
            dex::u2 reg_return, // 可重复
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    void do_lookup_findStatic(
            lir::Method *f_lookup_findStatic,
            lir::CodeIr *code_ir,
            dex::u2 reg_lookup,    // ← Lookup 实例
            dex::u2 reg_class, // 放方法名 String
            dex::u2 reg_name,  // name_class
            dex::u2 reg_mt, // MethodType
            dex::u2 reg_return, // 可以相同 返回 MethodHandle
            std::string &name_fun,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);


    bool do_get_java_method(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp,
            dex::u2 reg2_tmp,
            dex::u2 reg3_tmp,
            dex::u2 reg_clazz,
            dex::u2 reg_return,
            std::string &name_fun,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

    /** ----------------- 参数区 ------------------- */

    void cre_arr_do_arg_2p(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp_idx, // 索引寄存器（也临时承接 String/Long）
            dex::u2 reg2_value, // 需要打包的第一个参数     临时（用作 aput 索引=1 等）
            dex::u2 reg3_arr, // 外层 Object[] 数组寄存器（输出）
            uint64_t method_id, // 要写入的 methodId  打包的第二个参数
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point);

};


#endif //FHOOK_FIR_FUNS_DO_H
