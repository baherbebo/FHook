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

/**
 * 有 extern "C" add_plus
 * 无 extern "C" _Z8add_plusii
 * JNIEXPORT 让符号对外可见
 */
extern "C" JNIEXPORT int add_plus1(int a, int b) {
    return a + b;
}


extern "C"
JNIEXPORT jint JNICALL
Java_top_feadre_fhook_activitys_MainActivity_jcFt001T01(JNIEnv *env, jobject thiz, jint a, jint b) {
    LOGD("[jcFt001T01] ...")
    return add_plus1(a, b);
}