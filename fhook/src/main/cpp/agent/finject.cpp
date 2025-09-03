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

        int return_register = -1; // 这个主要用于 exit
        int reg_return = -1; // hook后的返回值寄存器编号

        bool is_wide_return_register = false; // 用于返回值校验
        if (!hook_info.isRunOrigFun) {
            fir_tools::clear_original_instructions(code_ir);
        } else {
            // 原始返回值寄存器编号
            return_register = fir_tools::find_return_register(code_ir, &is_wide_return_register);
            LOGI("[do_finject] return_register= %d, is_wide_return_register=%d", return_register,
                 is_wide_return_register);
        }

        // 清除操作后 确定插入点（MOD: 修复返回点找到逻辑）
        auto insert_point = fir_tools::find_first_code(code_ir);
        dex::u2 num_add_reg = 0;
        dex::u2 num_reg_non_param_new = 0;

        if (transform->is_app_loader()) {
            // 申请 只调一次，提前确认好
            num_add_reg = fir_tools::req_reg(code_ir, 4); // 需要两个寄存器
            // 这是源参数的寄存器索引
            num_reg_non_param_new = ir_method->code->registers - ir_method->code->ins_count;
        }

        // 先定义5个，不一定会用这么多个
        dex::u2 v0 = 0;
        dex::u2 v1 = 1;
        dex::u2 v2 = 2;
        dex::u2 v3 = 3;
        dex::u2 v4 = 4;
        // *********************************************************************

        // ------------------ Enter ------------------
        if (hook_info.isHEnter) {
            if (transform->is_app_loader()) {

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

                // 如果添加了寄存器 同时需要运行原始方法 则需要把参数移回原位
                if (num_add_reg > 0 && hook_info.isRunOrigFun) {
                    LOGD("[do_finject] 恢复寄存器位置 ...")
                    //扩展了几个寄存器 = 需要多少本地寄存器 - 原有多少个本地寄存器
                    fir_tools::restore_reg_params4shift(code_ir, insert_point, num_add_reg);
                }
            }
        }

        // ...... do RunOrigFun ......

        // ------------------ Exit ------------------

        if (hook_info.isHExit) {
            if (transform->is_app_loader()) {
                // 更新指针
                auto insert_return = fir_tools::find_return_code(code_ir);

                // 确保不与宽冲突 创建参数
                int reg_arg = fir_funs_do::cre_arr_do_args4onExit(
                        code_ir,
                        return_register, return_register,
                        insert_return);
                if (reg_arg < 0) return false;
                LOGD("[do_finject] reg_arg= %d", reg_arg)

                auto f_THook_onExit = fir_funs_def::get_THook_onExit(code_ir);
                bool res = fir_funs_do::do_THook_onExit(f_THook_onExit, code_ir,
                                                        reg_arg, v0,
                                                        hook_info.j_method_id, insert_return);
                if (!res) return false;
                reg_return = v0;
            }
        }

        if (!hook_info.isRunOrigFun) {
            LOGD("[do_finject] 添加返回代码 ...")
            // 如果清空了，一定要恢复
            fir_tools::cre_return_code(code_ir, orig_return_type,
                                       reg_return, insert_point);
        }

        return true;

    }
}
