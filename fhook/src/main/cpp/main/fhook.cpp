//
// Created by Administrator on 2025/9/1.
//
#include <dlfcn.h>
#include "../FGlobal_flib.h"

#define TAG FEADRE_TAG("fhook_agent")

#include "fhook.h"
#include "../tools/fsys.h"
#include "../agent/fhook_agent.h"
#include <string>

static void *fAgentHandle = nullptr;
static std::string fAgentPath;


jmethodID getMethodId(JNIEnv *env, jclass clazz, jobject jTargetMethod) {
    jmethodID methodID = env->FromReflectedMethod(jTargetMethod);
    if (!methodID) {
        LOGE("[getMethodInfo] method 方法ID引用失败");
        return JNI_FALSE;
    }
    return methodID;
//    return reinterpret_cast<jlong>(methodID);
}

template<typename FuncType>
/**
 * 从已加载的 JVMTI 代理库（libjtik_agent.so）中获取指定名称的函数指针
 * @tparam FuncType
 * @param funcName
 * @return
 */
static FuncType getAgentFn(const char *funcName) {
    // 去打开 句柄
    if (fAgentHandle == nullptr && !fAgentPath.empty()) {
        if (fAgentPath.empty()) {
            LOGE("[dcJvmtiSuccess] agent path empty")
            return nullptr;
        }
    }
    // 从动态库中获取指定名称的符号（函数）
    return reinterpret_cast<FuncType>(::dlsym(fAgentHandle, funcName));
}


/**
 * agent_do_transform 对应 agent_do_transform
 */
extern "C" JNIEXPORT jlong JNICALL dcHook(
        JNIEnv *env,
        jclass clazz,
        jobject jTargetMethod,
        jboolean isHEnter,
        jboolean isHExit,
        jboolean isRunOrigFun) {
    LOGD("[dcHook] start...")

    //拿到  这个函数 agent_do_transform
    auto transformFn = getAgentFn<AgentDoTransformFn>(
            AgentDoTransformFnName.c_str());

    jmethodID methodID = getMethodId(env, clazz, jTargetMethod);
    if (!methodID) {
        LOGE("[dcHook] 获取方法ID失败")
        return -1;
    }


    if (transformFn == nullptr) {
        LOGE("[dcHook] 没有找到符号 %s", AgentDoTransformFnName.c_str())
        return -1;
    }

    return transformFn(env, clazz, jTargetMethod, isHEnter, isHExit, isRunOrigFun);
}


enum jdwpState {
    UNKNOWN, ON, OFF
};

static std::atomic<jdwpState> f_orgJdwp{UNKNOWN};
static std::atomic<bool> f_orgRtKnown{false};
static std::atomic<bool> f_orgRtDebug{false};


/**
 * 把 ART 的 JDWP 开关（art::Dbg::SetJdwpAllowed(true)）打开 ,“放宽”调试限制
 * manifest 的 android:debuggable="true"
 */
// art::Dbg::IsJdwpAllowed() / SetJdwpAllowed(bool)
using fnIsJdwpAllowed = bool (*)();
using fnSetJdwpAllowed = void (*)(bool);

static jboolean dcEnableJdwp(JNIEnv *env, jclass clazz, jboolean enable) {
    void *h = fsys::open_libart();
    if (!h) {
        LOGE("open libart.so fail: %s", dlerror());
        return false;
    }

    static std::atomic<fnIsJdwpAllowed> pIs{nullptr};
    static std::atomic<fnSetJdwpAllowed> pSet{nullptr};

    auto fIs = pIs.load();
    auto fSet = pSet.load();
    if (!fIs) {
        fIs = (fnIsJdwpAllowed) fsys::must_dlsym(h, "_ZN3art3Dbg13IsJdwpAllowedEv");
        if (!fIs) {
            dlclose(h);
            return false;
        }
        pIs.store(fIs);
    }
    if (!fSet) {
        fSet = (fnSetJdwpAllowed) fsys::must_dlsym(h, "_ZN3art3Dbg14SetJdwpAllowedEb");
        if (!fSet) {
            dlclose(h);
            return false;
        }
        pSet.store(fSet);
    }

    if (f_orgJdwp.load() == UNKNOWN) {
        bool on = fIs();
        f_orgJdwp.store(on ? ON : OFF);
        LOGD("Original JDWP = %s", on ? "ON" : "OFF");
    }

    bool cur = fIs();
    if (enable && !cur) {
        LOGI("Enabling JDWP ...");
        fSet(true);
    } else if (!enable && cur && f_orgJdwp.load() == OFF) {
        LOGI("Disabling JDWP (restore OFF) ...");
        fSet(false);
    } else {
        LOGD("Skip JDWP change: want=%d cur=%d org=%d", enable ? 1 : 0, cur ? 1 : 0,
             f_orgJdwp.load());
    }
    dlclose(h);
    return true;
}


