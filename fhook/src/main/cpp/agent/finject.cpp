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

    // 仅判断三种“框架关键方法”
    static bool is_frame_method(const ir::EncodedMethod *m) {
        if (!m || !m->decl || !m->decl->parent || !m->decl->prototype) return false;

        const std::string cls = m->decl->parent->Decl();           // 例：Ljava/lang/ClassLoader;
        const std::string mn = m->decl->name->c_str();            // 方法名
        const std::string sig = m->decl->prototype->Signature();   // 例：(Ljava/lang/String;)Ljava/lang/Class;

        // 1) ClassLoader.loadClass(String): (Ljava/lang/String;)Ljava/lang/Class;
        if (cls == "Ljava/lang/ClassLoader;" && mn == "loadClass"
            && SigEq(sig, "(Ljava/lang/String;)Ljava/lang/Class;")) {
            return true;
        }

        // 2) Class.getDeclaredMethod(String, Class[]): (Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;
        if (cls == "Ljava/lang/Class;" && mn == "getDeclaredMethod"
            && SigEq(sig, "(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;")) {
            return true;
        }

        // 3) Method.invoke(Object, Object[]): (Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;
        if (cls == "Ljava/lang/reflect/Method;" && mn == "invoke"
            && SigEq(sig, "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;")) {
            return true;
        }

        return false;
    }


    /**
     Thread thread = Thread.currentThread();
    ClassLoader contextClassLoader = thread.getContextClassLoader();
     * @param m
     * @return
     */
    static bool is_bridge_critical_ir(const ir::EncodedMethod *m) {
        if (!m || !m->decl || !m->decl->parent || !m->decl->prototype) return false;
        const std::string cls = m->decl->parent->Decl();
        const std::string mn = m->decl->name->c_str();
        const std::string sig = m->decl->prototype->Signature();

        // Thread.currentThread()
        if (cls == "Ljava/lang/Thread;" && mn == "currentThread"
            && sig == "()Ljava/lang/Thread;")
            return true;

        // Thread.getContextClassLoader()
        if (cls == "Ljava/lang/Thread;" && mn == "getContextClassLoader"
            && sig == "()Ljava/lang/ClassLoader;")
            return true;


        return false;
    }


    static bool do_HEnter(const deploy::Transform *transform,
                          const deploy::MethodHooks hook_info,
                          lir::CodeIr *code_ir,
                          slicer::IntrusiveList<lir::Instruction>::Iterator insert_point,
                          unsigned short &num_add_reg,
                          unsigned short &num_reg_non_param_new,
                          bool &is_run_backup_plan) {

        LOGD("[do_HEnter] 运行 isHEnter ......")

        int count;
        std::vector<int> forbidden_v = {}; // 禁止使用
        dex::u4 reg_do_return = 0; //执行后的返回值所有的寄存器用于恢复

        if (transform->is_app_loader()) {
            LOGD("[do_HEnter] 应用侧 调用 isHEnter ...")

            // --------------------- 1 参数包装成 object[]
            count = 3;
            forbidden_v.clear();
            auto regs1 = FRegManager::AllocV(
                    code_ir, forbidden_v, count, "regs1");
            CHECK_ALLOC_OR_RET(regs1, count, false, "[do_HEnter] regs1 申请寄存器失败 ...");

            // 运行方法
            auto f_THook_onEnter = fir_funs_def::get_THook_onEnter(code_ir);

            fir_funs_do::cre_arr_object0(
                    code_ir, regs1[0], regs1[1], regs1[2], insert_point);


            // --------------------- 2 直接执行 do_THook_onEnter 这个没得返回 改了的参数在 regs3[0]
            count = 1; // 先申请宽
            forbidden_v.push_back(regs1[2]);
            auto regs2_wide = FRegManager::AllocWide(
                    code_ir, forbidden_v, count, "regs2_wide");
            CHECK_ALLOC_OR_RET(regs2_wide, count, false,
                               "[do_HEnter] regs2_wide 申请寄存器失败 ...");

            count = 1;
            forbidden_v.push_back(regs2_wide[0]);
            forbidden_v.push_back(regs2_wide[0] + 1);
            auto regs3 = FRegManager::AllocV(
                    code_ir, forbidden_v, count, "regs3");
            CHECK_ALLOC_OR_RET(regs3, count, false, "[do_HEnter] regs3 申请寄存器失败 ...");

            bool res = fir_funs_do::do_THook_onEnter(
                    f_THook_onEnter, code_ir,
                    regs1[2], regs2_wide[0], regs3[0],
                    hook_info.j_method_id, insert_point);
            if (!res)return false;

            reg_do_return = regs3[0];

        } else {
            // 系统侧 调用
            LOGD("[do_HEnter] 系统侧 调用 isHEnter ...")

            // --------------------- 1
            count = 3;
            forbidden_v.clear();
            auto regs5 = FRegManager::AllocV(
                    code_ir, forbidden_v, count, "regs5");
            CHECK_ALLOC_OR_RET(regs5, count, false, "[do_HEnter] regs5 申请寄存器失败 ...");

            fir_funs_do::cre_arr_do_args4onEnter(
                    code_ir, regs5[0], regs5[1], regs5[2],
                    hook_info.j_method_id, insert_point);


            // --------------------- 2
            count = 3;
            forbidden_v.push_back(regs5[2]);
            auto regs6 = FRegManager::AllocV(
                    code_ir, forbidden_v, count, "regs6");
            CHECK_ALLOC_OR_RET(regs6, count, false, "[do_HEnter] regs6 申请寄存器失败 ...");
            // 调用 onEnter 函数
            // 创建参数 Class[] 在 v2 （Class[]{Object[].class,Long.class}）
            fir_funs_do::cre_arr_class_args4onEnter(
                    code_ir, regs6[0], regs6[1], regs6[2], insert_point);

            // --------------------- 3
            count = 3;
            forbidden_v.push_back(regs6[2]);
            auto regs7 = FRegManager::AllocV(code_ir, forbidden_v, count);

            // 执行结果返回到 v0`
            bool res = fir_funs_do::do_apploader_static_fun(
                    code_ir, regs7[0], regs7[1], regs7[2],
                    regs6[2], regs5[2],
                    regs7[0],
                    g_name_class_THook, g_name_fun_onEnter,
                    insert_point);
            if (!res)return false;

            reg_do_return = regs7[0];
        }


        // --------------------- 3 恢复类型和位置

        // 没有执行原始方法 则不需要恢复参数类型
        if (hook_info.isRunOrigFun) {
            count = 2;
            forbidden_v.clear();
            forbidden_v.push_back(reg_do_return);
            auto regs4 = FRegManager::AllocV(
                    code_ir, forbidden_v, count, "regs4");
            CHECK_ALLOC_OR_RET(regs4, count, false, "[do_HEnter] regs4 申请寄存器失败 ...");

//             需要运行原方法才恢复
            LOGD("[do_HEnter] 恢复参数的类型 ...")
            // 把打包包装的参数恢复成原有的类型
            fir_tools::restore_reg_params4type(
                    code_ir,
                    reg_do_return,  //arr_reg
                    num_reg_non_param_new, //base_param_reg
                    regs4[0], regs4[1],
                    insert_point);
        }

        // 如果添加了寄存器 同时需要运行原始方法 则需要把参数移回原位
        if (num_add_reg > 0 && hook_info.isRunOrigFun) {
            LOGD("[do_HEnter] 恢复寄存器位置 ...")
            //扩展了几个寄存器 = 需要多少本地寄存器 - 原有多少个本地寄存器
            fir_tools::restore_reg_params4shift(code_ir, insert_point, num_add_reg);
        }

        return true;
    }

    int doHExit(const deploy::Transform *transform,
                const deploy::MethodHooks &hook_info,
                lir::CodeIr *code_ir,
                slicer::IntrusiveList<lir::Instruction>::Iterator insert_point,
                int &reg_return_orig, // 原方法返回值所在寄存器 清空了这里是 -1
                bool &is_wide_reg_return,
                bool &is_run_backup_plan) {

        LOGD("[doHExit] 运行 isHExit ......")
        int reg_return_dst = -1;  // 最终返回值

        // 申请一个统一可用的返回寄存器 根据返回寄存器类型 epilogue
        // 如果需要运行原方法才进行统一出口， reg_return_orig 肯定不会是 -1
        if (hook_info.isRunOrigFun) {
            // reg_return_orig 里存的返回值原方法，不能动 这里 reg_return_orig 可能是宽寄存器
            fir_tools::instrument_with_epilogue(code_ir, reg_return_orig);
        }

        int count;
        std::vector<int> forbidden_v = {}; // 禁止使用


        // 如果清空了，这里拿到的是一个null
        count = 1; // 申请一个宽存
        forbidden_v.clear();

        std::vector<int> regs0;

        dex::u2 reg_do_arg = 0;
        // 拿到 reg_return_dst
        if (reg_return_orig < 0) {
            // 清空了的 随机造一个
            LOGW("[doHExit] reg_return_orig 已清空不存在 ... %d",
                 reg_return_orig)
            // 随机选一个
            if (is_wide_reg_return) {
                regs0 = FRegManager::AllocWide(
                        code_ir, forbidden_v, count, "regs0-1");
                CHECK_ALLOC_OR_RET(regs0, count, -1,
                                   "[doHExit] regs0 申请寄存器失败 ...");
                forbidden_v.push_back(regs0[0]);
                forbidden_v.push_back(regs0[0] + 1);
            } else {
                regs0 = FRegManager::AllocV(
                        code_ir, forbidden_v, count, "regs0-2");
                CHECK_ALLOC_OR_RET(regs0, count, -1,
                                   "[doHExit] regs0-2 申请寄存器失败 ...");
                forbidden_v.push_back(regs0[0]);
            }
            reg_do_arg = regs0[0];

        } else {

            // 直接取用原函数的最后一个返回值寄存器
            if (is_wide_reg_return) {
                // 这里直接取第一个宽寄存器
                forbidden_v.push_back(reg_return_orig);
                forbidden_v.push_back(reg_return_orig + 1);
            } else {
                forbidden_v.push_back(reg_return_orig);
            }
            reg_do_arg = reg_return_orig;
        }

        insert_point = code_ir->instructions.end(); // 参数等会用到这个指针
        // --------------------- 1
        // 确保不与宽冲突 创建参数
        fir_funs_do::cre_arr_do_args4onExit(
                code_ir,
                reg_do_arg,
                reg_return_orig,
                is_wide_reg_return,
                insert_point);
        if (reg_do_arg < 0) return -1; // 里面已汇报了错误

        forbidden_v.clear();
        forbidden_v = {reg_do_arg}; // 禁止使用
        if (is_wide_reg_return) {
            forbidden_v.push_back(reg_do_arg + 1);
        }

//            if (!transform->is_app_loader()) { //  调试使用
        if (transform->is_app_loader()) {
            LOGD("[doHExit] 应用侧 调用 isHExit ...")

            // --------------------- 2
            count = 1; // 申请一个宽存
            auto regs8_wide = FRegManager::AllocWide(
                    code_ir, forbidden_v, count, "regs8_wide");
            CHECK_ALLOC_OR_RET(regs8_wide, count, -1,
                               "[doHExit] regs8_wide 申请寄存器失败 ...");

            auto f_THook_onExit = fir_funs_def::get_THook_onExit(code_ir);
            bool res = fir_funs_do::do_THook_onExit(
                    f_THook_onExit, code_ir,
                    reg_do_arg, reg_do_arg, regs8_wide[0],
                    hook_info.j_method_id, insert_point);
            if (!res) return -1;

            reg_return_dst = reg_do_arg;

        } else {
            // 系统侧 搞用
            LOGD("[doHExit] 系统侧 调用 isHExit ...")

            // --------------------- 2 反射 Class 要再包一层
            count = 2;
            auto regs9 = FRegManager::AllocV(
                    code_ir, forbidden_v, count, "regs9");
            CHECK_ALLOC_OR_RET(regs9, count, -1,
                               "[doHExit] regs9 申请寄存器失败 ...");

            // 反射要再包一层  Class[]{Object[].class,Long.class}
            fir_funs_do::cre_arr_object1(
                    code_ir, regs9[0], reg_do_arg, regs9[1],
                    hook_info.j_method_id, insert_point);


            // --------------------- 3
            count = 3;// 前面只有最后的封装参数需要保留
            forbidden_v.clear();
            forbidden_v.push_back(regs9[1]);
            auto regs10 = FRegManager::AllocV(
                    code_ir, forbidden_v, count, "regs10");
            CHECK_ALLOC_OR_RET(regs10, count, -1, "[doHExit] regs10 申请寄存器失败 ...");

            fir_funs_do::cre_arr_class_args4onExit(
                    code_ir, regs10[0], regs10[1], regs10[2],
                    insert_point);


            // --------------------- 4
            count = 3;
            forbidden_v.push_back(regs10[2]);
            auto regs11 = FRegManager::AllocV(
                    code_ir, forbidden_v, count, "regs11");
            CHECK_ALLOC_OR_RET(regs11, count, -1, "[doHExit] regs11 申请寄存器失败 ...");

            bool res = fir_funs_do::do_apploader_static_fun(
                    code_ir, regs11[0], regs11[1], regs11[2],
                    regs10[2], regs9[1], regs11[2],
                    g_name_class_THook, g_name_fun_onExit,
                    insert_point);
            if (!res) return -1;
            reg_return_dst = regs11[2];
        }

        return reg_return_dst;
    }

    /**
            void Transform::Apply(std::shared_ptr<ir::DexFile> dex_ir)
                bool ret = finject::do_finject(this, hook, &code_ir);
     * @param transform 类的hook配置
     * @param hook_info 方法信息
     * @param code_ir  方法的IR实体
     * @return
     */
    bool do_finject(deploy::Transform *transform,
                    const deploy::MethodHooks &hook_info,
                    lir::CodeIr *code_ir) {



        /// 这里是用于调试反向 使用时注释
//        transform->set_app_loader(!transform->is_app_loader());
//        LOGE("[do_finject]  ------ 开启了反向调试  ------ transform->set_app_loader(!transform->is_app_loader())  -----------")

        auto ir_method = code_ir->ir_method;
        // 方法信息
        LOGD("[do_finject] start ... %s -> %s %s ",
             ir_method->decl->parent->Decl().c_str(),
             ir_method->decl->name->c_str(),
             ir_method->decl->prototype->Signature().c_str())

        // 如果是系统侧 需要根据方法信息需要框架是否走备用方案
        bool is_run_backup_plan;
        if (!transform->is_app_loader()) {
            is_run_backup_plan = is_frame_method(ir_method);
        }

        auto orig_return_type = ir_method->decl->prototype->return_type;
        ir::Builder builder(code_ir->dex_ir);

        int reg_return_orig = -1; // 这个主要用于 exit
        int reg_return_dst = -1; // hook后的返回值寄存器编号

        bool is_wide_reg_return = false; // 返回类型是否为宽，用于返回值校验
        if (!hook_info.isRunOrigFun) {
            fir_tools::clear_original_instructions(code_ir);
        } else {
            // 快速查找，原始返回值寄存器编号和类型
            reg_return_orig = fir_tools::find_return_register(code_ir, &is_wide_reg_return);
            LOGI("[do_finject] reg_return_orig= %d, is_wide_reg_return= %d", reg_return_orig,
                 is_wide_reg_return);
        }

        // 清除操作后 确定插入点（MOD: 修复返回点找到逻辑）
        auto insert_point = fir_tools::find_first_code(code_ir);
        dex::u2 num_add_reg = 0;
        dex::u2 num_reg_non_param_new = 0;

        // 没有 hook 方法 不需要申请寄存器
        if (hook_info.isHEnter || hook_info.isHExit) {
            if (transform->is_app_loader()) {
                // 申请 只调一次，提前确认好
                num_add_reg = FRegManager::RequestLocals(code_ir, 4); // 应用侧4个

                // 这是源参数的寄存器索引
                num_reg_non_param_new = FRegManager::Locals(code_ir);
            } else {
                num_add_reg = FRegManager::RequestLocals(code_ir, 5); // 系统侧5个
                num_reg_non_param_new = FRegManager::Locals(code_ir);
            }
        }
        // 如果单清除掉原始方法，也需要申请一个寄存器 cre_return_code 需要1个
        if (!hook_info.isRunOrigFun) {
            num_add_reg = FRegManager::RequestLocals(code_ir, 1);
            num_reg_non_param_new = FRegManager::Locals(code_ir);
        }

        // *********************************************************************

        // ------------------ Enter ------------------
        if (hook_info.isHEnter) {
            bool res = do_HEnter(
                    transform, hook_info, code_ir,
                    insert_point, num_add_reg, num_reg_non_param_new,
                    is_run_backup_plan);

            if (!res)return false;
        }

        // ...... do RunOrigFun ......

        // ------------------ Exit ------------------

        if (hook_info.isHExit) {
            reg_return_dst = doHExit(
                    transform, hook_info, code_ir,
                    insert_point,
                    reg_return_orig,
                    is_wide_reg_return,
                    is_run_backup_plan);
            if (reg_return_dst < 0)return false;
        }

        // 如果不执行原方法 或者运行了 isHExit 就需要 cre_return_code
        if (!hook_info.isRunOrigFun || hook_info.isHExit) {
            LOGD("[do_finject] 添加返回代码 cre_return_code ...")
            // 如果清空了，一定要恢复
            bool res = fir_tools::cre_return_code(
                    code_ir, orig_return_type,
                    reg_return_dst, hook_info.isHExit);
            if (!res) return false;
        }

        return true;
    }
}
