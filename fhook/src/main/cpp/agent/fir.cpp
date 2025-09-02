//
// Created by Administrator on 2025/8/28.
//
#include "log.h"

#define TAG FEADRE_TAG("freplace_fun")

#include "fir.h"

#include "dexter/slicer/fsmali_printer.h"


#include <vector>
#include <string>

namespace fir {
    /**
 * 通用创建lir::Method的方法
 * @param code_ir        代码中间表示（用于分配lir::Method对象）
 * @param builder        ir::Builder实例（用于创建类型、原型、方法声明）
 * @param return_type    返回值类型描述符（如"I"→int，"Ljava/lang/String;"→String，"V"→void）
 * @param param_types    参数类型描述符列表（如{"Ljava/lang/String;", "I"}→String和int参数）
 * @param method_name    方法名（如"d"→Log.d的方法名）
 * @param owner_class    方法所属类的类型描述符（如"Landroid/util/Log;"→Log类）
 * @return 成功返回lir::Method*，失败返回nullptr
 */
    lir::Method *FIR::cre_method(
            lir::CodeIr *code_ir,
            const std::string &name_class,
            const std::string &name_method,
            const std::vector<std::string> &param_types,
            const std::string &return_type
    ) {
        ir::Builder builder(code_ir->dex_ir);

        // 1. 校验入参有效性（避免空指针或无效描述符导致崩溃）
        if (code_ir == nullptr || return_type.empty() || name_method.empty() ||
            name_class.empty()) {
            LOGE("[cre_method] 入参无效（code_ir=nullptr或关键字符串为空）");
            return nullptr;
        }

        // 2. 构建返回值类型（从描述符获取ir::Type）
        ir::Type *ir_return_type = builder.GetType(return_type.c_str());
        if (ir_return_type == nullptr) {
            LOGE("[cre_method] 无效的返回类型描述符：%s", return_type.c_str());
            return nullptr;
        }

        // 3. 构建参数类型列表（从描述符列表获取ir::TypeList）
        std::vector<ir::Type *> ir_param_types;
        for (const auto &param_desc: param_types) {
            ir::Type *param_type = builder.GetType(param_desc.c_str());
            if (param_type == nullptr) {
                LOGE("[cre_method] 无效的参数类型描述符：%s", param_desc.c_str());
                return nullptr;
            }
            ir_param_types.push_back(param_type);
        }
        ir::TypeList *ir_type_list = builder.GetTypeList(ir_param_types);  // 包装为TypeList

        // 4. 构建方法原型（Proto = 返回类型 + 参数类型列表）
        ir::Proto *ir_proto = builder.GetProto(
                ir_return_type, ir_type_list);
        if (ir_proto == nullptr) {
            LOGE("[cre_method] 构建方法原型失败（返回类型：%s，参数数：%zu）",
                 return_type.c_str(), param_types.size());
            return nullptr;
        }

        // 5. 构建方法声明（MethodDecl = 方法名 + 原型 + 所属类）
        ir::String *ir_method_name = builder.GetAsciiString(name_method.c_str());  // 方法名字符串
        ir::Type *ir_owner_class = builder.GetType(name_class.c_str());          // 所属类类型
        if (ir_method_name == nullptr || ir_owner_class == nullptr) {
            LOGE("[cre_method] 构建方法名或所属类失败（方法名：%s，类：%s）",
                 name_method.c_str(), name_class.c_str());
            return nullptr;
        }

        ir::MethodDecl *ir_method_decl = builder.GetMethodDecl(
                ir_method_name,    // 方法名
                ir_proto,          // 方法原型
                ir_owner_class     // 所属类
        );
        if (ir_method_decl == nullptr) {
            LOGE("[cre_method] 构建MethodDecl失败");
            return nullptr;
        }

        // 6. 分配lir::Method对象（传入MethodDecl和原始索引）
        // 注：decl->orig_index是Dex常量池中的原始索引，确保非kInvalidIndex
        if (ir_method_decl->orig_index == dex::kNoIndex) {
            LOGE("[cre_method] MethodDecl原始索引无效（可能未注册到常量池）");
            return nullptr;
        }

        lir::Method *lir_method = code_ir->Alloc<lir::Method>(
                ir_method_decl,
                ir_method_decl->orig_index
        );
        if (lir_method == nullptr) {
            LOGE("[cre_method] 分配lir::Method失败");
        }

        return lir_method;
    }

}
