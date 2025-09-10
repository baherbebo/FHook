//
// Created by Administrator on 2025/9/1.
//

#include "../FGlobal_flib.h"

#define TAG FEADRE_TAG("fhook_agent")

#include <string>

#include "fhook_agent.h"
#include "../tools/fsys.h"
#include "transform.h"
#include "../agent/transform.h"
#include "../dexter/slicer/reader.h"
#include "../dexter/slicer/writer.h"

static jvmtiEnv *fAgentJvmtiEnv = nullptr; // 全局JVMTI环境
static JavaVM *f_vm;

static std::map<std::string, std::unique_ptr<deploy::Transform>> fClassTransforms;// 当前已hook的所有配置
thread_local deploy::Transform *current_transform = nullptr; // 当前hook配置，用于缓存和隔离其它类


/**
 * 命中 A组 → 返回 -1（调用方直接 return -1）
 * 命中 B/C组 → 把 isRunOrigFun 设为 true 并返回 1
 * 其它 → 返回 0（不处理）
 */
static int is_frame_methods(const std::string &targetClassName,
                            const std::string &targetMethodName,
                            const std::string &targetMethodSignature,
                            jboolean &isRunOrigFun) {
    // ---- A组：禁止一切（直接拦截）----
    if (SigEq(targetClassName, "Ljava/lang/Thread;")
        && targetMethodName == "currentThread"
        && SigEq(targetMethodSignature, "()Ljava/lang/Thread;")) {
        return -1;
    }
    if (SigEq(targetClassName, "Ljava/lang/Thread;")
        && targetMethodName == "getContextClassLoader"
        && SigEq(targetMethodSignature, "()Ljava/lang/ClassLoader;")) {
        return -1;
    }

    // ---- B/C组：允许 hook，但强制保留原实现（不可清空）----
    // ClassLoader.loadClass(String)
    if (SigEq(targetClassName, "Ljava/lang/ClassLoader;")
        && targetMethodName == "loadClass"
        && SigEq(targetMethodSignature, "(Ljava/lang/String;)Ljava/lang/Class;")) {
//        isRunOrigFun = JNI_TRUE;
        return 1;
    }
    // Class.getDeclaredMethod(String, Class[])
    if (SigEq(targetClassName, "Ljava/lang/Class;")
        && targetMethodName == "getDeclaredMethod"
        && SigEq(targetMethodSignature,
                 "(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;")) {
//        isRunOrigFun = JNI_TRUE;
        return 1;
    }

    // Method.invoke(Object, Object[])  这个是native 方法  其它已处理
//    if (SigEq(targetClassName, "Ljava/lang/reflect/Method;")
//        && targetMethodName == "invoke"
//        &&
//        SigEq(targetMethodSignature, "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;")) {
////        isRunOrigFun = JNI_TRUE;
//        return 1;
//    }

    // Class.forName(String, boolean, ClassLoader)
    if (SigEq(targetClassName, "Ljava/lang/Class;")
        && targetMethodName == "forName"
        && SigEq(targetMethodSignature,
                 "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;")) {
//        isRunOrigFun = JNI_TRUE;
        return 1;
    }

    return 0; // 不命中，啥也不做
}


/**
 * loader 通过 loader 拿到类加载器的类型显示
 * @param jni
 * @param loader
 * @param name
 * @return
 */