// ========== 2) Runtime::SetJavaDebuggable(true/false) ==========
/**
 * 说明：
 *   部分系统 attach JVMTI 还要求 Runtime.IsJavaDebuggable()==true。
 *   这里通过私有符号修改该标志，以“更像调试包”。
 *   注意：私有 API，可能随版本变化；已放了常见主符号。
 */
using fnSetRtDebug = void (*)(void *, bool);
using fnIsRtDebug = bool (*)(void *);

/**
 *
 * @param enable -1探测;0关;1开
 * @return
 */
static int dcSetRuntimeJavaDebuggable(JNIEnv *env, jclass clazz, int debuggable) {
    void *h = fsys::open_libart();
    if (!h) {
        LOGE("open libart.so fail");
        return -2;
    }

    void **ppRt = (void **) fsys::must_dlsym(h, "_ZN3art7Runtime9instance_E");
    if (!ppRt || !*ppRt) {
        dlclose(h);
        return -1;
    }
    void *rt = *ppRt;

    auto fSet = (fnSetRtDebug) fsys::must_dlsym(h, "_ZN3art7Runtime17SetJavaDebuggableEb");
    auto fIs = (fnIsRtDebug) fsys::must_dlsym(h, "_ZN3art7Runtime16IsJavaDebuggableEv");
    if (!fSet) {
        dlclose(h);
        return -1;
    }

    if (!f_orgRtKnown.load() && fIs) {
        bool org = fIs(rt);
        f_orgRtDebug.store(org);
        f_orgRtKnown.store(true);
        LOGD("Original Runtime.JavaDebuggable = %s", org ? "true" : "false");
    }

    int out = -1;
    if (debuggable < 0) { // 探测
        if (fIs) out = fIs(rt) ? 1 : 0;
    } else if (debuggable == 0) { // 关（仅在原本是 false 时才“强制恢复”）
        if (f_orgRtKnown.load() && !f_orgRtDebug.load()) {
            LOGI("Restoring Runtime.JavaDebuggable=false ...");
            fSet(rt, false);
        }
        if (fIs) out = fIs(rt) ? 1 : 0;
    } else { // 开
        LOGI("Setting Runtime.JavaDebuggable=true ...");
        fSet(rt, true);
        if (fIs) out = fIs(rt) ? 1 : 0;
    }

    dlclose(h);
    return out; // -1=缺符号/-2=libart失败/0=false/1=true
}

static jboolean dcIsJdwpAllowed(JNIEnv *env, jclass clazz) {
    void *h = fsys::open_libart();
    if (!h) return false;

    // 解析 art::Dbg::IsJdwpAllowed()
    auto fIs = (fnIsJdwpAllowed) fsys::must_dlsym(
            h,
            "_ZN3art3Dbg13IsJdwpAllowedEv");
    bool ok = false;
    if (fIs) ok = fIs(); // 调用它：true = 允许 JDWP，false = 不允许
    dlclose(h);
    return ok;
}


/// 签名 （参数）返回值
static JNINativeMethod methods[] = {
        {"dcHook",                     "(Ljava/lang/reflect/Method;ZZZ)J", reinterpret_cast<void *>(dcHook)},
        {"dcEnableJdwp",               "(Z)Z",                             reinterpret_cast<void *>(dcEnableJdwp)},
        {"dcSetRuntimeJavaDebuggable", "(I)I",                             reinterpret_cast<void *>(dcSetRuntimeJavaDebuggable)},
        {"dcIsJdwpAllowed",            "()Z",                              reinterpret_cast<void *>(dcIsJdwpAllowed)},
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGD("[JNI_OnLoad] start...")

    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        LOGE("[JNI_OnLoad] GetEnv failed");
        return JNI_ERR;
    }
    jclass clazz = env->FindClass("top/feadre/fhook/CLinker");
    if (!clazz) {
        LOGE("[JNI_OnLoad] FindClass CLinker failed");
        return JNI_ERR;
    }

    if (env->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(methods[0])) != 0) {
        LOGE("[JNI_OnLoad] RegisterNatives failed");
        return JNI_ERR;
    }

    return JNI_VERSION_1_6;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_top_feadre_fhook_CLinker_dcJvmtiSuccess(JNIEnv *env, jclass clazz,
                                             jstring name_so_fhook_agent) {
    LOGD("[dcJvmtiSuccess] start...")
    if (!name_so_fhook_agent) {
        return false;
    }

    fAgentPath = fsys::jstring2cstring(env, name_so_fhook_agent);
    if (fAgentPath.empty()) {
        LOGE("[dcJvmtiSuccess] agent path empty")
        return false;
    }

    fAgentHandle = fsys::dlopen_best_effort(fAgentPath.c_str(), RTLD_NOW);
    if (fAgentHandle == nullptr) {
        LOGE("[dcJvmtiSuccess] open %s fail", fAgentPath.c_str());
    }

    return true;

}