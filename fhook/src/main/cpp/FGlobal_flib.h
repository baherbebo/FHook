//
// Created by Administrator on 2025/4/16.
//

#ifndef PJ02_FGLOBAL_FLIB_H
#define PJ02_FGLOBAL_FLIB_H

#include <jni.h>
#include <android/log.h>
#
// 绿色
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__);
// 黄色
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO , TAG, __VA_ARGS__);
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN , TAG, __VA_ARGS__);
// 红色
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__);

#define FEADRE_PREFIX "feadre_"  // 公共前缀
#define FEADRE_TAG(name) FEADRE_PREFIX name  // 宏拼接


#endif //PJ02_FGLOBAL_FLIB_H
