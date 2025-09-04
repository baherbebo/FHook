//
// Created by Administrator on 2025/9/2.
//

#ifndef FHOOK_TRANSFORM_H
#define FHOOK_TRANSFORM_H

#include <vector>
#include <memory>
#include <string>
#include "../dexter/slicer/dex_ir.h"

namespace deploy {
    struct Method {
        long j_method_id;
        bool is_static;
        std::string method_name;
        std::string method_signature;

        Method(long j_method_id,
               bool is_static,
               std::string method_name,
               std::string method_signature)
                : j_method_id(j_method_id),
                  is_static(is_static),
                  method_name(method_name),
                  method_signature(method_signature) {}

    };

    /**
     * hook 方法基础信息
     */
    struct MethodHooks : public Method {
        // 定义一个枚举类 位运算枚举，组合多种 Hook 类型

        bool isHEnter;
        bool isHExit;
        bool isRunOrigFun;

        MethodHooks(
                long j_method_id,
                bool is_static,
                const std::string &method_name,
                const std::string &method_signature,
                bool isHEnter,
                bool isHExit,
                bool isRunOrigFun
        )
                : Method(j_method_id,
                         is_static,
                         method_name,
                         method_signature
        ),
                  isHEnter(isHEnter),
                  isHExit(isHExit),
                  isRunOrigFun(isRunOrigFun) {}

    };

/**
 * Transform t2("Ljava/lang/String;");
 * std::cout << t2.GetJniClassName() << "\n"; // 输出: Ljava/lang/String;
 * std::cout << t2.GetClassName()    << "\n"; // 输出: java/lang/String
 */
    class Transform {
    public:
        Transform(const std::string &class_name,
                  long j_method_id,
                  bool is_static,
                  std::string &method_name,
                  std::string &method_signature,
                  bool isHEnter,
                  bool isHExit,
                  bool isRunOrigFun
        )
                : class_name_(class_name) {
            hooks_.clear();
            hooks_.emplace_back(j_method_id,
                                is_static,
                                method_name,
                                method_signature,
                                isHEnter,
                                isHExit,
                                isRunOrigFun
            );
        }

        Transform(const std::string &class_name) : class_name_(class_name) {
            hooks_.clear();
        }

        ~Transform() = default;

        Transform(const Transform &) = default;            // 明确允许“拷贝构造”
        Transform &operator=(const Transform &) = delete;  // 明确禁止“拷贝赋值”

        Transform(Transform &&) = default;                 // 明确允许“移动构造”
        Transform &operator=(Transform &&) = delete;       // 明确禁止“移动赋值”

        std::string GetJniClassName() const;

        std::string GetClassName() const;

        void set_app_loader(bool sys_loader);

        bool is_app_loader() const;

        void Apply(std::shared_ptr<ir::DexFile> dex_ir);

        bool hasHook(long mid) const;

        bool removeHook(long mid);

        void addHook(long mid,
                     bool is_static,
                     std::string &method_name,
                     std::string &method_signature,
                     bool isHEnter,
                     bool isHExit,
                     bool isRunOrigFun);

        int getHooksSize() const;

    private:
        const std::string class_name_;

        bool is_app_loader_ = false;

        std::vector<MethodHooks> hooks_; // 能够在同一个转换操作中处理多个方法 hook
    };

}


#endif //FHOOK_TRANSFORM_H
