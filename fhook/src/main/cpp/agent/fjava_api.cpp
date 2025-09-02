//
// Created by Administrator on 2025/8/28.
//
#include "log.h"

#include "dexter/slicer/fsmali_printer.h"
#include "dexter/slicer/fir.h"

#define TAG FEADRE_TAG("freplace_fun")

#include "fjava_api.h"

namespace fjava_api {

    /// 直接构造绕行方法
    lir::Method *get_detour_method(slicer::DetourHook *thiz,
                                   lir::CodeIr *code_ir) {

        ir::Builder builder(code_ir->dex_ir);
        const auto ir_method = code_ir->ir_method;

        // 就是拿原方法信息，主要是在非静态时，添加一个this 的参数类型（加一个参数）
        //        bool is_static = (ir_method->access_flags & dex::kAccStatic) != 0;
        bool is_static = thiz->orig_method_id_.is_static;


        // 原方法声明的返回类型
        auto orig_proto = ir_method->decl->prototype;
        auto orig_return_type = orig_proto->return_type;

        // 2. 构建绕行方法的参数列表（与原方法保持一致，包括非静态方法的this指针）
        std::vector<ir::Type *> param_types;
        if (!is_static) {
            // 非静态方法需要传递this指针作为第一个参数
            param_types.push_back(ir_method->decl->parent);
        }
        // 添加原方法的参数类型
        if (orig_proto->param_types != nullptr) {
            const auto &orig_params = orig_proto->param_types->types;
            // 尾部插入
            param_types.insert(param_types.end(), orig_params.begin(), orig_params.end());
        }

        // 3. 构建绕行方法的签名
        auto detour_proto = builder.GetProto(
                orig_return_type,
                builder.GetTypeList(param_types));

        // 4. 获取绕行方法的声明
        auto detour_decl = builder.GetMethodDecl(
                builder.GetAsciiString(thiz->detour_method_id_.method_name),
                detour_proto,
                builder.GetType(thiz->detour_method_id_.class_descriptor)
        );
        auto detour_method = code_ir->Alloc<lir::Method>(
                detour_decl,
                detour_decl->orig_index);
        return detour_method;
    }

    /// public static int d(String tag, String msg)
    lir::Method *get_log_method(lir::CodeIr *code_ir) {
        std::string name_class = "Landroid/util/Log;";
        std::string name_method = "d";
        std::vector<std::string> param_types = {
                "Ljava/lang/String;",
                "Ljava/lang/String;"
        };
        std::string return_type = "I";

        return fir::FIR::cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    lir::Method *
    get_HookBridge_replace_fun(lir::CodeIr *code_ir) {

        std::string name_class = "Lcom/zxc/jtik/HookBridge;";
        std::string name_method = "replace_fun";
        std::vector<std::string> param_types = {};
        std::string return_type = "V";

        return fir::FIR::cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    // ------------------------------------------

    /// public static Class<?> forName(String className)
    lir::Method *get_Class_forName(lir::CodeIr *code_ir) {

        std::string name_class = "Ljava/lang/Class;";
        std::string name_method = "forName";
        std::vector<std::string> param_types = {
                "Ljava/lang/String;",
        };
        std::string return_type = "Ljava/lang/Class;";

        return fir::FIR::cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// public static ClassLoader getSystemClassLoader()
    lir::Method *
    get_ClassLoader_getSystemClassLoader(lir::CodeIr *code_ir) {

        std::string name_class = "Ljava/lang/ClassLoader;";
        std::string name_method = "getSystemClassLoader";
        std::vector<std::string> param_types = {};
        std::string return_type = "Ljava/lang/ClassLoader;";

        return fir::FIR::cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// public Class<?> loadClass(String name) throws ClassNotFoundException
    lir::Method *
    get_classloader_loadClass(lir::CodeIr *code_ir) {

        std::string name_class = "Ljava/lang/ClassLoader;";
        std::string name_method = "loadClass";
        std::vector<std::string> param_types = {
                "Ljava/lang/String;",
        };
        std::string return_type = "Ljava/lang/Class;";

        return fir::FIR::cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// public static native Thread currentThread()
    lir::Method *
    get_Thread_currentThread(lir::CodeIr *code_ir) {

        std::string name_class = "Ljava/lang/Thread;";
        std::string name_method = "currentThread";
        std::vector<std::string> param_types = {};
        std::string return_type = "Ljava/lang/Thread;";

        return fir::FIR::cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// public ClassLoader getContextClassLoader()
    lir::Method *
    get_thread_getContextClassLoader(lir::CodeIr *code_ir) {

        std::string name_class = "Ljava/lang/Thread;";
        std::string name_method = "getContextClassLoader";
        std::vector<std::string> param_types = {};
        std::string return_type = "Ljava/lang/ClassLoader;";

        return fir::FIR::cre_method(code_ir, name_class, name_method, param_types, return_type);
    }


    /// public static Class<?> forName(String name, boolean initialize,ClassLoader loader)
    lir::Method *get_Class_forName4loader(lir::CodeIr *code_ir) {

        std::string name_class = "Ljava/lang/Class;";
        std::string name_method = "forName";
        std::vector<std::string> param_types = {
                "Ljava/lang/String;",
                "Z",
                "Ljava/lang/ClassLoader;",
        };
        std::string return_type = "Ljava/lang/Class;";

        return fir::FIR::cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// public Method getDeclaredMethod(String name, Class<?>... parameterTypes)
    lir::Method *get_class_getDeclaredMethod(lir::CodeIr *code_ir) {

        std::string name_class = "Ljava/lang/Class;";
        std::string name_method = "getDeclaredMethod";
        std::vector<std::string> param_types = {
                "Ljava/lang/String;",
                "[Ljava/lang/Class;"
        };
        std::string return_type = "Ljava/lang/reflect/Method;";

        return fir::FIR::cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// public native Object invoke(Object obj, Object... args)
    lir::Method *get_method_invoke(lir::CodeIr *code_ir) {

        std::string name_class = "Ljava/lang/reflect/Method;";
        std::string name_method = "invoke";
        std::vector<std::string> param_types = {
                "Ljava/lang/Object;",
                "[Ljava/lang/Object;"
        };
        std::string return_type = "Ljava/lang/Object;";

        return fir::FIR::cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    // ------------------------------------------

    /// public static Object[] onEnter(Object[] args)
    lir::Method *get_THook_onEnter(lir::CodeIr *code_ir) {

        std::string name_class = "Ltop/feadre/finject/ftest/THook;";  // 用/分隔，结尾有;
        std::string name_method = "onEnter";
        std::vector<std::string> param_types = {
                "[Ljava/lang/Object;", "J"
        };
        std::string return_type = "[Ljava/lang/Object;";

        return fir::FIR::cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    ///  public static Object onExit(Object ret)
    lir::Method *get_THook_onExit(lir::CodeIr *code_ir) {

        std::string name_class = "Ltop/feadre/finject/ftest/THook;";  // 用/分隔，结尾有;
        std::string name_method = "onExit";
        std::vector<std::string> param_types = {
                "Ljava/lang/Object;", "J"
        };
        std::string return_type = "Ljava/lang/Object;";

        return fir::FIR::cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

}
