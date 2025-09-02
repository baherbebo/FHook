//
// Created by Administrator on 2025/9/2.
//
#include "../FGlobal_flib.h"

#define TAG FEADRE_TAG("finject")

#include "finject.h"
#include "../dexter/slicer/dex_ir_builder.h"
#include "fir_tools.h"
#include "fir_funs_def.h"
#include "fir_funs_do.h"

namespace finject {


    bool do_finject(const deploy::Transform *transform,
                    const deploy::MethodHooks &hook_info,
                    lir::CodeIr *code_ir) {

        auto ir_method = code_ir->ir_method;
        // 方法信息
        LOGD("[do_finject] start ... %s -> %s %s ",
             ir_method->decl->parent->Decl().c_str(),
             ir_method->decl->name->c_str(),
             ir_method->decl->prototype->Signature().c_str())

        auto orig_return_type = ir_method->decl->prototype->return_type;
        ir::Builder builder(code_ir->dex_ir);

        int return_register = -1;
        if (!hook_info.isRunOrigFun) {
            fir_tools::clear_original_instructions(code_ir);
        } else {
            bool is_wide_out = false;
            return_register = fir_tools::find_return_register(code_ir, &is_wide_out);
            LOGI("[do_finject] return_register= %d, is_wide_out=%d", return_register,
                 is_wide_out);
        }

        // 确定插入点（MOD: 修复返回点找到逻辑）
        auto insert_point = fir_tools::find_first_code(code_ir);
        dex::u2 num_add_reg = 0; // 初始寄存器增加数量

        // 先定义5个，不一定会用这么多个
        dex::u2 v0 = 0;
        dex::u2 v1 = 1;
        dex::u2 v2 = 2;
        dex::u2 v3 = 3;
        dex::u2 v4 = 4;
        // *********************************************************************

        if (hook_info.isHEnter) {
            if (transform->is_app_loader()) {
                // 申请寄存器
                num_add_reg = fir_tools::req_reg(code_ir, 3); // 需要两个寄存器
                // 申请后的参数索引
                auto num_reg_non_param_new =
                        ir_method->code->registers - ir_method->code->ins_count;

                auto f_THook_onEnter = fir_funs_def::get_THook_onEnter(code_ir);
                fir_funs_do::cre_arr_object0(
                        code_ir, v0, v1, v2, insert_point);
                fir_funs_do::do_THook_onEnter(
                        f_THook_onEnter, code_ir,
                        v2, v0, v2,
                        hook_info.j_method_id, insert_point);

                if (hook_info.isRunOrigFun) {
                    LOGD("[do_finject] 恢复参数的类型 ...")
                    // 把打包包装的参数恢复成原有的类型
                    fir_tools::restore_reg_params4type(
                            code_ir,
                            v2,  //arr_reg
                            num_reg_non_param_new, //base_param_reg
                            v0, v1,
                            insert_point);
                }


            }
        }

        // 如果添加了寄存器 同时需要运行原始方法 则需要把参数移回原位
        if (num_add_reg > 0 && hook_info.isRunOrigFun) {
            LOGD("[do_finject] 恢复寄存器位置 ...")
            //扩展了几个寄存器 = 需要多少本地寄存器 - 原有多少个本地寄存器
            fir_tools::restore_reg_params4shift(code_ir, insert_point, num_add_reg);
        }

        if (!hook_info.isRunOrigFun) {
            LOGD("[do_finject] 添加返回代码 ...")
            // 如果清空了，一定要恢复
            fir_tools::cre_return_code(code_ir, orig_return_type, -1, insert_point);
        }

        return true;

    }
}
