//
// Created by Administrator on 2025/4/16.
//
#include "FGlobal_flib.h"

#define TAG FEADRE_TAG("FGlobal_flib")

// ========== 日志控制默认值： ==========
std::atomic<int>  g_log_level{ ANDROID_LOG_DEBUG }; // 默认：DEBUG
std::atomic<bool> g_log_exact_only{ false };       // 默认：阈值模式（打印 I/W/E），非“精确只 I”

bool gIsDebug = false;
bool gIsShowSmail = false;
std::string g_name_class_THook = "top.feadre.fhook.FHook";
std::string g_name_fun_onEnter = "onEnter4fhook";
std::string g_name_fun_onExit = "onExit4fhook";

std::string g_name_fun_MH_ENTER = "MH_ENTER";
std::string g_name_fun_MH_EXIT = "MH_EXIT";

bool g_is_safe_mode = true;