static bool find_app_class_loader(JNIEnv *jni, jobject loader, const char *name) {
    if (loader == nullptr) {
        // Bootstrap（BootClassLoader）——系统/核心类
        LOGW("[find_app_class_loader] Class %s is loaded by Bootstrap ClassLoader",
             name ? name : "<null>");
        return false;
    }

    jclass loaderClass = jni->GetObjectClass(loader);        // 这是个 java.lang.Class 实例（loader 的类对象）
    if (!loaderClass) {
        if (jni->ExceptionCheck()) jni->ExceptionClear();
        LOGW("[find_app_class_loader] %s by <loader ?>(GetObjectClass failed)",
             name ? name : "<null>");
        return false;
    }

    // 常见 App 侧 Loader
    jclass pathClassLoader = jni->FindClass("dalvik/system/PathClassLoader");
    if (jni->ExceptionCheck()) jni->ExceptionClear();
    jclass dexClassLoader = jni->FindClass("dalvik/system/DexClassLoader");
    if (jni->ExceptionCheck()) jni->ExceptionClear();
    jclass inMemClassLoader = jni->FindClass("dalvik/system/InMemoryDexClassLoader");
    if (jni->ExceptionCheck()) jni->ExceptionClear(); // 26+
    jclass delegLastClassLoader = jni->FindClass("dalvik/system/DelegateLastClassLoader");
    if (jni->ExceptionCheck()) jni->ExceptionClear(); // 27+
    // 兜底：广义“应用侧”都继承它
    jclass baseDexClassLoader = jni->FindClass("dalvik/system/BaseDexClassLoader");
    if (jni->ExceptionCheck()) jni->ExceptionClear();

    // 打印真实 loader 类名：对 Class 实例调用 Class.getName()
    jclass clsClass = jni->FindClass("java/lang/Class");
    if (jni->ExceptionCheck()) jni->ExceptionClear();
    jmethodID midGetName = clsClass ? jni->GetMethodID(clsClass, "getName", "()Ljava/lang/String;")
                                    : nullptr;
    if (jni->ExceptionCheck()) {
        jni->ExceptionClear();
        midGetName = nullptr;
    }

    auto print_with_name = [&](const char *tag) {
        if (midGetName) {
            // 注意：这里的 receiver 必须是“类对象”，即 loaderClass，而不是 loader 实例
            jstring jn = (jstring) jni->CallObjectMethod(loaderClass, midGetName);
            if (jni->ExceptionCheck()) {
                jni->ExceptionClear();
                jn = nullptr;
            }
            const char *s = jn ? jni->GetStringUTFChars(jn, nullptr) : "<?>"; // 下面会释放
            LOGW("[find_app_class_loader] %s by %s (%s)", name ? name : "<null>", tag, s);
            if (jn) {
                jni->ReleaseStringUTFChars(jn, s);
                jni->DeleteLocalRef(jn);
            }
        } else {
            LOGW("[find_app_class_loader] %s by %s", name ? name : "<null>", tag);
        }
    };

    bool is_app = false;
    if (pathClassLoader && jni->IsInstanceOf(loader, pathClassLoader)) {
        print_with_name("PathClassLoader");
        is_app = true;
    } else if (dexClassLoader && jni->IsInstanceOf(loader, dexClassLoader)) {
        print_with_name("DexClassLoader");
        is_app = true;
    } else if (inMemClassLoader && jni->IsInstanceOf(loader, inMemClassLoader)) {
        print_with_name("InMemoryDexClassLoader");
        is_app = true;
    } else if (delegLastClassLoader && jni->IsInstanceOf(loader, delegLastClassLoader)) {
        print_with_name("DelegateLastClassLoader");
        is_app = true;
    } else if (baseDexClassLoader && jni->IsInstanceOf(loader, baseDexClassLoader)) {
        // 任何继承 BaseDexClassLoader 的自定义 Loader（插件/热修复等）
        print_with_name("BaseDexClassLoader<custom>");
        is_app = true;
    } else {
        // 系统侧/厂商自定义非 BaseDex 的 Loader
        print_with_name("Non-App/ClassLoader");
        is_app = false;
    }

    // 回收本地引用
    if (clsClass) jni->DeleteLocalRef(clsClass);
    if (pathClassLoader) jni->DeleteLocalRef(pathClassLoader);
    if (dexClassLoader) jni->DeleteLocalRef(dexClassLoader);
    if (inMemClassLoader) jni->DeleteLocalRef(inMemClassLoader);
    if (delegLastClassLoader)jni->DeleteLocalRef(delegLastClassLoader);
    if (baseDexClassLoader) jni->DeleteLocalRef(baseDexClassLoader);
    jni->DeleteLocalRef(loaderClass);

    return is_app;
}


static int SetNeedCapabilities(jvmtiEnv *jvmti) {
    jvmtiCapabilities caps = {0};
    jvmtiError error;
    error = jvmti->GetPotentialCapabilities(&caps);

    LOGD("[SetNeedCapabilities] %d, %d, %d, %d",
         caps.can_redefine_classes,
         caps.can_retransform_any_class,
         caps.can_set_native_method_prefix,
         error);

    jvmtiCapabilities newCaps = {0};
    newCaps.can_retransform_classes = 1;
    if (caps.can_set_native_method_prefix) {
        newCaps.can_set_native_method_prefix = 1;

    }

    if (caps.can_tag_objects) {
        newCaps.can_tag_objects = 1; // 开启类实例侦测
    }

    jvmtiError addErr = jvmti->AddCapabilities(&newCaps);
    LOGD("[AddCapabilities] err=%d (0=OK, 99=MUST_POSSESS_CAPABILITY)", addErr);
    return addErr;
}


static inline bool IsBlacklistedClass(const char *internal_name) {
    // 内部名，如 "java/lang/invoke/MethodHandles"
    if (!internal_name) return false;
    if (StartsWith(internal_name, "java/lang/invoke/")) return true;
    if (StartsWith(internal_name, "java/lang/reflect/")) return true;
    if (strcmp(internal_name, "java/lang/Class") == 0) return true;
    if (strcmp(internal_name, "java/lang/ClassLoader") == 0) return true;
    return false;
}


