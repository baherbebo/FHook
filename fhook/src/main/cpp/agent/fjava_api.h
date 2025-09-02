//
// Created by Administrator on 2025/8/28.
//

#ifndef RIFT_FJAVA_API_H
#define RIFT_FJAVA_API_H


namespace fjava_api {
    lir::Method *get_detour_method(slicer::DetourHook *thiz,
                                   lir::CodeIr *code_ir);

    lir::Method *get_log_method(lir::CodeIr *code_ir);

    lir::Method *
    get_HookBridge_replace_fun(lir::CodeIr *code_ir);

    lir::Method *get_Class_forName(lir::CodeIr *code_ir);

    lir::Method *
    get_ClassLoader_getSystemClassLoader(lir::CodeIr *code_ir);

    lir::Method *
    get_classloader_loadClass(lir::CodeIr *code_ir);

    lir::Method *
    get_Thread_currentThread(lir::CodeIr *code_ir);

    lir::Method *
    get_thread_getContextClassLoader(lir::CodeIr *code_ir);

    lir::Method *get_Class_forName4loader(lir::CodeIr *code_ir);


    lir::Method *get_class_getDeclaredMethod(lir::CodeIr *code_ir);

    lir::Method *get_method_invoke(lir::CodeIr *code_ir);

    lir::Method *get_THook_onEnter(lir::CodeIr *code_ir);

    lir::Method *get_THook_onExit(lir::CodeIr *code_ir);
}


#endif //RIFT_FJAVA_API_H
