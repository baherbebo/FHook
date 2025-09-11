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
#include "fir_funs_do_impl.h"
#include "freg_manager.h"
#include "freg_allocator.h"
#include "../tools/fsys.h"


namespace finject {
    enum class HookPlan : uint8_t {
        None = 0,
        PlanA,   // 方案 A：ClassLoader + Class.getDeclaredMethod + Method.invoke + Class.loadClass
        PlanB,   // 方案 B：MethodHandles + MethodType + Lookup.findStatic（动态查找静态 MH）
        PlanC,   // 方案 C：Class.forName + 直接取 public 静态字段 MethodHandle（如 MH_ENTER/MH_EXIT）
    };

    static std::unique_ptr<feadre::FRegAllocator> fRegs;


    /**
     * 这是最后的防线
     * 方案 A：ClassLoader + Class.getDeclaredMethod + Method.invoke
     * 方案 B：MethodHandles + MethodType + Lookup.findStatic（动态查找静态 MH）  --- Class.forName
     * 方案 C：Class.forName + 直接取 public 静态字段 MethodHandle（如 MH_ENTER/MH_EXIT）
     * @param m
     * @return
     */
    static inline HookPlan select_plan(ir::EncodedMethod *m) {

        const std::string cls = fsys::norm_cls(m->decl->parent->Decl());
        const char *mn = m->decl->name->c_str();
        const std::string sig = m->decl->prototype->Signature();

        ///  最后的防线 -----------------
        // ---- 规则 2：Class.getDeclaredMethod → 用 None 这个是系统要用一点不能动
        if (cls == "java/lang/Class" &&
            std::strcmp(mn, "getDeclaredMethod") == 0 &&
            sig == "(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;") {
            return HookPlan::None;
        }

        // ---- 跳过桥接/基础“管道”方法：命中直接返回 None
        // Thread.currentThread() → ()Ljava/lang/Thread;
        if (cls == "java/lang/Thread" &&
            std::strcmp(mn, "currentThread") == 0 &&
            sig == "()Ljava/lang/Thread;") {
            return HookPlan::None;
        }

        // Thread.getContextClassLoader() → ()Ljava/lang/ClassLoader;
        if (cls == "java/lang/Thread" &&
            std::strcmp(mn, "getContextClassLoader") == 0 &&
            sig == "()Ljava/lang/ClassLoader;") {
            return HookPlan::None;
        }


        // ---- 规则 X：Class.forName → 用 PlanA  间接调用 ClassLoader.loadClass
        if (cls == "java/lang/Class" &&
            std::strcmp(mn, "forName") == 0 &&
            (sig == "(Ljava/lang/String;)Ljava/lang/Class;" ||  // Class.forName(String)
             sig ==
             "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;")) { // Class.forName(String, boolean, ClassLoader)
            return HookPlan::PlanA;
        }

        // ---- 规则 3：Method.invoke(Object,Object[]) → 这个是native
        // if (cls == "java/lang/reflect/Method" &&
        //     std::strcmp(mn, "invoke") == 0 &&
        //     sig == "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;") {
        //     return HookPlan::PlanA;
        // }

        // ---- 规则 1：ClassLoader.loadClass → 用 PlanB C都可用
        if (cls == "java/lang/ClassLoader" &&
            std::strcmp(mn, "loadClass") == 0 &&
            (sig == "(Ljava/lang/String;)Ljava/lang/Class;" ||
             sig == "(Ljava/lang/String;Z)Ljava/lang/Class;")) {
            return HookPlan::PlanC;
        }

        return HookPlan::PlanC;
    }