/**
 * 这个调用  transform.cpp  current_transform->Apply(dex_ir);
 */
extern "C" void JNICALL HookClassFileLoadHook(
        jvmtiEnv *jvmti,
        JNIEnv *jni,
        jclass class_being_redefined, // 首次加载的类为 nullptr,已加载类的重转换
        jobject loader, // 该类的类加载器对象
        const char *name,  // 内部名：top/feadre/fhook/THook 可能为 null
        jobject protection_domain,
        jint class_data_len,
        const unsigned char *class_data, // 指向原始类字节码的指针
        jint *new_class_data_len,
        unsigned char **new_class_data) {

    // 没有配置直接返回
    if (current_transform == nullptr) {
//        LOGD("[HookClassFileLoadHook] 当前没有hook配置 类名= %s", name)
        return;
    }

    // “全局护栏”，设置黑名单，也不会改这些核心包
    if (g_is_safe_mode && (name == nullptr || IsBlacklistedClass(name))) {
        LOGW("[HookClassFileLoadHook] skip blacklisted class: %s", name);
        return;
    }

    const char *internal_name = name ? name : "";
    const std::string want_internal = current_transform->GetClassName();

    // 有hook配置后 加载的类，不匹配不处理
    if (internal_name[0] == '\0') {
        // 不知道类名，用下面的“单类 dex 头里类型 id”兜底再判断（见第4点）
        // 暂时先别 return，继续读 header 再核对
    } else if (want_internal != internal_name) {
        return;
    }

    LOGI("[HookClassFileLoadHook] 配置类名 %s ", name)

    // 这里确定是要改hook配置  --------

    /** ------------- 反射查看类加载器 的类型 ------------- */
    current_transform->set_app_loader(find_app_class_loader(jni, loader, name));

    std::string descriptor = current_transform->GetJniClassName(); // "Ltop/feadre/fhook/THook;

    if (class_data_len < 8 || memcmp(class_data, "dex\n", 4) != 0) {
        LOGE("[HookClassFileLoadHook] Not a DEX image (len=%d, name=%s)", class_data_len,
             internal_name);
        return;
    }

    // 定位查找类索引
    dex::Reader reader(class_data, class_data_len);
    auto class_index = reader.FindClassIndex(descriptor.c_str());
    if (class_index == dex::kNoIndex) {
        LOGE("[HookClassFileLoadHook] %s reader 定位失败", descriptor.c_str())
        return;
    }

    // 类的原始字节码解析为 IR（中间表示） 包含类的方法、字段、字节码指令等
    reader.CreateClassIr(class_index);
    auto dex_ir = reader.GetIr();

    /**这里执行 Transform::Apply 方法修改字节码 *****************/
    current_transform->Apply(dex_ir);

    // 生成新的文件
    size_t new_image_size = 0;
    dex::u1 *new_image = nullptr;
    dex::Writer writer(dex_ir); // 将修改后的 IR 重新编码为 Dex 字节码

    JvmtiAllocator allocator(jvmti); // 基于 JVMTI 接口分配内存
    new_image = writer.CreateImage(&allocator, &new_image_size);

#ifdef DEBUG_DEX_FILE
    // 将修改后的 Dex 保存到 /sdcard/Download/dex_debug.dex
    FILE* out_debug_check_file = fopen("/sdcard/Download/dex_debug.dex", "w");
        if(out_debug_check_file == nullptr) {
            ALOGI("out_debug_check_file open fail");
        } else {
            int write_cnt = fwrite(new_image, 1, new_image_size, out_debug_check_file);
            ALOGI("DexBuild wirte out_debug_check_file %d for %zu", write_cnt, new_image_size);
            fclose(out_debug_check_file);
        }
#endif

    LOGD("[HookClassFileLoadHook] 修改字节码完成 长度变化 %d -> %zu", class_data_len,
         new_image_size)

    *new_class_data_len = new_image_size; // 修改后的长度
    *new_class_data = new_image; // 修改后的字节码（new_image

}

/**
 * 当代理加载类文件时触发的事件   sAgentJvmtiEnv->RetransformClasses(1, &nativeClass) 重载回调
 */


/**
 * 对应  dcHook 通过C++ 符号反射回调
 */
