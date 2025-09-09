//
// Created by Administrator on 2025/9/1.
//
#include <dlfcn.h>
#include "../FGlobal_flib.h"

#define TAG FEADRE_TAG("fsys")

#include "fsys.h"
#include <link.h>
#include <sys/stat.h>

namespace fsys{
    /**
     *
     * @param handle
     * @param sym
     * @return
     */
    void *must_dlsym(void *handle, const char *sym) {
        dlerror(); // 清空上次错误
        void *p = dlsym(handle, sym); // 从动态库句柄中获取符号地址
        const char *err = dlerror();
        if (!p || err) {
            LOGE("[must_dlsym] fail: %s (%s)", sym, err ? err : "unknown");
            return nullptr;
        }
        return p;
    }

    /**
     * 尝试多路径打开 libart.so（在不同 Android 版本路径不同
     * @return
     */
    void* open_libart() {
        // 1) 先试 NOLOAD（如果已在本命名空间加载，最稳）
        void* h = dlopen("libart.so", RTLD_NOW | RTLD_NOLOAD);
        if (h) return h;

        // 2) 准备 __loader_dlopen
        void* libdl = dlopen("libdl.so", RTLD_NOW);
        if (!libdl) return nullptr;
        using __loader_dlopen_fn = void* (*)(const char*, int, const void*);
        auto __loader_dlopen = ( __loader_dlopen_fn ) dlsym(libdl, "__loader_dlopen");
        if (!__loader_dlopen) return nullptr;

        // 3) 在进程已加载列表中找到 libart.so 的基址
        struct Found { const char* name; uintptr_t base; } f { "libart.so", 0 };
        auto cb = [](struct dl_phdr_info* info, size_t, void* data)->int {
            auto* out = (Found*)data;
            if (!info->dlpi_name) return 0;
            // 末尾匹配 libart.so
            const char* p = strrchr(info->dlpi_name, '/');
            const char* fname = p ? p+1 : info->dlpi_name;
            if (strcmp(fname, out->name) == 0) { out->base = info->dlpi_addr; return 1; }
            return 0;
        };
        dl_iterate_phdr(cb, &f);

        if (f.base) {
            // 4) 用 caller=base 跨命名空间打开
            h = __loader_dlopen("libart.so", RTLD_NOW, (void*)f.base);
            if (h) return h;
        }

        // 5) 兜底再试常见 APEX 路径（有些 ROM 允许）
#if defined(__LP64__)
        const char* candidates[] = {
                "/apex/com.android.art/lib64/libart.so",
                "/system/lib64/libart.so",
                "/apex/com.android.runtime/lib64/libart.so",
        };
#else
        const char* candidates[] = {
        "/apex/com.android.art/lib/libart.so",
        "/system/lib/libart.so",
        "/apex/com.android.runtime/lib/libart.so",
    };
#endif
        for (auto p: candidates) {
            h = __loader_dlopen(p, RTLD_NOW, (void*)f.base /*没有也无所谓*/);
            if (h) return h;
        }

        return nullptr;
    }


    std::string jstring2cstring(JNIEnv* env, jstring jstr) {
        if (!jstr) return std::string(); // 处理空指针

        const char* chars = env->GetStringUTFChars(jstr, nullptr);
        if (!chars) return ""; // 获取失败处理
        std::string str(chars);
        env->ReleaseStringUTFChars(jstr, chars);
        return str;
    }

    static bool sameFile(const char *path1, const char *path2) {
        struct stat stat1, stat2;

        if (stat(path1, &stat1) < 0) {
            return false;
        }

        if (stat(path2, &stat2) < 0) {
            return false;
        }
        return (stat1.st_ino == stat2.st_ino) && (stat1.st_dev == stat2.st_dev);
    }