    static bool do_HEnter(const deploy::Transform *transform,
                          const deploy::MethodHooks &hook_info,
                          lir::CodeIr *code_ir,
                          slicer::IntrusiveList<lir::Instruction>::Iterator insert_point,
                          unsigned short &num_reg_non_param_new,
                          HookPlan plan) {

        LOGD("[do_HEnter] 运行 isHEnter ......")

        int count;
        dex::u4 reg_do_return = 9999; //必须要处理 执行后的返回值所有的寄存器用于恢复

        // --------------------- 1 参数包装成 object[object arg0,object arg1...]
        count = 2;
        auto regs1 = fRegs.get()->AllocSinglesTail("regs1", count, "regs1");
        CHECK_ALLOC_OR_RET(regs1, count, false, "[do_HEnter] regs1 申请寄存器失败 ...");

        count = 2;
        auto regs_arr1 = fRegs.get()->GetRegs22c4Count(count, "regs_arr1");
        CHECK_ALLOC_OR_RET(regs_arr1, count, false, "[do_HEnter] regs_arr1 申请寄存器失败 ...");

        // 这里参数来源 自动读 p指针 object[arg0,arg1...]
        fir_impl::cre_arr_do_args4onEnter(
                code_ir,
                regs_arr1[0], regs1[0], regs_arr1[1],
                regs1[1],
                insert_point);
        auto reg_do_args = regs1[1]; // Object[]

        if (transform->is_app_loader()) {
            LOGD("[do_HEnter] 应用侧 调用 isHEnter ...")

            count = 1; // 后申请宽，保持连续
            auto regs2_wide = fRegs.get()->AllocWidesTail(
                    "regs2_wide", count, "regs2_wide");
            CHECK_ALLOC_OR_RET(regs2_wide, count, false,
                               "[do_HEnter] regs2_wide 申请寄存器失败 ...");

            // ---------------------  直接执行 do_frame_hfun 这个没得返回 改了的参数在 这里要确保连续
            count = 1;
            auto regs3 = fRegs.get()->AllocSinglesTail(
                    "regs3", count, "regs3");
            CHECK_ALLOC_OR_RET(regs3, count, false, "[do_HEnter] regs3 申请寄存器失败 ...");

            // 拿到 运行方法
            auto f_THook_onEnter = fir_funs_def::get_THook_onEnter(code_ir);
            bool res = fir_funs_do::do_frame_hfun(
                    f_THook_onEnter, code_ir,
                    reg_do_args, regs2_wide[0], regs3[0],
                    hook_info.j_method_id, insert_point);
            if (!res)return false;

            reg_do_return = regs3[0];

        } else {
            // 系统侧 调用
            LOGD("[do_HEnter] 系统侧 调用 isHEnter ...")

            // reg_do_args  Object[arg0,arg1...]
            // --------------------- 1
            count = 1;
            auto regs5 = fRegs.get()->AllocSinglesTail("regs5", count, "regs5");
            CHECK_ALLOC_OR_RET(regs5, count, false, "[do_HEnter] regs5 申请寄存器失败 ...");

            count = 2;
            // 前两个第一个可以复用,第二个不能用,这里要用第三个, 这里多少个需要手动管理 前面的要申请
            auto regs_arr5_1 = fRegs.get()->GetRegs22c4All("regs_arr5_1");
            CHECK_ALLOC_OR_RET(regs_arr5_1, count, false,
                               "[do_HEnter] regs_arr5_1 申请寄存器失败 ...");

            // public final static Object[] onEnter4fhook(Object[] rawArgs, long methodId)
            /// Object[Object[arg0,arg1...],Long] 再包一层对象 reg_do_args 完成
            // 这里 reg_do_args 很可能 == reg3_arr_22c 所以要一个新的 GetRegs22c4All 所以要申请新增时就要至少3个
            fir_funs_do::cre_arr_do_arg_2p(code_ir,
                                           regs_arr1[0], reg_do_args,
                                           regs_arr5_1[2],
                                           regs5[0], hook_info.j_method_id, insert_point);
            reg_do_args = regs5[0]; // Object[ Object[arg0,arg1...],Long]

            // --------------------- 3
            switch (plan) {
                case HookPlan::PlanA: {
                    LOGI("[do_HEnter] 运行方案A ...")

                    // --------------------- 2
                    count = 2;
                    auto regs6 = fRegs.get()->AllocSinglesTail(
                            "regs6", count, "regs6");
                    CHECK_ALLOC_OR_RET(regs6, count, false, "[do_HEnter] regs6 申请寄存器失败 ...");

                    // 这里可以复用了 22C
                    count = 2;
                    auto regs_arr6_1 = fRegs.get()->GetRegs22c4Count(
                            count, "regs_arr6_1");
                    CHECK_ALLOC_OR_RET(regs_arr6_1, count, false,
                                       "[do_HEnter] regs_arr6_1 申请寄存器失败 ...");

                    // 调用 onEnter 函数
                    // 创建参数 Class[] 在 v2 （Class[]{Object[].class,Long.class}）
                    std::string name_class_arg = "[Ljava/lang/Object;";  // 普通obj
                    fir_impl::cre_arr_class_args4frame(
                            code_ir,
                            regs_arr6_1[0], regs6[0],
                            regs_arr6_1[1], regs6[1],
                            name_class_arg, insert_point);
                    auto reg_method_args = regs6[1];

                    count = 3;
                    auto regs7 = fRegs.get()->AllocSinglesTail(
                            "regs7", count, "regs7");
                    CHECK_ALLOC_OR_RET(regs7, count, false, "[do_HEnter] regs7 申请寄存器失败 ...");

                    // 执行结果返回到 v0`
                    bool res = fir_impl::do_sysloader_hook_funs_A(
                            code_ir, regs7[0], regs7[1], regs7[2],
                            reg_method_args, reg_do_args,
                            regs7[0],
                            g_name_class_THook, g_name_fun_onEnter,
                            insert_point);
                    if (!res)return false;

                    reg_do_return = regs7[0];

                    break;
                }

                case HookPlan::PlanB: {
                    LOGI("[do_HEnter] 运行B方案 ...")

                    count = 4;
                    auto regs8 = fRegs.get()->AllocSinglesTail(
                            "regs8", count, "regs8");
                    CHECK_ALLOC_OR_RET(regs8, count, false, "[do_HEnter] regs8 申请寄存器失败 ...");
                    std::string name_class_arg = "[Ljava/lang/Object;";
                    std::string rtype_name = "[Ljava/lang/Object;";

                    count = 2;
                    auto regs_arr8_1 = fRegs.get()->GetRegs22c4Count(count, "regs8");
                    CHECK_ALLOC_OR_RET(regs_arr8_1, count, false,
                                       "[do_HEnter] regs_arr8_1 申请寄存器失败 ...");

                    bool res = fir_impl::do_sysloader_hook_funs_B(
                            code_ir, regs8[0], regs8[1], regs8[2], regs8[3],
                            regs_arr8_1[0], regs_arr8_1[1],
                            reg_do_args, regs8[0],
                            g_name_class_THook, g_name_fun_onEnter,
                            name_class_arg, rtype_name, insert_point);

                    if (!res)return false;

                    reg_do_return = regs8[0];

                    break;
                }


                case HookPlan::PlanC: {
                    if (gIsDebug)LOGI("[do_HEnter] 运行C方案 ...")

                    count = 4;
                    auto regs9 = fRegs.get()->AllocSinglesTail(
                            "regs9", count, "regs9");
                    CHECK_ALLOC_OR_RET(regs9, count, false, "[do_HEnter] regs9 申请寄存器失败 ...");

                    bool res = fir_impl::do_sysloader_hook_funs_C(
                            code_ir, regs9[0], regs9[1], regs9[2], regs9[3],
                            reg_do_args, regs9[0],
                            g_name_class_THook, g_name_fun_MH_ENTER, insert_point);

                    if (!res)return false;

                    reg_do_return = regs9[0];

                    break;
                }

                case HookPlan::None:
                default:
                    break;
            }
        }

        // --------------------- 3 恢复类型和位置

        // 没有执行原始方法 则不需要恢复参数类型
        if (hook_info.isRunOrigFun) {
            count = 2;
            auto regs4 = fRegs.get()->AllocSinglesTail(
                    "regs4", count, "regs4");
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

        return true;
    }

    /// 当 需要运行原始方法时，并申请了寄存器时 一定要恢复参数位置
    int doHExit(const deploy::Transform *transform,
                const deploy::MethodHooks &hook_info,
                lir::CodeIr *code_ir,
                slicer::IntrusiveList<lir::Instruction>::Iterator insert_point,
                int &reg_return_orig, // 原方法所在的最后一个返回值所在寄存器 清空了这里是 -1
                bool &is_wide_reg_return,
                HookPlan plan) {

        LOGD("[doHExit] 运行 isHExit ......")
        int reg_return_dst = -1;  // 外把最终处理后的结果 返回值

        // 申请一个统一可用的返回寄存器 根据返回寄存器类型
        // 如果需要运行原方法才进行统一出口， reg_return_orig 肯定不会是 -1，不处理
        if (hook_info.isRunOrigFun) {
            // 原方法返回值处加工， reg_return_orig 里存的返回值原方法，不能动 这里 reg_return_orig 可能是宽寄存器
            fir_tools::instrument_with_epilogue(code_ir, reg_return_orig);
        }

        int count;

        // --- 确保返回寄存器不被其它函数占用  把他转成object, 如果清空了，就创建一个空对象 null
        count = 1; // 申请一个宽存
        std::vector<int> regs0;

        LOGI("[doHExit] 原最后一个返回值的寄存器 reg_return_orig= %d", reg_return_orig)

        // 如果有返回值 则锁定 没有就不管
        if (reg_return_orig >= 0) {
            // 直接取用原函数的最后一个返回值寄存器  锁住
            if (is_wide_reg_return) {
                fRegs.get()->AddGlobalForbid({reg_return_orig, reg_return_orig + 1});
            } else {
                fRegs.get()->AddGlobalForbid({reg_return_orig});
            }
        }

        insert_point = code_ir->instructions.end(); // 参数等会用到这个指针

        // --------------------- 1
        count = 1;
        auto regs0_1 = fRegs.get()->AllocSinglesTail(
                "regs0_1", count, "regs0_1");
        CHECK_ALLOC_OR_RET(regs0_1, count, -1,
                           "[doHExit] regs0_1 申请寄存器失败 ...");

        // 原返回值包装成 object
        fir_impl::cre_do_arg_2obj4onExit(
                code_ir,
                reg_return_orig,
                regs0_1[0],
                insert_point);

        dex::u2 reg_do_arg = regs0_1[0];

        // --- 完成：reg_do_arg 是把返回值转成了 Object对象

        /// 开发调试  -------------------
//        debug_log_2p(code_ir, forbidden_v, insert_point);

        if (transform->is_app_loader()) {
            LOGD("[doHExit] 应用侧 调用 isHExit ...")

            // --------------------- 2
            count = 1; // 申请一个宽存  这里要和 regs0_1 连续
            auto regs8_wide = fRegs.get()->AllocWidesTail(
                    "regs8_wide", count, "regs8_wide");
            CHECK_ALLOC_OR_RET(regs8_wide, count, -1,
                               "[doHExit] regs8_wide 申请宽寄存器失败 ...");

            auto f_THook_onExit = fir_funs_def::get_THook_onExit(code_ir);
            // 这里出来
//            bool res = fir_funs_do::do_THook_onExit(
            bool res = fir_funs_do::do_frame_hfun(
                    f_THook_onExit, code_ir,
                    reg_do_arg, regs8_wide[0], reg_do_arg,
                    hook_info.j_method_id, insert_point);
            if (!res) return -1;

            reg_return_dst = reg_do_arg;

        } else {
            // 系统侧 调用 --------------------------
            LOGD("[doHExit] 系统侧 调用 isHExit ...")

            // --------------------- 2 do 参数打包 Object[Object,Long]
            count = 1;
            auto regs9 = fRegs.get()->AllocSinglesTail(
                    "regs9", count, "regs9");
            CHECK_ALLOC_OR_RET(regs9, count, -1,
                               "[doHExit] regs9 申请寄存器失败 ...");

            count = 2; // 原参数不是数组这里只用到 一组
            auto regs_arr9_1 = fRegs.get()->GetRegs22c4Count(count, "regs_arr9_1");
            CHECK_ALLOC_OR_RET(regs_arr9_1, count, false,
                               "[doHExit] regs_arr9_1 申请22c寄存器失败 ...");

            // 反射要再包一层  Class[]{Object.class,Long.class}
            fir_funs_do::cre_arr_do_arg_2p(
                    code_ir, regs_arr9_1[0], reg_do_arg,
                    regs_arr9_1[1], regs9[0],
                    hook_info.j_method_id, insert_point);

            auto reg_do_args = regs9[0]; //  reg_do_arg -》 reg_do_args

            // --------------------- 4
            switch (plan) {
                case HookPlan::PlanA: {
                    LOGI("[doHExit] 运行A方案 ...")

                    // --------------------- 3
                    count = 2;// 前面只有最后的封装参数需要保留

                    auto regs10 = fRegs.get()->AllocSinglesTail("regs10", count);
                    CHECK_ALLOC_OR_RET(regs10, count, -1, "[doHExit] regs10 申请寄存器失败 ...");

                    count = 2;
                    auto regs_arr10_1 = fRegs.get()->GetRegs22c4Count(count, "regs_arr10_1");
                    CHECK_ALLOC_OR_RET(regs_arr10_1, count, false,
                                       "[doHExit] regs_arr10_1 申请22c寄存器失败 ...");

                    // Class[]{Object.class,Long.class}
                    std::string name_class_arg = "Ljava/lang/Object;";  // 普通obj
                    fir_impl::cre_arr_class_args4frame(
                            code_ir, regs_arr10_1[0],
                            regs10[0], regs_arr10_1[1], regs10[1],
                            name_class_arg,
                            insert_point);
                    auto reg_method_args = regs10[1];


                    count = 3;
                    auto regs11 = fRegs.get()->AllocSinglesTail(
                            "regs11", count, "regs11");
                    CHECK_ALLOC_OR_RET(regs11, count, -1, "[doHExit] regs11 申请寄存器失败 ...");

                    bool res = fir_impl::do_sysloader_hook_funs_A(
                            code_ir, regs11[0], regs11[1], regs11[2],
                            reg_method_args, reg_do_args, regs11[2],
                            g_name_class_THook, g_name_fun_onExit,
                            insert_point);
                    if (!res) return -1;
                    reg_return_dst = regs11[2];

                    break;

                }
                case HookPlan::PlanB: {
                    LOGI("[doHExit] 运行B方案 ...")

                    count = 4;
                    auto regs8 = fRegs.get()->AllocSinglesTail(
                            "regs8", count, "regs8");
                    CHECK_ALLOC_OR_RET(regs8, count, false, "[doHExit] regs8 申请寄存器失败 ...");

                    std::string name_class_arg = "Ljava/lang/Object;";
                    std::string rtype_name = "Ljava/lang/Object;";

                    count = 2;
                    auto regs_arr8_1 = fRegs.get()->GetRegs22c4Count(count, "regs_arr8_1");
                    CHECK_ALLOC_OR_RET(regs_arr8_1, count, false,
                                       "[doHExit] regs_arr8_1 申请寄存器失败 ...");

                    bool res = fir_impl::do_sysloader_hook_funs_B(
                            code_ir, regs8[0], regs8[1], regs8[2], regs8[3],
                            regs_arr8_1[0], regs_arr8_1[1],
                            reg_do_args, regs8[0],
                            g_name_class_THook, g_name_fun_onExit,
                            name_class_arg, rtype_name,
                            insert_point);


                    if (!res) return -1;
                    reg_return_dst = regs8[0];

                    break;

                }
                case HookPlan::PlanC: {
                    if (gIsDebug)LOGI("[doHExit] 运行C方案 ...")

                    count = 4;
                    auto regs9 = fRegs.get()->AllocSinglesTail(
                            "regs9", count, "regs9");
                    CHECK_ALLOC_OR_RET(regs9, count, false, "[doHExit] regs9 申请寄存器失败 ...");

                    bool res = fir_impl::do_sysloader_hook_funs_C(
                            code_ir, regs9[0], regs9[1], regs9[2], regs9[3],
                            reg_do_args, regs9[0],
                            g_name_class_THook, g_name_fun_MH_EXIT, insert_point);

                    if (!res) return -1;
                    reg_return_dst = regs9[0];

                    break;

                }
            }

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
                    deploy::MethodHooks &hook_info,
                    lir::CodeIr *code_ir) {

        /// 开发调试 清空方法切换
//        hook_info.isRunOrigFun = false;
//        LOGE("[do_finject]  ------ 开启了反向调试  ------ isRunOrigFun= %d", hook_info.isRunOrigFun)

        /// 开发调试 应用侧切换
//        transform->set_app_loader(!transform->is_app_loader());
//        LOGE("[do_finject]  ------ 开启了反向调试  ------ transform->set_app_loader(!transform->is_app_loader())  -----------")

        auto ir_method = code_ir->ir_method;
        // 方法信息
        LOGD("[do_finject] start ... %s -> %s %s ",
             ir_method->decl->parent->Decl().c_str(),
             ir_method->decl->name->c_str(),
             ir_method->decl->prototype->Signature().c_str())

        // ---- 如果是系统侧 需要根据方法信息需要框架是否走备用方案
        HookPlan plan = HookPlan::None;
        if (!transform->is_app_loader()) {
            plan = select_plan(ir_method);
            if (plan == HookPlan::None) {
                LOGE("[do_finject] 该方法框架不允许 hook", plan)
                return false;
            }
            LOGI("[do_finject] select_plan plan= %d", plan)
        }

        auto orig_return_type = ir_method->decl->prototype->return_type;
        ir::Builder builder(code_ir->dex_ir);

        int reg_return_orig = -1; // 缓存原有返回的寄存器，可能有多个， 用于exit 这个在原本地寄存器中
        int reg_return_dst = -1; // hook后的返回值寄存器编号 统一出口

        // ---- 拿到返回值的类型 如果不运行原有方法则清空不需要
        bool is_wide_reg_return = false; // 返回类型是否为宽，用于返回值校验
        if (!hook_info.isRunOrigFun) {
            fir_tools::clear_original_instructions(code_ir);
        } else {
            // 快速查找，原始返回值寄存器编号和类型 最后一个
            reg_return_orig = fir_tools::find_return_register(
                    code_ir, &is_wide_reg_return);
            LOGI("[do_finject] reg_return_orig= %d, is_wide_reg_return= %d", reg_return_orig,
                 is_wide_reg_return);
        }

        // 定位到方法开头
        auto insert_point = fir_tools::find_first_code(code_ir);
        dex::u2 num_add_reg = 0; // 这个用于位移恢复参数
        dex::u2 num_reg_non_param_new = 0;

        fRegs = std::make_unique<feadre::FRegAllocator>(code_ir); // 初始化寄存管理器

        /// 没有 hook 方法 不需要  申请寄存器 ****
        if (hook_info.isHEnter || hook_info.isHExit) {

            auto ins_count = fRegs.get()->InsCount();
            // 普通方法  --- 7 个 +  2个22c
//            int num_reg_non_param_orig = fRegs.get()->GrowLocals(
//                    7 + ins_count + 2, 2);  // 3个 22c 支持2组连续使用

            // 系统该方法   --- 8 个 +  3个22c
            int num_reg_non_param_orig = fRegs.get()->GrowLocals(
                    9 + ins_count + 2, 3);  // 3个 22c 支持2组连续使用
            fRegs.get()->LockOldBase(num_reg_non_param_orig); // 锁定旧寄存器
            num_reg_non_param_new = fRegs.get()->Locals();
            num_add_reg = num_reg_non_param_new - num_reg_non_param_orig;

        }

        // 如果单清除掉原始方法，也需要申请一个寄存器 用于 cre_return_code 需要1个 如果前面
        if (!hook_info.isRunOrigFun && num_add_reg == 0) {
            // 这里不需要 22c 寄存器
            num_add_reg = fRegs.get()->EnsureLocals(1); //这里不需要锁定
            num_reg_non_param_new = fRegs.get()->Locals();
        }

        // *********************************************************************

        // --- 只要申请了寄存器，同时要运行原方法，必须需要把参数移回原位
        if (num_add_reg > 0 && hook_info.isRunOrigFun) {
            LOGD("[do_finject] 恢复寄存器位置 ...")
            //扩展了几个寄存器 = 需要多少本地寄存器 - 原有多少个本地寄存器
            fir_tools::restore_reg_params4shift(code_ir, insert_point, num_add_reg);
        }


        /// 开发调试 用于测试恢复函数
//        LOGE("[do_finject] 开启了反向调试 强制清函数 ... ")
//        fir_tools::clear_original_instructions(code_ir);
//        hook_info.isRunOrigFun = false;
//        insert_point = fir_tools::find_first_code(code_ir);
//        fir_tools::restore_reg_params4shift(code_ir, insert_point, num_add_reg);


        // ------------------ Enter ------------------
        if (hook_info.isHEnter) {
            bool res = do_HEnter(
                    transform, hook_info, code_ir,
                    insert_point,
                    num_reg_non_param_new,
                    plan);

            if (!res)return false;
        }

        if (num_add_reg > 0 && hook_info.isRunOrigFun && hook_info.isHEnter) {
            LOGD("[do_finject] 恢复寄存器位置 ...")
            //扩展了几个寄存器 = 需要多少本地寄存器 - 原有多少个本地寄存器
            fir_tools::restore_reg_params4shift(code_ir, insert_point, num_add_reg);
        }

        // ...... do RunOrigFun ......

        // ------------------ Exit ------------------

        if (hook_info.isHExit) {
            fRegs.get()->ClearAllUsed();
            reg_return_dst = doHExit(
                    transform, hook_info, code_ir,
                    insert_point,
                    reg_return_orig,
                    is_wide_reg_return,
                    plan);
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

    static bool debug_log_2p(lir::CodeIr *code_ir,
                             std::vector<int> &forbidden_v,
                             slicer::IntrusiveList<lir::Instruction>::Iterator insert_point) {
        auto ir_method = code_ir->ir_method;

        lir::Method *f_Log_d = fir_funs_def::get_log_method(code_ir);
        std::string _text = "[finject] C++ ";
        _text += ir_method->decl->parent->Decl().c_str();
        _text += " ";
        _text += ir_method->decl->name->c_str();
        _text += " ";
        _text += ir_method->decl->prototype->Signature().c_str();
        _text += " ------";
        std::string tag = "Feadre_fjtik";

        int count = 2;
        auto regsx = FRegManager::AllocV(
                code_ir, forbidden_v, count, "regsx");

        CHECK_ALLOC_OR_RET(regsx, count, false, "[finject] regsx 申请寄存器失败 ...");
        auto reg_tag = regsx[0];
        auto reg_text = regsx[1];

        fir_funs_do::do_Log_d(f_Log_d, code_ir, reg_tag, reg_text,
                              tag, _text, insert_point);
        return true;
    }
}
