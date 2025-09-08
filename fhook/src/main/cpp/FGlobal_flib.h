//
// Created by Administrator on 2025/4/16.
//

#ifndef PJ02_FGLOBAL_FLIB_H
#define PJ02_FGLOBAL_FLIB_H

#include <jni.h>
#include <android/log.h>
#include <string>
#include <vector>


static inline std::string RegVecToString(const std::vector<int> &v) {
    if (v.empty()) return "{}";
    std::string s;
    s.reserve(v.size() * 4 + 2);
    s.push_back('{');
    for (size_t i = 0; i < v.size(); ++i) {
        s += std::to_string(v[i]);
        if (i + 1 < v.size()) s += ", ";
    }
    s.push_back('}');
    return s;
}

// 向量转字符串 用于输出
#define VEC_CSTR(vec) RegVecToString((vec)).c_str()

// 绿色
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__);
// 黄色
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO , TAG, __VA_ARGS__);
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN , TAG, __VA_ARGS__);
// 红色
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__);

#define FEADRE_PREFIX "feadre_"  // 公共前缀
#define FEADRE_TAG(name) FEADRE_PREFIX name  // 宏拼接

extern bool gIsDebug;
extern std::string g_name_class_THook;
extern std::string g_name_fun_onEnter;
extern std::string g_name_fun_onExit;

extern std::string g_name_fun_MH_ENTER;
extern std::string g_name_fun_MH_EXIT;

// 用于string 和 char* 比较
inline bool SigEq(const std::string &a, const char *b) noexcept {
    return b && a == b;
}

// char 比较
inline bool StrEq(const char *a, const char *b) noexcept { return std::strcmp(a, b) == 0; }

#define CHECK_OR_RETURN(val, ret,msg)        \
    do {                                      \
        if (!(val)) {                         \
            LOGE("%s", msg);                  \
            return (ret);                     \
        }                                     \
    } while (0)

#endif //PJ02_FGLOBAL_FLIB_H