extern "C" JNIEXPORT jlong JNICALL agent_do_transform(
        JNIEnv *env,
        jclass clazz,
        jobject jMethodObj,
        jboolean isHEnter,
        jboolean isHExit,
        jboolean isRunOrigFun) {

    LOGD("[agent_do_transform] start ...")

    if (fAgentJvmtiEnv == nullptr || !jMethodObj) {
        LOGE("[agent_do_transform] fAgentJvmtiEnv or jMethodObj is null")
        return -1;
    }

    // 对象 ->  方法ID
    jmethodID methodID4jmethodID = env->FromReflectedMethod(jMethodObj);
    if (!methodID4jmethodID) {
        LOGE("[agent_do_transform] method 方法ID引用失败");
        return -1;
    }

    /** ---------------------- 拿到方法信息 开始 ----------------------- */
    long methodId4long = reinterpret_cast<long>(methodID4jmethodID);
    long methodId4jlong = reinterpret_cast<jlong>(methodID4jmethodID);

    std::string targetClassName, targetMethodName, targetMethodSignature;

    // 创建三个对象 自动回收
    LocalJvmCharPtr methodName(fAgentJvmtiEnv),
            signature(fAgentJvmtiEnv),
            generic(fAgentJvmtiEnv);

    if (!fsys::CheckJvmti(
            fAgentJvmtiEnv->GetMethodName(methodID4jmethodID,
                                          methodName.getPtr(),
                                          signature.getPtr(),
                                          generic.getPtr()),
            "[agent_do_transform] get method signature fail")) {
        return -1;
    }

    targetMethodName = methodName.getValue();
    targetMethodSignature = signature.getValue();

    // 获取方法信息
    bool isStatic;
    int modifiers; // 是否为静态方法 public、static、final 等，以整数形式表示
    if (!fsys::CheckJvmti(
            fAgentJvmtiEnv->GetMethodModifiers(
                    methodID4jmethodID, &modifiers),
            "[agent_do_transform]  native class modifiers fail")) {
        return -1;
    }
    isStatic = (modifiers & fsys::kAccStatic) != 0; // 0x0008 是否为静态方法

    jclass nativeClass = nullptr;
    if (!fsys::CheckJvmti(fAgentJvmtiEnv->GetMethodDeclaringClass(
                                  methodID4jmethodID, &nativeClass),
                          "[agent_do_transform] get native class fail")) {
        return -1;
    }

    LocalJvmCharPtr jniClassName(fAgentJvmtiEnv), jniClassGeneric(fAgentJvmtiEnv);
    if (!fsys::CheckJvmti(fAgentJvmtiEnv->GetClassSignature(
                                  nativeClass,
                                  jniClassName.getPtr(),
                                  jniClassGeneric.getPtr()),
                          "[agent_do_transform] get native class name fail")) {
        if (nativeClass != nullptr) {
            env->DeleteLocalRef(nativeClass);
        }
        return -1;
    }
    targetClassName = jniClassName.getValue();
    /** ---------------------- 拿到方法信息 结束 ----------------------- */

    // 框架方法的强制处理
    int sg = is_frame_methods(targetClassName, targetMethodName, targetMethodSignature,
                              isRunOrigFun);

    if (sg == -1) {
        LOGW("[agent_do_transform] BLOCK by simple_guard: %s#%s %s",
             targetClassName.c_str(), targetMethodName.c_str(), targetMethodSignature.c_str());
        if (nativeClass) env->DeleteLocalRef(nativeClass);
        return -1; // 按你的要求：直接返回失败
    } else if (sg == 1) {
        LOGI("[agent_do_transform] FORCE keep original: %s#%s %s",
             targetClassName.c_str(), targetMethodName.c_str(), targetMethodSignature.c_str());
        isRunOrigFun = true;
    }

    /** ---------------------- 检测方法可行性 ----------------------- */
    {
        jboolean modifiable = JNI_FALSE;
        if (!fsys::CheckJvmti(
                fAgentJvmtiEnv->IsModifiableClass(nativeClass, &modifiable),
                "[agent_do_transform] IsModifiableClass fail")) {
            env->DeleteLocalRef(nativeClass);
            return -1;
        }
        if (!modifiable) {
            LOGE("[agent_do_transform] %s 不可重定义（IsModifiableClass=false）",
                 targetClassName.c_str());
            env->DeleteLocalRef(nativeClass);
            return -1;
        }

        jint cls_mod = 0;
        if (!fsys::CheckJvmti(
                fAgentJvmtiEnv->GetClassModifiers(nativeClass, &cls_mod),
                "[agent_do_transform] GetClassModifiers(class) fail")) {
            env->DeleteLocalRef(nativeClass);
            return -1;
        }

        // 仅在类级别判断是否为接口
        if (cls_mod & fsys::kAccInterface) { // 0x0200
            LOGE("[agent_do_transform] %s 是接口（类级别），保守策略：跳过 Retransform。",
                 targetClassName.c_str());
            env->DeleteLocalRef(nativeClass);
            return -1;
        }

        // --- 方法级别 ---
        jint mmods = 0;
        if (!fsys::CheckJvmti(
                fAgentJvmtiEnv->GetMethodModifiers(methodID4jmethodID, &mmods),
                "[agent_do_transform] GetMethodModifiers(method) fail")) {
            env->DeleteLocalRef(nativeClass);
            return -1;
        }

        if (mmods & fsys::kAccNative) {       // 0x0100
            LOGE("[agent_do_transform] %s#%s 是 native，JVMTI Retransform 不支持。",
                 targetClassName.c_str(), targetMethodName.c_str());
            env->DeleteLocalRef(nativeClass);
            return -1;
        }
        if (mmods & fsys::kAccAbstract) {     // 0x0400
            LOGE("[agent_do_transform] %s#%s 是 abstract（方法级别），无法插桩。",
                 targetClassName.c_str(), targetMethodName.c_str());
            env->DeleteLocalRef(nativeClass);
            return -1;
        }

        // 按策略跳过编译器方法
        if (mmods & (fsys::kAccBridge | fsys::kAccSynthetic)) {
            LOGW("[agent_do_transform] %s#%s 是 bridge/synthetic，编译器方案不支持。",
                 targetClassName.c_str(), targetMethodName.c_str());
            env->DeleteLocalRef(nativeClass);
            return -1;
        }

    }

    // 每次hook先清空 不处理删除
    current_transform = nullptr;
    deploy::Transform *t = nullptr;

    auto it = fClassTransforms.find(targetClassName);
    if (it == fClassTransforms.end()) {
        // 创建新的 HookTransform
        LOGD("[agent_do_transform] 该类没有被hook 创建新的 %s %s",
             targetClassName.c_str(), targetMethodName.c_str())

        fClassTransforms[targetClassName] = std::make_unique<deploy::Transform>(
                targetClassName,
                methodId4long,
                isStatic,
                targetMethodName,
                targetMethodSignature,
                isHEnter,
                isHExit,
                isRunOrigFun);

        t = fClassTransforms[targetClassName].get();

    } else {
        // 判断方法是否已 hook
        if (it->second->hasHook(methodId4long)) {
            LOGW("[agent_do_transform] %s %s已安装 hook", targetClassName.c_str(),
                 targetMethodName.c_str())

            if (nativeClass != nullptr) {
                env->DeleteLocalRef(nativeClass);
            }
            return methodId4jlong; // 成功返回

        } else {
            // 向现有的 HookTransform 添加新方法 it->first key ; it->second value
            t = it->second.get();

            t->addHook(
                    methodId4long,
                    isStatic,
                    targetMethodName,
                    targetMethodSignature,
                    isHEnter,
                    isHExit,
                    isRunOrigFun);
            current_transform = t;
            LOGD("[agent_do_transform] 已存在该类 添加后数量 current_transform hooks size %d",
                 it->second.get()->getHooksSize())
        }
    }


    LOGD("[agent_do_transform] targetClassName: %s, targetMethodName: %s, targetMethodSignature: %s isStatic= %d",
         targetClassName.c_str(), targetMethodName.c_str(), targetMethodSignature.c_str(),
         isStatic)

    current_transform = t;

    // 强制重新加载这个类  会调用 HookClassFileLoadHook 每次 hook 一个方法， 安装都会调用重转换
    // ART 的 JVMTI 不支持对接口做 Retransform
    bool transRet = fsys::CheckJvmti(fAgentJvmtiEnv->RetransformClasses(1, &nativeClass),
                                     "[agent_do_transform] RetransformClasses fail");

    current_transform = nullptr;

    if (nativeClass != nullptr) {
        env->DeleteLocalRef(nativeClass);
    }

    if (!transRet) {
        LOGE("[agent_do_transform] RetransformClasses failed, rolling back ...");
        // 回滚这次新增
        if (t) {
            t->removeHook(methodId4long);              // 需要你在 Transform 里提供这个 API
            if (t->getHooksSize() == 0) {              // 没有其它 hook 了，移除整个类
                fClassTransforms.erase(targetClassName);
            }
        }
        current_transform = nullptr;                   // 避免悬垂指针
        return -1;
    }

    LOGD("[agent_do_transform] 运行完成方法ID大于0成功 res= %ld", methodId4jlong);
    return methodId4jlong;

}


