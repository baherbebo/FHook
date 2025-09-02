//
// Created by Administrator on 2025/9/1.
//

#include "../FGlobal_flib.h"

#define TAG FEADRE_TAG("fhook_agent")

#include <string>

#include "fhook_agent.h"
#include "../tools/fsys.h"
#include "transform.h"

static jvmtiEnv *fAgentJvmtiEnv = nullptr; // 全局JVMTI环境
static std::map<std::string, std::unique_ptr<deploy::Transform>> fClassTransforms;// 当前已hook的所有配置
thread_local deploy::Transform *current_transform = nullptr; // 当前hook配置，用于缓存和隔离其它类

/**
 * 当代理加载类文件时触发的事件   sAgentJvmtiEnv->RetransformClasses(1, &nativeClass) 重载回调
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
        LOGD("[HookClassFileLoadHook] 当前没有hook配置 类名= %s", name)
        return;
    }

    if (current_transform->GetClassName() != name) {
        // 通常由系统加载的类，这里需要过滤掉
        LOGE("[HookClassFileLoadHook] 不需要配置 %s != %s", current_transform->GetClassName().c_str(), name)
        return;
    }

    LOGI("[HookClassFileLoadHook] 配置类名 %s ",  name)

    // 这里确定是要改hook配置  --------

    /** ------------- 反射查看类加载器 的类型 ------------- */
    if (loader == nullptr) {
        // Bootstrap（BootClassLoader）
        current_transform->set_sys_loader(true);
        LOGW("[HookClassFileLoadHook] Class %s is loaded by Bootstrap ClassLoader", name);
    } else {
        current_transform->set_sys_loader(false);

        jclass loaderClass = jni->GetObjectClass(loader);
        if (!loaderClass) {
            if (jni->ExceptionCheck()) jni->ExceptionClear();
            LOGW("[HookClassFileLoadHook] %s by <loader ?>(GetObjectClass failed)", name);
            return;
        }

        jclass pathClassLoader = jni->FindClass("dalvik/system/PathClassLoader");
        jclass bootClassLoader = jni->FindClass("java/lang/BootClassLoader");

        if (jni->IsInstanceOf(loader, bootClassLoader)) {
            // 负责加载 core-lib（/system/framework/*.jar 里的类，比如 java.lang.*
            LOGW("[HookClassFileLoadHook] Class %s is loaded by BootClassLoader", name);
        } else if (jni->IsInstanceOf(loader, pathClassLoader)) {
            // 这里就是app loader 加载的类
            LOGW("[HookClassFileLoadHook] Class %s is loaded by PathClassLoader (application class)",
                 name);
        } else {
            // 其它自定义的 classloader
            jmethodID mid = jni->GetMethodID(loaderClass, "toString", "()Ljava/lang/String;");
            jstring desc = (jstring) jni->CallObjectMethod(loader, mid);
            const char *str = jni->GetStringUTFChars(desc, nullptr);
            LOGW("[HookClassFileLoadHook] Class %s is loaded by custom loader: %s", name, str);
            jni->ReleaseStringUTFChars(desc, str);
            jni->DeleteLocalRef(desc);
        }

        jni->DeleteLocalRef(loaderClass);
        jni->DeleteLocalRef(pathClassLoader);
        jni->DeleteLocalRef(bootClassLoader);
    }
}

static int SetNeedCapabilities(jvmtiEnv *jvmti) {
    jvmtiCapabilities caps = {0};
    jvmtiError error;
    error = jvmti->GetPotentialCapabilities(&caps);
    LOGD("[SetNeedCapabilities] %d, %d, %d, %d", caps.can_redefine_classes,
         caps.can_retransform_any_class, caps.can_set_native_method_prefix, error);
    jvmtiCapabilities newCaps = {0};
    newCaps.can_retransform_classes = 1;
    if (caps.can_set_native_method_prefix) {
        newCaps.can_set_native_method_prefix = 1;
    }
    return jvmti->AddCapabilities(&newCaps);
}


/**
 * 对应  dcHook 通过C++ 符号反射回调
 */
