//
// Created by Administrator on 2025/9/2.
//
#include "../FGlobal_flib.h"

#define TAG FEADRE_TAG("transform")
#include "transform.h"

namespace deploy {
    /// Ltop/feadre/fhook/THook;
    std::string Transform::GetJniClassName() const { return class_name_; }

    /// top/feadre/fhook/THook
    std::string Transform::GetClassName() const {
        if (class_name_.length() > 2) {
            return class_name_.substr(1, class_name_.length() - 2);
        }
        return class_name_;
    };

    void Transform::set_sys_loader(bool sys_loader) {
        this->is_sys_loader_ = sys_loader;
    }

    void Transform::Apply(std::shared_ptr<ir::DexFile> dex_ir) {
        LOGD("[Transform::Apply]  %s", GetClassName().c_str());
        if (!dex_ir || hooks_.empty()) return;

        const std::string internal_name = GetClassName();  // 例如 java/lang/String 或原样（非 L…;）

        // 伪代码示意（根据你的 dexter 接口调整）：
        // auto* klass = dex_ir->FindClass(internal_name);
        // if (!klass) return;
        //
        // for (const auto& h : hooks_) {
        //     // h.method_name, h.method_signature（例如 "(II)I"）
        //     auto* method = klass->FindMethod(h.method_name, h.method_signature, h.is_static);
        //     if (!method) continue;
        //
        //     if (h.isHEnter) { /* 注入方法进入日志/回调 */ }
        //     if (h.isHExit)  { /* 注入方法退出日志/回调 */ }
        //     if (!h.isRunOrigFun) {
        //         /* 替换原实现或短路返回常量/代理调用 */
        //     }
        // }

        // 完成后通常需要 writer/encoder 把 IR 写回（你已有 dexter/writer.cc 可用）
    }

    // -------- Transform: Hook 列表管理 --------
    bool Transform::hasHook(long mid) const {
        return std::any_of(hooks_.begin(), hooks_.end(),
                           [mid](const MethodHooks& h){ return h.j_method_id == mid; });
    }

    bool Transform::removeHook(long mid) {
        const auto old_sz = hooks_.size();
        hooks_.erase(std::remove_if(hooks_.begin(), hooks_.end(),
                                    [mid](const MethodHooks& h){ return h.j_method_id == mid; }),
                     hooks_.end());
        return hooks_.size() != old_sz;
    }

    void Transform::addHook(long mid,
                            bool is_static,
                            std::string &method_name,
                            std::string &method_signature,
                            bool isHEnter,
                            bool isHExit,
                            bool isRunOrigFun) {
        // 若已存在同一 j_method_id，则更新配置；否则追加
        auto it = std::find_if(hooks_.begin(), hooks_.end(),
                               [mid](const MethodHooks& h){ return h.j_method_id == mid; });
        if (it != hooks_.end()) {
            it->is_static       = is_static;
            it->method_name     = method_name;       // 拷贝（入参是 lvalue ref）
            it->method_signature= method_signature;  // 拷贝
            it->isHEnter        = isHEnter;
            it->isHExit         = isHExit;
            it->isRunOrigFun    = isRunOrigFun;
            return;
        }
        hooks_.emplace_back(mid, is_static, method_name, method_signature,
                            isHEnter, isHExit, isRunOrigFun);
    }

    int Transform::getHooksSize() const {
        // size_t -> int 显式收窄（列表不会很大，安全）
        return static_cast<int>(hooks_.size());
    }
}
