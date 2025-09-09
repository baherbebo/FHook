//
// Created by Administrator on 2025/9/1.
//

#ifndef FHOOK_FHOOK_AGENT_H
#define FHOOK_FHOOK_AGENT_H

#include "../jvmti.h"
#include "../dexter/slicer/writer.h"

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

class LocalJvmCharPtr {
public:
    explicit LocalJvmCharPtr(jvmtiEnv *jvmtiEnv) : mJvmtiEnv(jvmtiEnv), mPtr(nullptr) {}

    char **getPtr() { return &mPtr; }

    char *getValue() const { return mPtr; }

    ~LocalJvmCharPtr() {
        if (mJvmtiEnv != nullptr && mPtr != nullptr) {
            mJvmtiEnv->Deallocate((unsigned char *) mPtr);
        }
    }

private:
    jvmtiEnv *mJvmtiEnv;
    char *mPtr;
};

class JvmtiAllocator : public dex::Writer::Allocator {
public:
    JvmtiAllocator(jvmtiEnv *jvmti) : jvmti_(jvmti) {}

    virtual void *Allocate(size_t size) {
        unsigned char *alloc = nullptr;
        jvmti_->Allocate(size, &alloc);
        return (void *) alloc;
    }

    virtual void Free(void *ptr) {
        if (ptr == nullptr) {
            return;
        }

        jvmti_->Deallocate((unsigned char *) ptr);
    }

private:
    jvmtiEnv *jvmti_;
};

#endif //FHOOK_FHOOK_AGENT_H