/**
 * 对应  dcUnhook 通过C++ 符号反射回调
 */
extern "C" JNIEXPORT jboolean JNICALL agent_do_unHook_transform(
        JNIEnv *env, jlong jMethodID) {

    LOGD("[agent_do_unHook_transform] start ...")

    if (fAgentJvmtiEnv == nullptr) {
        LOGE("[agent_do_unHook_transform] fAgentJvmtiEnv is null")
        return false;
    }

    /** ---------------------- 拿到方法信息 结束 ----------------------- */

    long methodID4long = reinterpret_cast<long>(jMethodID);
    jmethodID methodID4jmethodID = reinterpret_cast<jmethodID>(jMethodID);

    std::string targetClassName, targetMethodName, targetMethodSignature;

    // 创建三个对象 自动回收
    LocalJvmCharPtr methodName(fAgentJvmtiEnv),
            signature(fAgentJvmtiEnv),
            generic(fAgentJvmtiEnv);

    if (!fsys::CheckJvmti(
            fAgentJvmtiEnv->GetMethodName(methodID4jmethodID,
                                          methodName.getPtr(),
                                          signature.getPtr(),
                                          generic.getPtr()),
            "[agent_do_unHook_transform] get method signature fail")) {
        return -1;
    }

    targetMethodName = methodName.getValue();
    targetMethodSignature = signature.getValue();

    // 获取方法信息
    bool isStatic;
    int modifiers; // 是否为静态方法 public、static、final 等，以整数形式表示
    if (!fsys::CheckJvmti(
            fAgentJvmtiEnv->GetMethodModifiers(
                    methodID4jmethodID, &modifiers),
            "[agent_do_unHook_transform]  native class modifiers fail")) {
        return -1;
    }
    isStatic = (modifiers & fsys::kAccStatic) != 0; // 0x0008 是否为静态方法

    jclass nativeClass = nullptr;
    if (!fsys::CheckJvmti(fAgentJvmtiEnv->GetMethodDeclaringClass(
                                  methodID4jmethodID, &nativeClass),
                          "[agent_do_unHook_transform] get native class fail")) {
        return -1;
    }

    LocalJvmCharPtr jniClassName(fAgentJvmtiEnv), jniClassGeneric(fAgentJvmtiEnv);
    if (!fsys::CheckJvmti(fAgentJvmtiEnv->GetClassSignature(
                                  nativeClass,
                                  jniClassName.getPtr(),
                                  jniClassGeneric.getPtr()),
                          "[agent_do_unHook_transform] get native class name fail")) {
        if (nativeClass != nullptr) {
            env->DeleteLocalRef(nativeClass);
        }
        return -1;
    }
    targetClassName = jniClassName.getValue();
    /** ---------------------- 拿到方法信息 结束 ----------------------- */

    // 每次hook先清空 不处理删除
    current_transform = nullptr;

    auto it = fClassTransforms.find(targetClassName);
    if (it == fClassTransforms.end()) {
        // 没有找到 没有被hoook
        LOGW("[agent_do_unHook_transform] %s %s 未被 hook", targetClassName.c_str(),
             targetMethodName.c_str())
        return true;
    }

    std::unique_ptr<deploy::Transform> staged =
            std::make_unique<deploy::Transform>(*it->second);

    if (!staged->removeHook(methodID4long)) {
        LOGW("[agent_do_unHook_transform] removeHook(staged) 失败，可能已被移除：%s#%s",
             targetClassName.c_str(), targetMethodName.c_str());
        if (nativeClass != nullptr) env->DeleteLocalRef(nativeClass);
        return JNI_TRUE;
    }
    bool staged_empty = (staged->getHooksSize() == 0);                        // ★ NEW

    // ★ NEW: 仅在 Retransform 期间暴露 current_transform，供 HookClassFileLoadHook 读取
    current_transform = staged.get();                                         // ★ NEW
    bool transRet = fsys::CheckJvmti(fAgentJvmtiEnv->RetransformClasses(1, &nativeClass),
                                     "[agent_do_unHook_transform] RetransformClasses fail");
    current_transform = nullptr;                                              // ★ NEW: 立刻清空

    if (nativeClass != nullptr) {
        env->DeleteLocalRef(nativeClass);
    }

    if (!transRet) {
        LOGE("[agent_do_unHook_transform] RetransformClasses 失败，保持原配置不变（未提交）")
        return JNI_FALSE;                                                     // ★ NEW: 内存未改，无需回滚
    }

    // ★ NEW: 成功 → 提交 staged 到 map
    if (staged_empty) {
        fClassTransforms.erase(it);
        LOGD("[agent_do_unHook_transform] 卸载完成：类 %s 已无任何 hook，已移除",
             targetClassName.c_str());
    } else {
        it->second = std::move(staged);
        LOGD("[agent_do_unHook_transform] 卸载完成：类 %s 仍有 %zu 个 hook",
             targetClassName.c_str(), it->second->getHooksSize());
    }

    LOGD("[agent_do_unHook_transform] 运行完成方法 %s 成功", targetMethodName.c_str())
    return JNI_TRUE;
}


