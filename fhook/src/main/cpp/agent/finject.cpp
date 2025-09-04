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
#include "freg_manager.h"

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

        bool is_wide_return_register = false; // 返回类型是否为宽，用于返回值校验
        if (!hook_info.isRunOrigFun) {
            fir_tools::clear_original_instructions(code_ir);
        } else {
            // 原始返回值寄存器编号
            return_register = fir_tools::find_return_register(code_ir, &is_wide_return_register);
            LOGI("[do_finject] return_register= %d, is_wide_return_register= %d", return_register,
                 is_wide_return_register);
        }

        // 清除操作后 确定插入点（MOD: 修复返回点找到逻辑）
        auto insert_point = fir_tools::find_first_code(code_ir);
        dex::u2 num_add_reg = 0;
        dex::u2 num_reg_non_param_new = 0;

        if (transform->is_app_loader()) {
            // 申请 只调一次，提前确认好
//            num_add_reg = fir_tools::req_reg(code_ir, 4); // 应用侧 需要3个寄存器
//            num_add_reg = fir_tools::req_reg(code_ir, 5); // 反射侧 需要5寄存器
            num_add_reg = FRegManager::RequestLocals(code_ir, 5); // 这里会视情况调整

            // 这是源参数的寄存器索引
            num_reg_non_param_new = FRegManager::Locals(code_ir);
        } else {

        }


        // *********************************************************************

        // ------------------ Enter ------------------
        if (hook_info.isHEnter) {
//            if (!transform->is_app_loader()) {  // 调试使用
            if (transform->is_app_loader()) {
                LOGD("[do_finject] 应用侧 调用 isHEnter ...")

                // --------------------- 1
                int count = 3;
                auto regs1 = FRegManager::AllocV(
                        code_ir, {}, count, "regs1");
                CHECK_ALLOC_OR_RET(regs1, count, false, "[do_finject] regs1 申请寄存器失败 ...");

                // 运行方法
                auto f_THook_onEnter = fir_funs_def::get_THook_onEnter(code_ir);

                fir_funs_do::cre_arr_object0(
                        code_ir, regs1[0], regs1[1], regs1[2], insert_point);


                // --------------------- 2
                count = 1; // 先申请宽
                std::vector<int> forbidden_v = {regs1[2]}; // 禁止使用
                auto regs2_wide = FRegManager::AllocWide(
                        code_ir, forbidden_v, count, "regs2_wide");
                CHECK_ALLOC_OR_RET(regs2_wide, count, false,
                                   "[do_finject] regs2_wide 申请寄存器失败 ...");

                count = 1;
                forbidden_v.push_back(regs2_wide[0]);
                forbidden_v.push_back(regs2_wide[0] + 1);
                auto regs3 = FRegManager::AllocV(
                        code_ir, forbidden_v, count, "regs3");
                CHECK_ALLOC_OR_RET(regs3, count, false, "[do_finject] regs3 申请寄存器失败 ...");


                fir_funs_do::do_THook_onEnter(
                        f_THook_onEnter, code_ir,
                        regs1[2], regs2_wide[0], regs3[0],
                        hook_info.j_method_id, insert_point);


                // --------------------- 3
                count = 2;
                forbidden_v.clear();
                forbidden_v.push_back(regs3[0]);
                auto regs4 = FRegManager::AllocV(
                        code_ir, forbidden_v, count, "regs4");
                CHECK_ALLOC_OR_RET(regs4, count, false, "[do_finject] regs4 申请寄存器失败 ...");


                // 需要运行原方法才恢复
                if (hook_info.isRunOrigFun) {
                    LOGD("[do_finject] 恢复参数的类型 ...")
                    // 把打包包装的参数恢复成原有的类型
                    fir_tools::restore_reg_params4type(
                            code_ir,
                            regs3[0],  //arr_reg
                            num_reg_non_param_new, //base_param_reg
                            regs4[0], regs4[1],
                            insert_point);
                }

            } else {
                // 系统侧 调用
                LOGD("[do_finject] 系统侧 调用 isHEnter ...")

                // --------------------- 1
                int count = 3;
                std::vector<int> forbidden_v = {}; // 禁止使用
                auto regs5 = FRegManager::AllocV(
                        code_ir, forbidden_v, count, "regs5");
                CHECK_ALLOC_OR_RET(regs5, count, false, "[do_finject] regs5 申请寄存器失败 ...");

                fir_funs_do::cre_arr_do_args4onEnter(
                        code_ir, regs5[0], regs5[1], regs5[2],
                        hook_info.j_method_id, insert_point);


                // --------------------- 2
                count = 3;
                forbidden_v.push_back(regs5[2]);
                auto regs6 = FRegManager::AllocV(
                        code_ir, forbidden_v, count, "regs6");
                CHECK_ALLOC_OR_RET(regs6, count, false, "[do_finject] regs6 申请寄存器失败 ...");
                // 调用 onEnter 函数
                // 创建参数 Class[] 在 v2 （Class[]{Object[].class,Long.class}）
                fir_funs_do::cre_arr_class_args4onEnter(
                        code_ir, regs6[0], regs6[1], regs6[2], insert_point);

                // --------------------- 3
                count = 3;
                forbidden_v.push_back(regs6[2]);
                auto regs7 = FRegManager::AllocV(code_ir, forbidden_v, count);

                // 执行结果返回到 v0`
                fir_funs_do::do_apploader_static_fun(
                        code_ir, regs7[0], regs7[1], regs7[2],
                        regs6[2], regs5[2],
                        regs7[0],
                        g_name_class_THook, g_name_fun_onEnter,
                        insert_point);
            }

            // 如果添加了寄存器 同时需要运行原始方法 则需要把参数移回原位
            if (num_add_reg > 0 && hook_info.isRunOrigFun) {
                LOGD("[do_finject] 恢复寄存器位置 ...")
                //扩展了几个寄存器 = 需要多少本地寄存器 - 原有多少个本地寄存器
                fir_tools::restore_reg_params4shift(code_ir, insert_point, num_add_reg);
            }
        }

        // ...... do RunOrigFun ......

        // ------------------ Exit ------------------

        if (hook_info.isHExit) {

            // 更新指针 删除原有返回代码 这里如果执行原有方法，才需要删除
            auto insert_return = fir_tools::find_return_code(code_ir);
            insert_point = insert_return;
            if (hook_info.isRunOrigFun && insert_return != code_ir->instructions.end()) {
                ++insert_point;
                code_ir->instructions.Remove(*insert_return);
            }


            if (!transform->is_app_loader()) { //  调试使用
//            if (transform->is_app_loader()) {
                LOGD("[do_finject] 应用侧 调用 isHExit ...")

                // --------------------- 1
                // 确保不与宽冲突 创建参数
                int reg_arg = fir_funs_do::cre_arr_do_args4onExit(
                        code_ir,
                        return_register, is_wide_return_register,
                        insert_point);
                if (reg_arg < 0) return false; // 里面已汇报了错误


                // --------------------- 2
                int count = 1; // 申请一个宽存
                std::vector<int> forbidden_v = {reg_arg}; // 禁止使用
                auto regs8_wide = FRegManager::AllocWide(
                        code_ir, forbidden_v, count, "regs8_wide");
                CHECK_ALLOC_OR_RET(regs8_wide, count, false,
                                   "[do_finject] regs8_wide 申请寄存器失败 ...");

                auto f_THook_onExit = fir_funs_def::get_THook_onExit(code_ir);
                bool res = fir_funs_do::do_THook_onExit(
                        f_THook_onExit, code_ir,
                        reg_arg, reg_arg, regs8_wide[0],
                        hook_info.j_method_id, insert_point);
                if (!res) return false;

                reg_return = reg_arg;

            } else {
                // 系统侧 搞用
                LOGD("[do_finject] 系统侧 调用 isHExit ...")

                // --------------------- 1
                int reg_arg = fir_funs_do::cre_arr_do_args4onExit(
                        code_ir,
                        return_register,
                        is_wide_return_register,
                        insert_point);
                if (reg_arg < 0) return false;// 里面已汇报了错误


                // --------------------- 2 反射 Class 要再包一层
                int count = 2;
                std::vector<int> forbidden_v = {reg_arg}; // 禁止使用
                auto regs9 = FRegManager::AllocWide(
                        code_ir, forbidden_v, count, "regs9");
                CHECK_ALLOC_OR_RET(regs9, count, false,
                                   "[do_finject] regs9 申请寄存器失败 ...");

                // 反射要再包一层  Class[]{Object[].class,Long.class}
                fir_funs_do::cre_arr_object1(
                        code_ir, regs9[0], reg_arg, regs9[1],
                        hook_info.j_method_id, insert_point);


                // --------------------- 3
                count = 3;// 前面只有最后的封装参数需要保留
                forbidden_v.clear();
                forbidden_v.push_back(regs9[1]);
                auto regs10 = FRegManager::AllocV(
                        code_ir, forbidden_v, count, "regs10");
                CHECK_ALLOC_OR_RET(regs10, count, false, "[do_finject] regs10 申请寄存器失败 ...");

                fir_funs_do::cre_arr_class_args4onExit(
                        code_ir, regs10[0], regs10[1], regs10[2],
                        insert_point);


                // --------------------- 4
                count = 3;
                forbidden_v.push_back(regs10[2]);
                auto regs11 = FRegManager::AllocV(
                        code_ir, forbidden_v, count, "regs11");
                CHECK_ALLOC_OR_RET(regs11, count, false, "[do_finject] regs11 申请寄存器失败 ...");

                fir_funs_do::do_apploader_static_fun(
                        code_ir, regs11[0], regs11[1], regs11[2],
                        regs10[2], regs9[1], regs9[1],
                        g_name_class_THook, g_name_fun_onExit,
                        insert_point);

                reg_return = regs9[1];
            }

        }

        // 如果不执行原方法 或者运行了 isHExit 就需要 cre_return_code
        if (!hook_info.isRunOrigFun || hook_info.isHExit) {
            LOGD("[do_finject] 添加返回代码 ...")
            // 如果清空了，一定要恢复
            bool res = fir_tools::cre_return_code(
                    code_ir, orig_return_type,
                    reg_return, hook_info.isHExit, insert_point);
            if (!res) return false;
        }

        return true;

    }
}