extern "C" JNIEXPORT jlong JNICALL agent_do_transform(
        JNIEnv *env,
        jclass clazz,
        jobject jTargetMethod,
        jboolean isHEnter,
        jboolean isHExit,
        jboolean isRunOrigFun) {

    LOGD("[agent_do_transform] start ...")

    if (fAgentJvmtiEnv == nullptr || !jTargetMethod) {
        LOGE("[agent_do_transform] fAgentJvmtiEnv or jTargetMethod is null")
        return -1;
    }

    jmethodID nativeMethodID = env->FromReflectedMethod(jTargetMethod);
    if (!nativeMethodID) {
        LOGE("[agent_do_transform] method 方法ID引用失败");
        return -1;
    }

    /** ---------------------- 拿到方法信息 开始 ----------------------- */
    std::string targetClassName, targetMethodName, targetMethodSignature;

    // 创建三个对象 自动回收
    LocalJvmCharPtr methodName(fAgentJvmtiEnv),
            signature(fAgentJvmtiEnv),
            generic(fAgentJvmtiEnv);

    if (!fsys::CheckJvmti(
            fAgentJvmtiEnv->GetMethodName(nativeMethodID,
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
                    nativeMethodID, &modifiers),
            "[agent_do_transform]  native class modifiers fail")) {
        return -1;
    }
    isStatic = (modifiers & fsys::kAccStatic) != 0; // 0x0008 是否为静态方法

    jclass nativeClass = nullptr;
    if (!fsys::CheckJvmti(fAgentJvmtiEnv->GetMethodDeclaringClass(
                                  nativeMethodID, &nativeClass),
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

    // 每次hook先清空 不处理删除
    if (current_transform != nullptr) {
        current_transform = nullptr;
    }

    long methodId4long = reinterpret_cast<long>(nativeMethodID);
    long methodId4jlong = reinterpret_cast<jlong>(nativeMethodID);
    auto it = fClassTransforms.find(targetClassName);
    if (it == fClassTransforms.end()) {
        // 创建新的 HookTransform
        LOGI("[agent_do_transform] 该类没有被hook 创建新的 %s %s",
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

        current_transform = fClassTransforms[targetClassName].get();
    } else {
        // 判断方法是否已 hook
        // 判断方法是否已 hook
        if (it->second->hasHook(methodId4long)) {
            LOGW("[agent_do_transform] %s %s已安装 hook", targetClassName.c_str(),
                 targetMethodName.c_str())

            return methodId4jlong; // 成功返回

        } else {
            // 向现有的 HookTransform 添加新方法 it->first key ; it->second value
            it->second->addHook(
                    methodId4long,
                    isStatic,
                    targetMethodName,
                    targetMethodSignature,
                    isHEnter,
                    isHExit,
                    isRunOrigFun);
            current_transform = it->second.get();
            LOGD("[agent_do_transform] 已存在该类 添加后数量 current_transform hooks size %d",
                 it->second.get()->getHooksSize())
        }
    }


    LOGD("[agent_do_transform] targetClassName: %s, targetMethodName: %s, targetMethodSignature: %s isStatic= %d",
         targetClassName.c_str(), targetMethodName.c_str(), targetMethodSignature.c_str(),
         isStatic)

    // 类配置

    // 4. 强制重新加载这个类  会调用 HookClassFileLoadHook 每次 hook 一个方法， 安装都会调用重转换
    bool transRet = fsys::CheckJvmti(fAgentJvmtiEnv->RetransformClasses(1, &nativeClass),
                                     "[agent_do_transform] RetransformClasses fail");

    if (nativeClass != nullptr) {
        env->DeleteLocalRef(nativeClass);
    }

    jlong res = transRet ? methodId4jlong : -1;
    LOGD("[agent_do_transform] 运行完成大于0成功 res= %ld", res);
    return res;

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


    jvmtiEnv *jvmti_env;
    // 全局ENV 缓存
    jint result = vm->GetEnv((void **) &jvmti_env, JVMTI_VERSION_1_2);
    if (result != JNI_OK) {
        LOGE("[Agent_OnAttach] GetEnv failed")
        return false;
    }

    fAgentJvmtiEnv = jvmti_env;

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