/**
 * Debug.attachJvmtiAgent("libfhook_agent.so", null, context.getClassLoader());
 * 初始化入口
 * 创建JVMTI环境
 */
extern "C" JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM *vm, char *options,
                                                 void *reserved) {
    LOGD("[Agent_OnAttach] start ...")


    if (vm == nullptr) {
        LOGE("[Agent_OnAttach] vm is null")
        return false;
    }
    f_vm = vm; // 缓存全局

    jvmtiEnv *jvmti_env;
    // 全局ENV 缓存
    jint result = vm->GetEnv((void **) &jvmti_env, JVMTI_VERSION_1_2);
    if (result != JNI_OK) {
        LOGE("[Agent_OnAttach] GetEnv failed")
        return false;
    }

    fAgentJvmtiEnv = jvmti_env; // 缓存全局

    // 检查 JVMTI 能力（如 can_retransform_classes 支持类重转换），这是修改类字节码的前提
    int setRet = SetNeedCapabilities(jvmti_env);
    if (setRet != JVMTI_ERROR_NONE) {
        LOGE("[Agent_OnAttach] SetNeedCapabilities failed")
        return setRet;
    }

    // 该回调会在类加载 / 重转换时被触发，用于修改类的 Dex 字节码
    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.ClassFileLoadHook = &HookClassFileLoadHook; // 重加载时的回调

    //callbacks.MethodEntry = &MethoddEntry;
    int error = jvmti_env->SetEventCallbacks(&callbacks, sizeof(callbacks));
    LOGD("[Agent_OnAttach] SetEventCallbacks %d", error) //非 0 都是错误码

    // 当代理加载类文件时触发的事件
    jvmti_env->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
    LOGD("[Agent_OnAttach] 完成")
    return error;
}

