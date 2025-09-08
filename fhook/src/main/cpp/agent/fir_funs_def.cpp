//
// Created by Administrator on 2025/8/28.
//

#include "../FGlobal_flib.h"

#define TAG FEADRE_TAG("fir_funs_def")

#include "fir_funs_def.h"
#include "../dexter/slicer/dex_ir_builder.h"


namespace fir_funs_def {

    lir::Method *cre_method(
            lir::CodeIr *code_ir,
            const std::string &name_class,
            const std::string &name_method,
            const std::vector<std::string> &param_types,
            const std::string &return_type
    ) {
        ir::Builder builder(code_ir->dex_ir);

        // 1. 校验入参有效性（避免空指针或无效描述符导致崩溃）
        if (code_ir == nullptr || return_type.empty() || name_method.empty() ||
            name_class.empty()) {
            LOGE("[cre_method] 入参无效（code_ir=nullptr或关键字符串为空）");
            return nullptr;
        }

        // 2. 构建返回值类型（从描述符获取ir::Type）
        ir::Type *ir_return_type = builder.GetType(return_type.c_str());
        if (ir_return_type == nullptr) {
            LOGE("[cre_method] 无效的返回类型描述符：%s", return_type.c_str());
            return nullptr;
        }

        // 3. 构建参数类型列表（从描述符列表获取ir::TypeList）
        std::vector<ir::Type *> ir_param_types;
        for (const auto &param_desc: param_types) {
            ir::Type *param_type = builder.GetType(param_desc.c_str());
            if (param_type == nullptr) {
                LOGE("[cre_method] 无效的参数类型描述符：%s", param_desc.c_str());
                return nullptr;
            }
            ir_param_types.push_back(param_type);
        }
        ir::TypeList *ir_type_list = builder.GetTypeList(ir_param_types);  // 包装为TypeList

        // 4. 构建方法原型（Proto = 返回类型 + 参数类型列表）
        ir::Proto *ir_proto = builder.GetProto(
                ir_return_type, ir_type_list);
        if (ir_proto == nullptr) {
            LOGE("[cre_method] 构建方法原型失败（返回类型：%s，参数数：%zu）",
                 return_type.c_str(), param_types.size());
            return nullptr;
        }

        // 5. 构建方法声明（MethodDecl = 方法名 + 原型 + 所属类）
        ir::String *ir_method_name = builder.GetAsciiString(name_method.c_str());  // 方法名字符串
        ir::Type *ir_owner_class = builder.GetType(name_class.c_str());          // 所属类类型
        if (ir_method_name == nullptr || ir_owner_class == nullptr) {
            LOGE("[cre_method] 构建方法名或所属类失败（方法名：%s，类：%s）",
                 name_method.c_str(), name_class.c_str());
            return nullptr;
        }

        ir::MethodDecl *ir_method_decl = builder.GetMethodDecl(
                ir_method_name,    // 方法名
                ir_proto,          // 方法原型
                ir_owner_class     // 所属类
        );
        if (ir_method_decl == nullptr) {
            LOGE("[cre_method] 构建MethodDecl失败");
            return nullptr;
        }

        // 6. 分配lir::Method对象（传入MethodDecl和原始索引）
        // 注：decl->orig_index是Dex常量池中的原始索引，确保非kInvalidIndex
        if (ir_method_decl->orig_index == dex::kNoIndex) {
            LOGE("[cre_method] MethodDecl原始索引无效（可能未注册到常量池）");
            return nullptr;
        }

        lir::Method *lir_method = code_ir->Alloc<lir::Method>(
                ir_method_decl,
                ir_method_decl->orig_index
        );
        if (lir_method == nullptr) {
            LOGE("[cre_method] 分配lir::Method失败");
        }

        return lir_method;
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

        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// lookup.findStatic(Class refc, String name, MethodType type)
    /// (Ljava/lang/Class;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;
    lir::Method *get_lookup_findStatic(lir::CodeIr *code_ir) {
        std::string name_class = "Ljava/lang/invoke/MethodHandles$Lookup;";
        std::string name_method = "findStatic";
        std::vector<std::string> param_types = {
                "Ljava/lang/Class;",                 // refc
                "Ljava/lang/String;",                // name
                "Ljava/lang/invoke/MethodType;"     // type
        };
        std::string return_type = "Ljava/lang/invoke/MethodHandle;";
        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// MethodHandle.invokeWithArguments(Object[] arguments)
    /// ([Ljava/lang/Object;)Ljava/lang/Object;
    lir::Method *get_methodHandle_invokeWithArguments(lir::CodeIr *code_ir) {
        std::string name_class = "Ljava/lang/invoke/MethodHandle;";
        std::string name_method = "invokeWithArguments";
        std::vector<std::string> param_types = {
                "[Ljava/lang/Object;"          // arguments
        };
        std::string return_type = "Ljava/lang/Object;";
        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// public Method getDeclaredMethod(String name, Class<?>... parameterTypes)
    lir::Method *get_class_getDeclaredMethod(lir::CodeIr *code_ir) {

        std::string name_class = "Ljava/lang/Class;";
        std::string name_method = "getDeclaredMethod";
        std::vector<std::string> param_types = {
                "Ljava/lang/String;",
                "[Ljava/lang/Class;"  // 这个可以为 null
        };
        std::string return_type = "Ljava/lang/reflect/Method;";

        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// public java.lang.reflect.Field Class.getDeclaredField(String)
    lir::Method* get_class_getDeclaredField(lir::CodeIr* code_ir) {
        std::string name_class  = "Ljava/lang/Class;";
        std::string name_method = "getDeclaredField";
        std::vector<std::string> param_types = {
                "Ljava/lang/String;",
        };
        std::string return_type = "Ljava/lang/reflect/Field;";
        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// public void Field.setAccessible(boolean)   （声明在 AccessibleObject，但用 Field 调也OK）
    lir::Method* get_field_setAccessible(lir::CodeIr* code_ir) {
        std::string name_class  = "Ljava/lang/reflect/Field;";
        std::string name_method = "setAccessible";
        std::vector<std::string> param_types = { "Z" };
        std::string return_type = "V";
        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// public Object Field.get(Object)
    lir::Method* get_field_get(lir::CodeIr* code_ir) {
        std::string name_class  = "Ljava/lang/reflect/Field;";
        std::string name_method = "get";
        std::vector<std::string> param_types = { "Ljava/lang/Object;" };
        std::string return_type = "Ljava/lang/Object;";
        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// public Field Class.getField(String name)   // 只拿 public 字段
    lir::Method* get_class_getField(lir::CodeIr* code_ir) {
        std::string name_class  = "Ljava/lang/Class;";
        std::string name_method = "getField";
        std::vector<std::string> param_types = { "Ljava/lang/String;" };
        std::string return_type = "Ljava/lang/reflect/Field;";
        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }


    /// public int AtomicInteger.incrementAndGet()
    lir::Method* get_atomicInteger_incrementAndGet(lir::CodeIr* code_ir) {
        std::string name_class  = "Ljava/util/concurrent/atomic/AtomicInteger;";
        std::string name_method = "incrementAndGet";
        std::vector<std::string> param_types = {};
        std::string return_type = "I";
        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// public int AtomicInteger.decrementAndGet()
    lir::Method* get_atomicInteger_decrementAndGet(lir::CodeIr* code_ir) {
        std::string name_class  = "Ljava/util/concurrent/atomic/AtomicInteger;";
        std::string name_method = "decrementAndGet";
        std::vector<std::string> param_types = {};
        std::string return_type = "I";
        return cre_method(code_ir, name_class, name_method, param_types, return_type);
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

        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }


    // ------------------------------------------

    /// public static Class<?> forName(String className)
    lir::Method *get_Class_forName_1p(lir::CodeIr *code_ir) {

        std::string name_class = "Ljava/lang/Class;";
        std::string name_method = "forName";
        std::vector<std::string> param_types = {
                "Ljava/lang/String;",
        };
        std::string return_type = "Ljava/lang/Class;";

        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// public static ClassLoader getSystemClassLoader()
    lir::Method *
    get_ClassLoader_getSystemClassLoader(lir::CodeIr *code_ir) {

        std::string name_class = "Ljava/lang/ClassLoader;";
        std::string name_method = "getSystemClassLoader";
        std::vector<std::string> param_types = {};
        std::string return_type = "Ljava/lang/ClassLoader;";

        return cre_method(code_ir, name_class, name_method, param_types, return_type);
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

        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// public static native Thread currentThread()
    lir::Method *
    get_Thread_currentThread(lir::CodeIr *code_ir) {

        std::string name_class = "Ljava/lang/Thread;";
        std::string name_method = "currentThread";
        std::vector<std::string> param_types = {};
        std::string return_type = "Ljava/lang/Thread;";

        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// public ClassLoader getContextClassLoader()
    lir::Method *get_thread_getContextClassLoader(lir::CodeIr *code_ir) {

        std::string name_class = "Ljava/lang/Thread;";
        std::string name_method = "getContextClassLoader";
        std::vector<std::string> param_types = {};
        std::string return_type = "Ljava/lang/ClassLoader;";

        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// Class.forName(String, boolean, ClassLoader)
    /// (Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;
    lir::Method *get_Class_forName_3p(lir::CodeIr *code_ir) {

        std::string name_class = "Ljava/lang/Class;";
        std::string name_method = "forName";
        std::vector<std::string> param_types = {
                "Ljava/lang/String;",         // className
                "Z",                          // initialize
                "Ljava/lang/ClassLoader;"     // loader
        };
        std::string return_type = "Ljava/lang/Class;";

        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    /// MethodHandles.publicLookup()
    /// ()Ljava/lang/invoke/MethodHandles$Lookup;
    lir::Method *get_MethodHandles_publicLookup(lir::CodeIr *code_ir) {
        std::string name_class = "Ljava/lang/invoke/MethodHandles;";
        std::string name_method = "publicLookup";
        std::vector<std::string> param_types = {};
        std::string return_type = "Ljava/lang/invoke/MethodHandles$Lookup;";
        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    // 用于 MethodType.methodType(Class, Class[])
    lir::Method *get_MethodType_methodType_2p(lir::CodeIr *code_ir) {
        const std::string name_class = "Ljava/lang/invoke/MethodType;";
        const std::string name_method = "methodType";
        std::vector<std::string> param_types = {
                "Ljava/lang/Class;",     // rtype
                "[Ljava/lang/Class;"     // ptypes (数组)
        };
        const std::string return_type = "Ljava/lang/invoke/MethodType;";
        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }


    // ------------------------------------------
    /// public static Object[] onEnter4fhook(Object[] rawArgs, long methodId)
    lir::Method *get_THook_onEnter(lir::CodeIr *code_ir) {

        std::string name_class = "Ltop/feadre/fhook/FHook;";  // 用/分隔，结尾有;
        std::string name_method = "onEnter4fhook";
        std::vector<std::string> param_types = {
                "[Ljava/lang/Object;", "J"
        };
        std::string return_type = "[Ljava/lang/Object;";

        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

    ///  public static Object onExit4fhook(Object ret, long methodId)
    lir::Method *get_THook_onExit(lir::CodeIr *code_ir) {

        std::string name_class = "Ltop/feadre/fhook/FHook;";  // 用/分隔，结尾有;
        std::string name_method = "onExit4fhook";
        std::vector<std::string> param_types = {
                "Ljava/lang/Object;", "J"
        };
        std::string return_type = "Ljava/lang/Object;";

        return cre_method(code_ir, name_class, name_method, param_types, return_type);
    }

}
