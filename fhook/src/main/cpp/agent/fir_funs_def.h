//
// Created by Administrator on 2025/8/28.
//

#ifndef RIFT_FJAVA_API_H
#define RIFT_FJAVA_API_H


#include "../dexter/slicer/code_ir.h"

namespace fir_funs_def {

    lir::Method *cre_method(lir::CodeIr *code_ir,
                            const std::string &name_class,
                            const std::string &name_method,
                            const std::vector<std::string> &param_types,
                            const std::string &return_type);

    lir::Method *get_log_method(lir::CodeIr *code_ir);

    lir::Method *get_lookup_findStatic(lir::CodeIr *code_ir);

    lir::Method *get_methodHandle_invokeWithArguments(lir::CodeIr *code_ir);

    lir::Method *get_class_getDeclaredMethod(lir::CodeIr *code_ir);

    lir::Method *get_method_invoke(lir::CodeIr *code_ir);

    // ------------------------------------------

    lir::Method *get_Class_forName_1p(lir::CodeIr *code_ir);

    lir::Method *
    get_ClassLoader_getSystemClassLoader(lir::CodeIr *code_ir);

    lir::Method *
    get_classloader_loadClass(lir::CodeIr *code_ir);

    lir::Method *
    get_Thread_currentThread(lir::CodeIr *code_ir);

    lir::Method *
    get_thread_getContextClassLoader(lir::CodeIr *code_ir);

    lir::Method *get_Class_forName_3p(lir::CodeIr *code_ir);

    lir::Method *get_MethodHandles_publicLookup(lir::CodeIr *code_ir);

    lir::Method *get_MethodType_methodType_2p(lir::CodeIr *code_ir);

    lir::Method *get_THook_onEnter(lir::CodeIr *code_ir);

    lir::Method *get_THook_onExit(lir::CodeIr *code_ir);
}


#endif //RIFT_FJAVA_API_H