// ---------------------------------------------------  查找内存实例 或实现
static bool CheckAndClear(JNIEnv *env) {
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return true;
    }
    return false;
}

static bool IsConcreteClass(JNIEnv *env, jclass cls) {
    if (cls == nullptr) return false;

    jclass classCls = env->FindClass("java/lang/Class");
    if (CheckAndClear(env) || !classCls) return false;

    jmethodID midGetModifiers = env->GetMethodID(classCls, "getModifiers", "()I");
    jmethodID midIsInterface = env->GetMethodID(classCls, "isInterface", "()Z");
    jmethodID midIsSynthetic = env->GetMethodID(classCls, "isSynthetic", "()Z");
    if (CheckAndClear(env) || !midGetModifiers || !midIsInterface || !midIsSynthetic) return false;

    // interface?
    jboolean isInterface = env->CallBooleanMethod(cls, midIsInterface);
    if (CheckAndClear(env)) return false;
    if (isInterface) return false;

    // abstract?
    jint mods = env->CallIntMethod(cls, midGetModifiers);
    if (CheckAndClear(env)) return false;

    jclass modifierCls = env->FindClass("java/lang/reflect/Modifier");
    if (CheckAndClear(env) || !modifierCls) return false;
    jmethodID midIsAbstract = env->GetStaticMethodID(modifierCls, "isAbstract", "(I)Z");
    if (CheckAndClear(env) || !midIsAbstract) return false;

    jvalue arg{};
    arg.i = mods; // 用 A 版本显式传参，避免可变参数问题
    jboolean isAbs = env->CallStaticBooleanMethodA(modifierCls, midIsAbstract, &arg);
    if (CheckAndClear(env)) return false;
    if (isAbs) return false;

    // synthetic?
    jboolean isSyn = env->CallBooleanMethod(cls, midIsSynthetic);
    if (CheckAndClear(env)) return false;
    if (isSyn) return false;

    // （可选）排除数组/原始类型
    jmethodID midIsArray = env->GetMethodID(classCls, "isArray", "()Z");
    jmethodID midIsPrimitive = env->GetMethodID(classCls, "isPrimitive", "()Z");
    if (midIsArray && env->CallBooleanMethod(cls, midIsArray)) return false;
    if (midIsPrimitive && env->CallBooleanMethod(cls, midIsPrimitive)) return false;

    return true;
}


