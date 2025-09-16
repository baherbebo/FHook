//
// Created by Administrator on 2025/9/16.
//
#pragma once

#include <jni.h>
#include <string>


#ifdef __cplusplus
extern "C" {
#endif

const std::string AgentDoTransformFnName = "agent_do_transform";

typedef jlong (*AgentDoTransformFn)(
        JNIEnv *env,
        jclass clazz,
        jobject methodID,
        jboolean isHEnter,
        jboolean isHExit,
        jboolean isRunOrigFun);

const std::string AgentDoUnHookTransformFnName = "agent_do_unHook_transform";

typedef jboolean (*AgentDoUnHookTransformFn)(JNIEnv *env, jlong jTargetMethod);

const std::string agent_do_find_impl4name = "agent_do_find_impl";

typedef jobjectArray (*agent_do_find_impl4type)(JNIEnv *env, jclass clazz);

const std::string agent_do_find_instances4name = "agent_do_find_instances";

typedef jobjectArray (*agent_do_find_instances4type)(JNIEnv *env,
                                                     jclass targetClass,
                                                     jboolean onlyLive);

const std::string agent_do_init_success4name = "agent_do_init_success";

typedef void (*agent_do_init_success4type)(JNIEnv *env,
                                           jclass targetClass,
                                           jboolean is_safe_mode,
                                           jboolean is_debug);

// JVMTI 附加入口（Debug.attachJvmtiAgent 会走到这里）
JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM *vm, char *options, void *reserved);

// --- 你的“功能型”接口（JNI 形态）---
JNIEXPORT jlong       JNICALL agent_do_transform(JNIEnv *env, jclass clazz,
                                                 jobject jMethodObj,
                                                 jboolean isHEnter,
                                                 jboolean isHExit,
                                                 jboolean isRunOrigFun);

JNIEXPORT jboolean    JNICALL agent_do_unHook_transform(JNIEnv *env, jlong jMethodID);

JNIEXPORT jobjectArray JNICALL agent_do_find_impl(JNIEnv *env, jclass ifaceOrAbstract);

JNIEXPORT jobjectArray JNICALL agent_do_find_instances(JNIEnv *env, jclass klass,
                                                       jboolean only_live);

JNIEXPORT void        JNICALL agent_do_init_success(JNIEnv *env, jclass targetClass,
                                                    jboolean is_safe_mode,
                                                    jboolean is_debug);

#ifdef __cplusplus
} // extern "C"
#endif //FHOOKAGENT_FHOOK_AGENT_OUT_H
