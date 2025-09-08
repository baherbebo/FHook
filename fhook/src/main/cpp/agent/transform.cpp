//
// Created by Administrator on 2025/9/2.
//
#include "../FGlobal_flib.h"

#define TAG FEADRE_TAG("transform")

#include "transform.h"
#include "../dexter/slicer/dex_ir_builder.h"
#include "../dexter/slicer/code_ir.h"
#include "fsmali_printer.h"
#include "finject.h"
#include <algorithm>
#include <vector>

namespace deploy {

    static inline bool IsBlacklistedMethod(ir::EncodedMethod *m) {
        if (!m || !m->decl || !m->decl->parent || !m->decl->prototype) return false;
        const std::string &cls = m->decl->parent->Decl();          // 例：Ljava/lang/Class;
        const char *mn = m->decl->name->c_str();            // 方法名
        const std::string &sig = m->decl->prototype->Signature();   // 例：(Ljava/lang/String;)Ljava/lang/Class;

        // 整包禁止
        if (StartsWith(cls.c_str(), "Ljava/lang/invoke/")) return true;
        if (StartsWith(cls.c_str(), "Ljava/lang/reflect/")) return true;

        // 这个是可以的
//        if (cls == "Ljava/lang/ClassLoader;" && strcmp(mn, "loadClass") == 0) return true;

        if (cls == "Ljava/lang/Class;") {
            if ((strcmp(mn, "getDeclaredMethod") == 0 &&
                 sig == "(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;") ||
                (strcmp(mn, "getDeclaredField") == 0 &&
                 sig == "(Ljava/lang/String;)Ljava/lang/reflect/Field;") ||
                (strcmp(mn, "getField") == 0 &&
                 sig == "(Ljava/lang/String;)Ljava/lang/reflect/Field;"))
                return true;
        }

        if (cls == "Ljava/lang/reflect/Method;" && strcmp(mn, "invoke") == 0 &&
            sig == "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;") {
            return true;
        }

        if (cls == "Ljava/lang/reflect/AccessibleObject;" && strcmp(mn, "setAccessible") == 0 &&
            sig == "(Z)V") {
            return true;
        }

        return false;
    }


    /// Ltop/feadre/fhook/THook;
    std::string Transform::GetJniClassName() const { return class_name_; }

    /// top/feadre/fhook/THook
    std::string Transform::GetClassName() const {
        if (class_name_.length() > 2) {
            return class_name_.substr(1, class_name_.length() - 2);
        }
        return class_name_;
    };

    void Transform::set_app_loader(bool sys_loader) {
        this->is_app_loader_ = sys_loader;
    }

    bool Transform::is_app_loader() const { return is_app_loader_; }

    // -------- Transform: Hook 列表管理 --------
    bool Transform::hasHook(long mid) const {
        return std::any_of(hooks_.begin(), hooks_.end(),
                           [mid](const MethodHooks &h) { return h.j_method_id == mid; });
    }

    bool Transform::removeHook(long mid) {
        const auto old_sz = hooks_.size();
        hooks_.erase(std::remove_if(hooks_.begin(), hooks_.end(),
                                    [mid](const MethodHooks &h) { return h.j_method_id == mid; }),
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
                               [mid](const MethodHooks &h) { return h.j_method_id == mid; });
        if (it != hooks_.end()) {
            it->is_static = is_static;
            it->method_name = method_name;       // 拷贝（入参是 lvalue ref）
            it->method_signature = method_signature;  // 拷贝
            it->isHEnter = isHEnter;
            it->isHExit = isHExit;
            it->isRunOrigFun = isRunOrigFun;
            return;
        }
        hooks_.emplace_back(mid, is_static, method_name, method_signature,
                            isHEnter, isHExit, isRunOrigFun);
    }

    int Transform::getHooksSize() const {
        // size_t -> int 显式收窄（列表不会很大，安全）
        return static_cast<int>(hooks_.size());
    }

    /**
     * 核心修改逻辑 这里是把整个原始类送进来改
     * @param dex_ir
     */
    void Transform::Apply(std::shared_ptr<ir::DexFile> dex_ir) {
        LOGD("[Transform::Apply] 开始Apply %s 类 ------------------- ", GetClassName().c_str());
        if (!dex_ir || hooks_.empty()) {
            // 不做任何处理，直接恢复原装
            LOGW("[Transform::Apply] dex_ir 为空 或 没有需要修改的方法")
            return;
        };

        int i = 0;
        for (MethodHooks &hook: hooks_) {
            LOGI("[Transform::Apply] 开始修改[%d] %s %s %s isStatic= %d isHEnter= %d, isHEnter= %d, isRunOrigFun= %d",
                 ++i,
                 this->is_app_loader_ ? "应用侧" : "系统侧",
                 hook.method_name.c_str(),
                 hook.method_signature.c_str(),
                 hook.is_static,
                 hook.isHEnter,
                 hook.isHExit,
                 hook.isRunOrigFun
            );

            // 组装 dex MethodId 类
            const ir::MethodId orig_method_obj(
                    GetJniClassName().c_str(),
                    hook.method_name.c_str(),
                    hook.method_signature.c_str(),
                    hook.is_static
            );

            ir::Builder builder(dex_ir);
            auto ir_method = builder.FindMethod(orig_method_obj);
            if (ir_method == nullptr) {
                // we couldn't find the specified method
                LOGE("[Transform::Apply] FindMethod 找不到指定方法")
                return;
            }

            // 黑名单
            if (IsBlacklistedMethod(ir_method) && g_is_safe_mode) {
                LOGW("[Apply] skip blacklisted method: %s#%s %s");
                continue;
            }

            if (ir_method->code == nullptr) {
                LOGE("[Transform::Apply] 方法不可修改 ir_method->code 为空")
                // 不可修改：抽象方法（无实现）、native 方法（实现由虚拟机提供）
                return;
            }

            // 拿到这个方法的IR
            lir::CodeIr code_ir(ir_method, dex_ir);

//            LOGD("[Transform::Apply] code_ir成功 ... %s -> %s %s ",
//                 ir_method->decl->parent->Decl().c_str(),
//                 ir_method->decl->name->c_str(),
//                 ir_method->decl->prototype->Signature().c_str())

            std::string _text = "修改前";
            SmaliPrinter::CodeIrToSmali4Print(&code_ir, _text);

            // do 注入 .... 修改
            bool ret = finject::do_finject(this, hook, &code_ir);

            _text = "修改后";
            SmaliPrinter::CodeIrToSmali4Print(&code_ir, _text);

            if (ret) {
                code_ir.Assemble();  // 方法块修改应用
                LOGI("[Transform::Apply] 修改完成 ...【成功】 %s -> %s %s ",
                     ir_method->decl->parent->Decl().c_str(),
                     ir_method->decl->name->c_str(),
                     ir_method->decl->prototype->Signature().c_str())
            } else {
                LOGE("[Transform::Apply] 修改完成 ...【失败】 %s -> %s %s ",
                     ir_method->decl->parent->Decl().c_str(),
                     ir_method->decl->name->c_str(),
                     ir_method->decl->prototype->Signature().c_str())
            }
        }
    }
}
