//
// Created by Administrator on 2025/9/1.
//

#include "t01.h"

#include <jni.h>
#include <string>
#include <dlfcn.h>
#include <android/log.h>

#define TAG "t01"
// 绿色
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__);
// 黄色
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO , TAG, __VA_ARGS__);
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN , TAG, __VA_ARGS__);
// 红色
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__);

extern "C" JNIEXPORT int add_plus2(int a, int b) {
    return a + b;
}


extern "C"
JNIEXPORT jint JNICALL
Java_top_feadre_fhook_FHook_jcFt001T02(JNIEnv *env, jclass clazz, jint a, jint b) {
    return add_plus2(a, b);
}