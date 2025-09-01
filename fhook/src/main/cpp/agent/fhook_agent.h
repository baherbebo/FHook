//
// Created by Administrator on 2025/9/1.
//

#ifndef FHOOK_FHOOK_AGENT_H
#define FHOOK_FHOOK_AGENT_H

#include "../jvmti.h"

typedef jlong (*AgentDoTransformFn)(
        JNIEnv *env,
        jclass clazz,
        jobject methodID,
        jboolean isHEnter,
        jboolean isHExit,
        jboolean isRunOrigFun);

const std::string AgentDoTransformFnName = "agent_do_transform";

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


#endif //FHOOK_FHOOK_AGENT_H