    void *fdlopen(const char *lib_name, int flags, bool loaded_only){
        LOGD("[DlUtil::dlopen] enter: lib_name=%s, flags=0x%x, loaded_only=%d",
             (lib_name ? lib_name : "(null)"), flags, loaded_only);

        // 安全判断：nullptr 或空串都直接失败
        if (!lib_name || lib_name[0] == '\0') {
            LOGE("[DlUtil::dlopen] lib_name is null or empty, return nullptr");
            return nullptr;
        }

        // 打开 libdl 并获取 __loader_dlopen
        void *libdl_handle = ::dlopen("libdl.so", RTLD_NOW);
        if (libdl_handle == nullptr){
            LOGE("[DlUtil::dlopen] open libdl fail");
            return nullptr;
        }
        LOGD("[DlUtil::dlopen] opened libdl.so handle=%p", libdl_handle);

        dlerror(); // 清空上次错误
        auto __loader_dlopen = reinterpret_cast<__loader_dlopen_fn>(
                ::dlsym(libdl_handle, "__loader_dlopen"));
        if (!__loader_dlopen) {
            const char* err = dlerror();
            LOGE("[DlUtil::dlopen] __loader_dlopen get fail: %s", err ? err : "(null)");
            ::dlclose(libdl_handle);
            return nullptr;
        }
        LOGD("[DlUtil::dlopen] resolved __loader_dlopen=%p", (void*)__loader_dlopen);

        void *lib_handle = nullptr;

        if (!loaded_only) {
            LOGD("[DlUtil::dlopen] calling __loader_dlopen(name=%s, flags=0x%x, caller=%p)",
                 lib_name, flags, (void*)__loader_dlopen);
            lib_handle = __loader_dlopen(lib_name, flags, (void *)__loader_dlopen);
            if (lib_handle == nullptr) {
                const char* err = dlerror();
                LOGE("[DlUtil::dlopen] __loader_dlopen returned nullptr. dlerror=%s",
                     err ? err : "(null)");
            } else {
                LOGD("[DlUtil::dlopen] __loader_dlopen success. handle=%p", lib_handle);
            }
        } else {
            LOGD("[DlUtil::dlopen] loaded_only=true, skip direct load and try dl_iterate_phdr fallback. name=%s",
                 lib_name);
        }

        if (lib_handle == nullptr) {
            LOGD("[DlUtil::dlopen] try dl_iterate_phdr for %s, loaded_only=%d", lib_name, loaded_only);

            dl_phdr_info targetData{};
            targetData.dlpi_name = lib_name;
            targetData.dlpi_addr = 0;

            auto iteratePhdrCb = [](struct dl_phdr_info *phdr_info, size_t /*size*/,
                                    void *data) -> int {
                dl_phdr_info *targetInfo = reinterpret_cast<dl_phdr_info *>(data);
                if (!phdr_info->dlpi_name) return 0;
                // 末尾精确匹配短名
                const char *sub_str = strstr(phdr_info->dlpi_name, targetInfo->dlpi_name);
                if (sub_str && strlen(sub_str) == strlen(targetInfo->dlpi_name)) {
                    LOGD("dl_iterate_phdr matched short name: want=%s, got=%s, base=%p",
                         targetInfo->dlpi_name, phdr_info->dlpi_name, (void*)phdr_info->dlpi_addr);
                    targetInfo->dlpi_addr = phdr_info->dlpi_addr;
                    return 1;
                }
                // 绝对路径同文件匹配
                if (targetInfo->dlpi_name && targetInfo->dlpi_name[0] == '/' &&
                    sameFile(targetInfo->dlpi_name, phdr_info->dlpi_name)) {
                    LOGD("dl_iterate_phdr matched same file: want=%s, got=%s, base=%p",
                         targetInfo->dlpi_name, phdr_info->dlpi_name, (void*)phdr_info->dlpi_addr);
                    targetInfo->dlpi_addr = phdr_info->dlpi_addr;
                    return 1;
                }
                return 0;
            };

            dl_iterate_phdr(iteratePhdrCb, reinterpret_cast<void *>(&targetData));
            if (targetData.dlpi_addr == 0) {
                LOGE("[DlUtil::dlopen] dl_iterate_phdr failed to locate %s, loaded_only=%d",
                     lib_name, loaded_only);
                ::dlclose(libdl_handle);
                return nullptr;
            }

            LOGD("[DlUtil::dlopen] dl_iterate_phdr found base=%p for %s",
                 (void*)targetData.dlpi_addr, lib_name);

            lib_handle = __loader_dlopen(lib_name, flags, (void *)targetData.dlpi_addr);
            if (lib_handle == nullptr) {
                const char* err = dlerror();
                LOGE("[DlUtil::dlopen] __loader_dlopen with base returned nullptr. dlerror=%s",
                     err ? err : "(null)");
            } else {
                LOGD("[DlUtil::dlopen] __loader_dlopen with base success. handle=%p", lib_handle);
            }
        }

        if (lib_handle) {
            LOGD("[DlUtil::dlopen] SUCCESS: name=%s, loaded_only=%d, handle=%p",
                 lib_name, loaded_only, lib_handle);
        } else {
            LOGE("[DlUtil::dlopen] FAIL: name=%s, loaded_only=%d, handle=nullptr",
                 lib_name, loaded_only);
        }

        // 记得关掉 libdl 句柄（避免小泄露）
        ::dlclose(libdl_handle);
        return lib_handle;
    }

    // 最佳努力：先 loaded_only，再跨域加载，最后系统 dlopen 兜底
    void* dlopen_best_effort(const char* lib_name, int flags) {
        LOGD("[fsys::dlopen_best_effort] enter name=%s flags=0x%x",
             lib_name ? lib_name : "(null)", flags);

        // 1) 仅已加载
        void* h = fdlopen(lib_name, flags, /*loaded_only=*/true);
        if (h) {
            LOGD("[fsys::dlopen_best_effort] hit loaded_only: handle=%p", h);
            return h;
        }

        // 2) 跨命名空间加载
        h = fdlopen(lib_name, flags, /*loaded_only=*/false);
        if (h) {
            LOGD("[fsys::dlopen_best_effort] cross-namespace success: handle=%p", h);
            return h;
        }

        // 3) 系统 dlopen 兜底（部分 ROM 可能放开了命名空间限制）
        h = ::dlopen(lib_name, flags);
        if (!h) {
            const char* err = dlerror();
            LOGE("[fsys::dlopen_best_effort] ::dlopen FAILED for %s (err=%s)",
                 lib_name ? lib_name : "(null)", err ? err : "(null)");
            return nullptr;
        }
        LOGD("[fsys::dlopen_best_effort] ::dlopen success: handle=%p", h);
        return h;
    }

    bool CheckJvmti(jvmtiError error, const std::string &error_message) {
        if (error != JVMTI_ERROR_NONE) {
            LOGE("%d:%s", error, error_message.c_str());
            return false;
        }
        return true;
    }


    // 把类名统一成 "java/lang/xxx" 形式
    std::string norm_cls(std::string s) {
        // 去掉前导 'L' 与末尾 ';'
        if (!s.empty() && s.front() == 'L') s.erase(s.begin());
        if (!s.empty() && s.back() == ';') s.pop_back();
        // 点号 -> 斜杠
        for (auto &ch: s) if (ch == '.') ch = '/';
        return s; // 例如 "java/lang/ClassLoader"
    }

}


