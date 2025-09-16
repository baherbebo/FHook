//
// Created by Administrator on 2025/4/16.
//
#pragma once

#ifndef PJ02_FGLOBAL_FLIB_H
#define PJ02_FGLOBAL_FLIB_H

#include <jni.h>
#include <android/log.h>
#include <string>
#include <vector>
#include <atomic>
#include <cstring>   // for std::strcmp

// ========== 向量转字符串：仅用于日志拼接 ==========
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

// 仅建议在日志参数中使用（临时字符串在整个 printf 调用期间有效）
#define VEC_CSTR(vec) RegVecToString((vec)).c_str()

// ========== TAG 拼接 ==========
#define FEADRE_PREFIX "feadre_"            // 公共前缀
#define FEADRE_TAG(name) FEADRE_PREFIX name // 要求 name 为字符串字面量

// ========== 运行期全局日志控制 ==========
extern std::atomic<int>  g_log_level;       // 默认 INFO（见 .cpp）
extern std::atomic<bool> g_log_exact_only;  // false: 阈值模式（>=level），true: 精确只该级别


/**
 * SetLogLevel(ANDROID_LOG_INFO, true); // 只打印 INFO，W/E 也静音
 * SetLogLevel(ANDROID_LOG_INFO, false); // 打印 I/W/E（不打印 D）
 * SetLogLevel(ANDROID_LOG_ERROR, true); // 仅 E
 * @param level
 * @param exactOnly
 */
static inline void SetLogLevel(int level, bool exactOnly = false) noexcept {
    g_log_level.store(level, std::memory_order_relaxed);
    g_log_exact_only.store(exactOnly, std::memory_order_relaxed);
}
static inline int  GetLogLevel()      noexcept { return g_log_level.load(std::memory_order_relaxed); }
static inline bool GetLogExactOnly()  noexcept { return g_log_exact_only.load(std::memory_order_relaxed); }

static inline bool ShouldLog(int prio) noexcept {
    const int  lv    = g_log_level.load(std::memory_order_relaxed);
    const bool exact = g_log_exact_only.load(std::memory_order_relaxed);
    return exact ? (prio == lv) : (prio >= lv);
}

// 绿色 / 黄色 / 红色
#define LOGD(...) do { if (ShouldLog(ANDROID_LOG_DEBUG)) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__); } while (0);
#define LOGI(...) do { if (ShouldLog(ANDROID_LOG_INFO )) __android_log_print(ANDROID_LOG_INFO , TAG, __VA_ARGS__); } while (0);
#define LOGW(...) do { if (ShouldLog(ANDROID_LOG_WARN )) __android_log_print(ANDROID_LOG_WARN , TAG, __VA_ARGS__); } while (0);
#define LOGE(...) do { if (ShouldLog(ANDROID_LOG_ERROR)) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__); } while (0);

// ========== 运行期全局日志控制 --- ==========

// ========== 业务全局 ==========
extern bool gIsDebug;
extern bool gIsShowSmail;
extern std::string g_name_class_THook;
extern std::string g_name_fun_onEnter;
extern std::string g_name_fun_onExit;

extern std::string g_name_fun_MH_ENTER;
extern std::string g_name_fun_MH_EXIT;

extern bool g_is_safe_mode;

// ========== 字符串帮助 ==========
inline bool SigEq(const std::string &a, const char *b) noexcept {
    return b && a == b;
}
inline bool StrEq(const char *a, const char *b) noexcept {
    return (a && b) ? (std::strcmp(a, b) == 0) : (a == b);
}
inline bool StartsWith(const char *s, const char *p) noexcept {
    if (!s || !p) return false;
    while (*p && *s && *s == *p) { ++s; ++p; }
    return *p == '\0';
}

// ========== 常用检查 ==========
#define CHECK_OR_RETURN(val, ret, msg) do { \
    if (!(val)) { LOGE("%s", msg); return (ret); } \
} while (0)

#ifndef CHECK_BOOL_EXPR
#define CHECK_BOOL_EXPR(expr, msg)                                                     \
    do {                                                                               \
        if ((expr)) {                                                                  \
            /* 先打错误日志，再抛异常 */                                               \
            LOGE("[CHECK_BOOL_EXPR] (%s) %s @%s:%d", #expr, (msg), __FILE__, __LINE__);\
            /* 条件满足 -> 抛异常中止 */                                                \
            throw std::runtime_error((msg));                                           \
        }                                                                              \
    } while (0)
#endif


#endif // PJ02_FGLOBAL_FLIB_H