extern "C" JNIEXPORT jobjectArray JNICALL agent_do_find_impl(JNIEnv *env,
                                                             jclass ifaceOrAbstract) {

    LOGD("[agent_do_find_impl] start ...")
    if (!fAgentJvmtiEnv || ifaceOrAbstract == nullptr) return nullptr;

    jint count = 0;
    jclass *classes = nullptr;
    jvmtiError err = fAgentJvmtiEnv->GetLoadedClasses(&count, &classes);
    if (err != JVMTI_ERROR_NONE || count <= 0 || !classes) return nullptr;

    std::vector<jclass> results;
    results.reserve(128);

    jclass classCls = env->FindClass("java/lang/Class");
    if (CheckAndClear(env) || !classCls) {
        fAgentJvmtiEnv->Deallocate(reinterpret_cast<unsigned char *>(classes));
        return nullptr;
    }
    jmethodID midIsAssignableFrom = env->GetMethodID(classCls, "isAssignableFrom",
                                                     "(Ljava/lang/Class;)Z");
    jmethodID midGetName = env->GetMethodID(classCls, "getName", "()Ljava/lang/String;");
    if (CheckAndClear(env) || !midIsAssignableFrom || !midGetName) {
        fAgentJvmtiEnv->Deallocate(reinterpret_cast<unsigned char *>(classes));
        LOGE("[agent_do_find_impl] GetMethodID failed")
        return nullptr;
    }

    for (int i = 0; i < count; ++i) {
        jclass c = classes[i];
        if (!c) continue;

        // ifaceOrAbstract.isAssignableFrom(c)
        jboolean ok = env->CallBooleanMethod(ifaceOrAbstract, midIsAssignableFrom, c);
        if (CheckAndClear(env) || !ok) continue;

        if (!IsConcreteClass(env, c)) continue;

        // （可选）过滤反射/代理类
        jstring jname = (jstring) env->CallObjectMethod(c, midGetName);
        if (CheckAndClear(env) || !jname) continue;
        const char *name = env->GetStringUTFChars(jname, nullptr);
        bool skip = name && (
                strncmp(name, "java.lang.reflect.", 18) == 0 ||
                strncmp(name, "sun.reflect.", 12) == 0
        );
        env->ReleaseStringUTFChars(jname, name);
        if (skip) continue;

        results.push_back((jclass) env->NewGlobalRef(c));
    }

    fAgentJvmtiEnv->Deallocate(reinterpret_cast<unsigned char *>(classes));

    // 打包返回：数组元素类型是 java/lang/Class
    jobjectArray arr = env->NewObjectArray((jsize) results.size(), classCls, nullptr);
    if (CheckAndClear(env) || !arr) {
        for (jclass gc: results) env->DeleteGlobalRef(gc);
        LOGE("[agent_do_find_impl] NewObjectArray failed")
        return nullptr;
    }
    for (jsize i = 0; i < (jsize) results.size(); ++i) {
        env->SetObjectArrayElement(arr, i, results[i]);
        env->DeleteGlobalRef(results[i]);
    }
    return arr;
}


static jint JNICALL TagInstanceCb(jlong /*class_tag*/,
                                  jlong /*size*/,
                                  jlong *tag_ptr,
                                  void *user_data) {
    // 仅打 tag，不做 JNI 操作
    *tag_ptr = (jlong) (uintptr_t) user_data;  // 非 0
    return JVMTI_ITERATION_CONTINUE;
}

extern "C"
JNIEXPORT jobjectArray JNICALL agent_do_find_instances(JNIEnv *env,
                                                       jclass klass,
                                                       jboolean only_live) {

    LOGD("[agent_do_find_instances] start ...")
    if (!fAgentJvmtiEnv || !klass) {
        jclass obj = env->FindClass("java/lang/Object");
        return env->NewObjectArray(0, obj, nullptr);
    }

    if (only_live) (void) fAgentJvmtiEnv->ForceGarbageCollection();

    jlong tag = ((((jlong) (uintptr_t) klass) ^ ((jlong) clock())) | 1);
    jvmtiError e = fAgentJvmtiEnv->IterateOverInstancesOfClass(
            klass, JVMTI_HEAP_OBJECT_EITHER, (jvmtiHeapObjectCallback) TagInstanceCb,
            (void *) (uintptr_t) tag);
    LOGD("IterateOverInstancesOfClass -> %d", e);

    jint count = 0;
    jobject *objects = nullptr;
    jlong *tags = nullptr;
    e = fAgentJvmtiEnv->GetObjectsWithTags(1, &tag, &count, &objects, &tags);
    LOGD("GetObjectsWithTags -> %d, count=%d", e, count);

    jclass objCls = env->FindClass("java/lang/Object");
    jobjectArray arr = env->NewObjectArray(count > 0 ? count : 0, objCls, nullptr);

    if (e == JVMTI_ERROR_NONE && count > 0 && objects) {
        for (jint i = 0; i < count; ++i) {
            env->SetObjectArrayElement(arr, i, objects[i]);
            (void) fAgentJvmtiEnv->SetTag(objects[i], 0);
        }
        fAgentJvmtiEnv->Deallocate((unsigned char *) objects);
        if (tags) fAgentJvmtiEnv->Deallocate((unsigned char *) tags);
    }
    return arr;
}



// ------------------------ Agent 初始化
extern "C" JNIEXPORT void JNICALL agent_do_init_success(JNIEnv *env,
                                                        jclass targetClass,
                                                        jboolean is_safe_mode,
                                                        jboolean is_debug) {

    g_is_safe_mode = is_safe_mode;
    gIsDebug = is_debug;
    if (!gIsDebug) {
        SetLogLevel(ANDROID_LOG_INFO, false);
//        SetLogLevel(ANDROID_LOG_DEBUG, false);
    }
    gIsShowSmail = true; // 默认值

}

