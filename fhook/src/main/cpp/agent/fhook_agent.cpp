//
// Created by Administrator on 2025/9/1.
//

#include "../FGlobal_flib.h"

#define TAG FEADRE_TAG("fhook_agent")

#include <string>

#include "fhook_agent.h"
#include "../tools/fsys.h"

static jvmtiEnv *fAgentJvmtiEnv = nullptr; // 全局JVMTI环境


/**
 * 当代理加载类文件时触发的事件   sAgentJvmtiEnv->RetransformClasses(1, &nativeClass) 重载回调
 */
extern "C" void JNICALL HookClassFileLoadHook(
        jvmtiEnv *jvmti,
        JNIEnv *jni,
        jclass class_being_redefined, // 首次加载的类为 nullptr,已加载类的重转换
        jobject loader, // 该类的类加载器对象
        const char *name,
        jobject protection_domain,
        jint class_data_len,
        const unsigned char *class_data, // 指向原始类字节码的指针
        jint *new_class_data_len,
        unsigned char **new_class_data) {

//    LOGD("[HookClassFileLoadHook] 重加载或系统调用 start 类名 %s,",         name)
//
//    /** ------------- 反射查看类加载器 的类型 ------------- */
//    if (loader == nullptr) {
//        LOGW("[HookClassFileLoadHook] Class %s is loaded by Bootstrap ClassLoader", name);
//    } else {
//        jclass loaderClass = jni->GetObjectClass(loader);
//
//        jclass pathClassLoader = jni->FindClass("dalvik/system/PathClassLoader");
//        jclass bootClassLoader = jni->FindClass("java/lang/BootClassLoader");
//
//        if (jni->IsInstanceOf(loader, bootClassLoader)) {
//            // 负责加载 core-lib（/system/framework/*.jar 里的类，比如 java.lang.*
//            LOGW("[HookClassFileLoadHook] Class %s is loaded by BootClassLoader", name);
//        } else if (jni->IsInstanceOf(loader, pathClassLoader)) {
//            LOGW("[HookClassFileLoadHook] Class %s is loaded by PathClassLoader (application class)",
//                 name);
//        } else {
//            // 其它自定义的 classloader
//            jmethodID mid = jni->GetMethodID(loaderClass, "toString", "()Ljava/lang/String;");
//            jstring desc = (jstring) jni->CallObjectMethod(loader, mid);
//            const char *str = jni->GetStringUTFChars(desc, nullptr);
//            LOGW("[HookClassFileLoadHook] Class %s is loaded by custom loader: %s", name, str);
//            jni->ReleaseStringUTFChars(desc, str);
//            jni->DeleteLocalRef(desc);
//        }
//
//        jni->DeleteLocalRef(loaderClass);
//        jni->DeleteLocalRef(pathClassLoader);
//        jni->DeleteLocalRef(bootClassLoader);
//    }
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

static bool getMethodInfo(JNIEnv *jniEnv, jvmtiEnv *jvmtiEnv,
                          jmethodID inJmethodId,
                          std::string &outClassName,
                          std::string &outMethodName,
                          std::string &outMethodSignature) {

    if (jvmtiEnv == nullptr || !inJmethodId) {
        return false;
    }
    LocalJvmCharPtr methodName(jvmtiEnv), signature(jvmtiEnv), generic(jvmtiEnv);
    if (!fsys::CheckJvmti(
            jvmtiEnv->GetMethodName(inJmethodId,
                                    methodName.getPtr(),
                                    signature.getPtr(),
                                    generic.getPtr()),
            "get method signature fail")) {
        return false;
    }
    outMethodName = methodName.getValue();
    outMethodSignature = signature.getValue();

    jclass nativeClass = nullptr;
    if (!fsys::CheckJvmti(jvmtiEnv->GetMethodDeclaringClass(
                                  inJmethodId, &nativeClass),
                          "get native class fail")) {
        return false;
    }

    LocalJvmCharPtr jniClassName(jvmtiEnv), jniClassGeneric(jvmtiEnv);
    if (!fsys::CheckJvmti(jvmtiEnv->GetClassSignature(
                                  nativeClass,
                                  jniClassName.getPtr(),
                                  jniClassGeneric.getPtr()),
                          "get native class name fail")) {
        if (nativeClass != nullptr) {
            jniEnv->DeleteLocalRef(nativeClass);
        }
        return false;
    }
    outClassName = jniClassName.getValue();
    if (nativeClass != nullptr) {
        jniEnv->DeleteLocalRef(nativeClass);
    }
    return true;

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

    std::string targetClassName, targetMethodName, targetMethodSignature;
    bool result = getMethodInfo(env, fAgentJvmtiEnv,
                                nativeMethodID,
                                targetClassName,
                                targetMethodName,
                                targetMethodSignature);
    if (!result) {
        LOGE("[agent_do_transform] getMethodInfo failed")
        return -1;
    }

    LOGD("[agent_do_transform] targetClassName: %s, targetMethodName: %s, targetMethodSignature: %s",
         targetClassName.c_str(), targetMethodName.c_str(), targetMethodSignature.c_str())
    return 0;

}


