//
// Created by Administrator on 2025/9/1.
//

#ifndef FHOOK_FSYS_H
#define FHOOK_FSYS_H

#include "string"
#include "../jvmti.h"


namespace fsys {

    static constexpr uint32_t kAccPublic = 0x0001;  // class, field, method, ic
    static constexpr uint32_t kAccPrivate = 0x0002;  // field, method, ic
    static constexpr uint32_t kAccProtected = 0x0004;  // field, method, ic
    static constexpr uint32_t kAccStatic = 0x0008;  // field, method, ic
    static constexpr uint32_t kAccFinal = 0x0010;  // class, field, method, ic
    static constexpr uint32_t kAccSynchronized = 0x0020;  // method (only allowed on natives)
    static constexpr uint32_t kAccSuper = 0x0020;  // class (not used in dex)
    static constexpr uint32_t kAccVolatile = 0x0040;  // field
    static constexpr uint32_t kAccBridge = 0x0040;  // method (1.5)
    static constexpr uint32_t kAccTransient = 0x0080;  // field
    static constexpr uint32_t kAccVarargs = 0x0080;  // method (1.5)
    static constexpr uint32_t kAccNative = 0x0100;  // method
    static constexpr uint32_t kAccInterface = 0x0200;  // class, ic
    static constexpr uint32_t kAccAbstract = 0x0400;  // class, method, ic
    static constexpr uint32_t kAccStrict = 0x0800;  // method
    static constexpr uint32_t kAccSynthetic = 0x1000;  // class, field, method, ic
    static constexpr uint32_t kAccAnnotation = 0x2000;  // class, ic (1.5)
    static constexpr uint32_t kAccEnum = 0x4000;  // class, field, ic (1.5)



    // 以下是普通方法 ---------------------------

    using __loader_dlopen_fn = void *(*)(const char *filename, int flag,
                                         const void *caller_addr);

    void *open_libart();

    void *must_dlsym(void *handle, const char *sym);

    std::string jstring2cstring(JNIEnv *env, jstring jstr);

    void *fdlopen(const char *lib_name, int flags, bool loaded_only);

    void *dlopen_best_effort(const char *lib_name, int flags);

    bool CheckJvmti(jvmtiError error, const std::string &error_message);
}


#endif //FHOOK_FSYS_H
