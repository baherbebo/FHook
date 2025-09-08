//
// Created by Administrator on 2025/9/2.
//
#include "../FGlobal_flib.h"

#define TAG FEADRE_TAG("fir_funs_do")

#include "fir_funs_do.h"
#include "fir_tools.h"
#include "fir_funs_def.h"
#include "freg_manager.h"

namespace fir_funs_do {
    /** ----------------- 静态函数区 ------------------- */
    void do_static_0p(lir::Method *method,
                      lir::CodeIr *code_ir,
                      dex::u2 reg_return,
                      slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {
        // 调用 MethodHandles.publicLookup()
        auto call = code_ir->Alloc<lir::Bytecode>();
        call->opcode = dex::OP_INVOKE_STATIC;
        auto regs = code_ir->Alloc<lir::VRegList>();
        regs->registers.clear();                   // 无参数

        call->operands.push_back(regs);            // 先参数
        call->operands.push_back(method); // 再方法
        code_ir->instructions.insert(insert_point, call);

        // 把返回 move-result-object v0
        auto move_res = code_ir->Alloc<lir::Bytecode>();
        move_res->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_res->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));
        code_ir->instructions.insert(insert_point, move_res);
    }


    void do_static_1p(lir::Method *method,
                      lir::CodeIr *code_ir,
                      dex::u2 reg1_tmp,
                      dex::u2 reg_return,
                      slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        auto call = code_ir->Alloc<lir::Bytecode>();
        call->opcode = dex::OP_INVOKE_STATIC; // 执行方法
        auto regs = code_ir->Alloc<lir::VRegList>();
        regs->registers.clear();
        regs->registers.push_back(reg1_tmp); // 添加参数

        call->operands.push_back(regs); // 先参数
        call->operands.push_back(method);// 再方法
        code_ir->instructions.insert(insert_point, call);

        // 把返回值弄到v0 ActivityThread 类到 v0 move-result-object v0
        auto move_res = code_ir->Alloc<lir::Bytecode>();
        move_res->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_res->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));
        code_ir->instructions.insert(insert_point, move_res);
    }


    /// 执行 Class<?> activityThreadClass = Class.forName("android.app.ActivityThread");
    void do_Class_forName_1p(
            lir::Method *f_Class_forName,
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp,
            dex::u2 reg_return, // 可以相同
            std::string &name_class,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);

        // const-string v0 "xxx"
        auto at_cls_str = builder.GetAsciiString(name_class.c_str());
        auto const_at_cls = code_ir->Alloc<lir::Bytecode>();
        const_at_cls->opcode = dex::OP_CONST_STRING;
        const_at_cls->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp)); // v0 = 类名
        const_at_cls->operands.push_back(
                code_ir->Alloc<lir::String>(at_cls_str, at_cls_str->orig_index));
        code_ir->instructions.insert(insert_point, const_at_cls);

        do_static_1p(f_Class_forName, code_ir, reg1_tmp, reg_return, insert_point);

    }


    bool do_THook_onExit(
            lir::Method *f_THook_onExit,
            lir::CodeIr *code_ir,
            int reg1_arg,  // onExit object 的参数寄存器（如 v4）
            int reg_return, // 可以相同 onExit 返回值 object
            int reg2_tmp_long, // 宽寄存器
            uint64_t method_id, // uint64_t
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);
        auto *ir_method = code_ir->ir_method;

        // 写入 method_id 到 {reg2_tmp_long, reg2_tmp_long+1}  const-wide v<reg_id_lo>, 123456L
        {
            auto cst = code_ir->Alloc<lir::Bytecode>();
            cst->opcode = dex::OP_CONST_WIDE; // 写入 v<reg_id_lo> 与 v<reg_id_lo+1>
            cst->operands.push_back(code_ir->Alloc<lir::VRegPair>(reg2_tmp_long));
            cst->operands.push_back(code_ir->Alloc<lir::Const64>(static_cast<uint64_t>(method_id)));
            code_ir->instructions.insert(insert_point, cst);
        }

        // 构建 invoke-static 指令：操作数顺序必须是 [VRegList, Method]
        auto invoke = code_ir->Alloc<lir::Bytecode>();
        invoke->opcode = dex::OP_INVOKE_STATIC;
        lir::VRegList *regs = code_ir->Alloc<lir::VRegList>();
        regs->registers.clear();
        regs->registers.push_back(reg1_arg);  // 将单个寄存器 reg1_arg 加入列表
        regs->registers.push_back(reg2_tmp_long);
        regs->registers.push_back(reg2_tmp_long + 1);

        invoke->operands.push_back(regs);  // 第一个操作数：VRegList（参数列表）
        invoke->operands.push_back(f_THook_onExit);  // 第二个操作数：方法引用（THook.onExit）
        code_ir->instructions.insert(insert_point, invoke);

        // 3. 处理返回值：move-result-object 指令（接收 onExit 的返回值，存入 reg1_arg）
        auto move_res = code_ir->Alloc<lir::Bytecode>();
        move_res->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_res->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));  // 接收返回值到 reg1_arg
        code_ir->instructions.insert(insert_point, move_res);

        {
            // 恢复避免后续恢复 这个完了直接返回了，但还有有可能被使用
            // 先清高半 v+1
            fir_tools::emitValToReg(code_ir, insert_point, reg2_tmp_long + 1, 0);
            // 再清低半 v
            fir_tools::emitValToReg(code_ir, insert_point, reg2_tmp_long, 0);
        }

        return true;
    }


    ///  LOG.d("Feadre_fjtik", "xxx") 这个没有返回值
    void do_Log_d(
            lir::Method *f_Log_d,
            lir::CodeIr *code_ir,
            dex::u2 reg1, dex::u2 reg2,
            std::string &tag,
            std::string &text,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {
        ir::Builder builder(code_ir->dex_ir);

        // ========================= 1. const-string v0 "xxx"   String tag = "Feadre_fjtik" =========================
        auto const_tag = code_ir->Alloc<lir::Bytecode>();
        const_tag->opcode = dex::OP_CONST_STRING;
        const_tag->operands.push_back(code_ir->Alloc<lir::VReg>(reg1));
        auto tag_str1 = builder.GetAsciiString(tag.c_str());
        const_tag->operands.push_back(code_ir->Alloc<lir::String>(tag_str1, tag_str1->orig_index));
        code_ir->instructions.insert(insert_point, const_tag);

        // ========================= 2. const-string v1  "xxx" =========================
        auto const_msg = code_ir->Alloc<lir::Bytecode>();
        const_msg->opcode = dex::OP_CONST_STRING;
        const_msg->operands.push_back(code_ir->Alloc<lir::VReg>(reg2)); // 目标寄存器 v1
        auto tag_str2 = builder.GetAsciiString(text.c_str());
        const_msg->operands.push_back(
                code_ir->Alloc<lir::String>(tag_str2, tag_str2->orig_index)); // 绑到 const_msg！
        code_ir->instructions.insert(insert_point, const_msg);



        // 3.3 构建 invoke-static 指令（操作数顺序：方法 → 寄存器列表）
        auto invoke = code_ir->Alloc<lir::Bytecode>();
        invoke->opcode = dex::OP_INVOKE_STATIC;
        auto regs = code_ir->Alloc<lir::VRegList>();
        regs->registers.clear();
        regs->registers.push_back(reg1);
        regs->registers.push_back(reg2);

        invoke->operands.push_back(regs);   // 先参数
        invoke->operands.push_back(f_Log_d); // 再方法
        code_ir->instructions.insert(insert_point, invoke);
        // 没有处理 返回值 ****************
    }


    /// MethodType mtEnter = MethodType.methodType(Object[].class, Object[].class, long.class);
    void do_MethodType_methodType_2p(
            lir::Method *f_MethodType_methodType,
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp,
            dex::u2 reg_method_args, // ptypes = new Class[2]; ptypes[0]=Object[].class; ptypes[1]=Long.TYPE;
            dex::u2 reg_return, // 可以重复
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        // 创建 rtype = Object[].class
        fir_tools::emitReference2Class(code_ir, "[Ljava/lang/Object;",
                                       reg1_tmp, insert_point);

        // --- invoke-static {reg1, reg2, reg3}, MethodType.methodType(Class, Class, Class)
        auto invoke = code_ir->Alloc<lir::Bytecode>();
        invoke->opcode = dex::OP_INVOKE_STATIC;

        auto regs = code_ir->Alloc<lir::VRegList>();
        regs->registers.clear();
        regs->registers.push_back(reg1_tmp);  // rtype: Class
        regs->registers.push_back(reg_method_args);  // p1   : Class

        invoke->operands.push_back(regs);
        invoke->operands.push_back(f_MethodType_methodType);
        code_ir->instructions.insert(insert_point, invoke);

        // --- move-result-object v<reg_return>  // MethodType
        auto mres = code_ir->Alloc<lir::Bytecode>();
        mres->opcode = dex::OP_MOVE_RESULT_OBJECT;
        mres->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));
        code_ir->instructions.insert(insert_point, mres);
    }


    /**
     * public Method getDeclaredMethod(String name, Class<?>... parameterTypes)
     * Method m = clazz.getDeclaredMethod("onEnter", new Class[]{Object[].class});
     * ------ 这个目前只能找到无参数的方法
     * @param f_class_getDeclaredMethod
     * @param code_ir
     * @param builder
     * @param reg_class 需要一个class对象
     * @param reg1_tmp 装 name_fun 的寄存器
     * @param reg_class_arr 装 parameterTypes 的寄存器
     * @param reg_return
     * @param name_fun
     */
    void do_class_getDeclaredMethod(
            lir::Method *f_class_getDeclaredMethod,
            lir::CodeIr *code_ir,
            dex::u2 reg_class, // class对象
            dex::u2 reg1_tmp, // 方法名
            dex::u2 reg_class_arr, // 方法参数
            dex::u2 reg_return,
            std::string &name_fun,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);

        // reg1_tmp 放入方法名
        auto const_currentApp = builder.GetAsciiString(name_fun.c_str());
        auto const_ca_str = code_ir->Alloc<lir::Bytecode>();
        const_ca_str->opcode = dex::OP_CONST_STRING;
        const_ca_str->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp));
        const_ca_str->operands.push_back(code_ir->Alloc<lir::String>(
                const_currentApp, const_currentApp->orig_index));
        code_ir->instructions.insert(insert_point, const_ca_str);


        // 调用 getDeclaredMethod("currentApplication", null)
        auto call_getMethod_ca = code_ir->Alloc<lir::Bytecode>();
        call_getMethod_ca->opcode = dex::OP_INVOKE_VIRTUAL;
        auto reg_getMethod_ca = code_ir->Alloc<lir::VRegList>();
//        reg_getMethod_ca->registers = {0, 1, 2}; // v0=ActivityThread类, v1=方法名, v2=null
        reg_getMethod_ca->registers.clear();
        reg_getMethod_ca->registers.push_back(reg_class);
        reg_getMethod_ca->registers.push_back(reg1_tmp);
        reg_getMethod_ca->registers.push_back(reg_class_arr);
        call_getMethod_ca->operands.push_back(reg_getMethod_ca);
        call_getMethod_ca->operands.push_back(f_class_getDeclaredMethod);
        code_ir->instructions.insert(insert_point, call_getMethod_ca);

        // 保存方法到 v1
        auto move_method_ca = code_ir->Alloc<lir::Bytecode>();
        move_method_ca->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_method_ca->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));
        code_ir->instructions.insert(insert_point, move_method_ca);
    }


    /**
     * 这个用了 reg_tmp_long 是恢复了的
     * @param f_THook_onEnter
     * @param code_ir
     * @param reg_arg
     * @param reg_tmp_long
     * @param reg_return
     * @param method_id
     * @param insert_point
     */
    bool do_THook_onEnter(
            lir::Method *f_THook_onEnter,
            lir::CodeIr *code_ir,
            dex::u2 reg_arg, // Object[]
            dex::u2 reg_tmp_long, // long 必须宽寄存器
            dex::u2 reg_return, // 不能重叠 原因是没有tmp 寄存器（reg2_tmp_long 这个是宽）
            uint64_t method_id, // uint64_t
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point
    ) {
        ir::Builder builder(code_ir->dex_ir);
        auto *ir_method = code_ir->ir_method;
        auto *code = ir_method->code;
        const dex::u2 regs_size = code->registers;

        // 校验 范围 + 宽寄存器 + 不重叠
        {
            // 超界检测
            auto in_range = [&](int v) { return v >= 0 && (dex::u2) v < regs_size; };
            if (!in_range(reg_arg) || !in_range(reg_return) ||
                !in_range(reg_tmp_long) || !in_range(reg_tmp_long + 1)) {
                LOGE("[do_THook_onEnter] reg 越界: regs=%u, reg_arg=%d, reg_return=%d, reg_tmp_long={%u,%u}",
                     regs_size, reg_arg, reg_return, reg_tmp_long, reg_tmp_long + 1);
                return false;
            }
            // 宽越界检测
            if (reg_tmp_long + 1 >= regs_size) {
                LOGE("[do_THook_onEnter] reg_tmp_long hi 越界: %u/%u", reg_tmp_long + 1,
                     regs_size);
                return false;
            }
            // 禁止 {reg_tmp_long, reg_tmp_long+1} 与 reg_arg/reg_return 重叠
            if (reg_arg == (int) reg_tmp_long || reg_arg == (int) reg_tmp_long + 1 ||
                reg_return == (int) reg_tmp_long ||
                reg_return == (int) reg_tmp_long + 1) {
                LOGE("[do_THook_onEnter] 重叠:  reg_arg=%d, reg_return=%d, reg_tmp_long={%u,%u}",
                     reg_arg, reg_return, reg_tmp_long, reg_tmp_long + 1);
                return false;
            }
        }

        // 1) const-wide v<reg_id_lo>, 123456L
        auto cst = code_ir->Alloc<lir::Bytecode>();
        cst->opcode = dex::OP_CONST_WIDE; // 写入 v<reg_id_lo> 与 v<reg_id_lo+1>
        cst->operands.push_back(code_ir->Alloc<lir::VRegPair>(reg_tmp_long));
        cst->operands.push_back(code_ir->Alloc<lir::Const64>(static_cast<uint64_t>(method_id)));
        code_ir->instructions.insert(insert_point, cst);

        // 构建 invoke-static（void，无参数）
        auto invoke = code_ir->Alloc<lir::Bytecode>();
        invoke->opcode = dex::OP_INVOKE_STATIC;

        auto regs = code_ir->Alloc<lir::VRegList>();
        regs->registers.clear();
        regs->registers.push_back(reg_arg);   // 传入 Object[] 实参
        regs->registers.push_back(reg_tmp_long);
        regs->registers.push_back(reg_tmp_long + 1);

        invoke->operands.push_back(regs);                // 先参数列表
        invoke->operands.push_back(f_THook_onEnter);     // 再方法引用
        code_ir->instructions.insert(insert_point, invoke);

        // move-result-object v<reg_arg>
        auto mres = code_ir->Alloc<lir::Bytecode>();
        mres->opcode = dex::OP_MOVE_RESULT_OBJECT;
        mres->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));
        code_ir->instructions.insert(insert_point, mres);

        {
            // long 临时必须恢复宽寄存器
            // 先清高半 v+1
            fir_tools::emitValToReg(code_ir, insert_point, reg_tmp_long + 1, 0);
            // 再清低半 v
            fir_tools::emitValToReg(code_ir, insert_point, reg_tmp_long, 0);
        }

        return true;
    }


    /// Class.forName("xxxxx", true, ClassLoader.getSystemClassLoader)
    void do_Class_forName_3p(
            lir::Method *f_Class_forName4loader,
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp, dex::u2 reg2_tmp,
            dex::u2 reg3_classloader,
            dex::u2 reg_return, // 可以相同
            std::string &name_class,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);

        // const-string v0 "xxx"
        auto at_cls_str = builder.GetAsciiString(name_class.c_str());
        auto const_at_cls = code_ir->Alloc<lir::Bytecode>();
        const_at_cls->opcode = dex::OP_CONST_STRING;
        const_at_cls->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp)); // v0 = 类名
        const_at_cls->operands.push_back(
                code_ir->Alloc<lir::String>(at_cls_str, at_cls_str->orig_index));
        code_ir->instructions.insert(insert_point, const_at_cls);


        // 准备 Class.forName 的参数：initialize = true (v5)
        fir_tools::emitValToReg(code_ir, insert_point, reg2_tmp, 1);

        auto call_forName3 = code_ir->Alloc<lir::Bytecode>();
        call_forName3->opcode = dex::OP_INVOKE_STATIC;
        auto reg_forName3 = code_ir->Alloc<lir::VRegList>();
        reg_forName3->registers.clear();// v0=类名, v5=true, v4=应用类加载器
        reg_forName3->registers.push_back(reg1_tmp);
        reg_forName3->registers.push_back(reg2_tmp);
        reg_forName3->registers.push_back(reg3_classloader);

        call_forName3->operands.push_back(reg_forName3); // 先参数
        call_forName3->operands.push_back(f_Class_forName4loader);// 再方法
        code_ir->instructions.insert(insert_point, call_forName3);

        // 保存 HookBridge 类到 v0
        auto move_hb_cls = code_ir->Alloc<lir::Bytecode>();
        move_hb_cls->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_hb_cls->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));
        code_ir->instructions.insert(insert_point, move_hb_cls);

    }

    /** ----------------- 对象函数区 ------------------- */

    void do_classloader_loadClass(
            lir::Method *f_classloader_loadClass,
            lir::CodeIr *code_ir,
            dex::u2 reg_class, dex::u2 reg2_tmp,
            dex::u2 reg_return, // 可以相同
            std::string &name_class,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);

        // 准备类名字符串到 reg2_tmp
        auto class_name_str = builder.GetAsciiString(name_class.c_str());
        auto const_class_name = code_ir->Alloc<lir::Bytecode>();
        const_class_name->opcode = dex::OP_CONST_STRING;
        const_class_name->operands.push_back(code_ir->Alloc<lir::VReg>(reg2_tmp));
        const_class_name->operands.push_back(
                code_ir->Alloc<lir::String>(class_name_str, class_name_str->orig_index));
        code_ir->instructions.insert(insert_point, const_class_name);


        // 调用 loadClass 方法（使用应用类加载器 reg_class 和类名 reg2_tmp）
        auto call_loadClass = code_ir->Alloc<lir::Bytecode>();
        call_loadClass->opcode = dex::OP_INVOKE_VIRTUAL;
        auto load_class_regs = code_ir->Alloc<lir::VRegList>();
        load_class_regs->registers.clear();
        load_class_regs->registers.push_back(reg_class);
        load_class_regs->registers.push_back(reg2_tmp);
        call_loadClass->operands.push_back(load_class_regs);
        call_loadClass->operands.push_back(f_classloader_loadClass);
        code_ir->instructions.insert(insert_point, call_loadClass);


        // 保存加载的类到 v0
        auto move_loaded_class = code_ir->Alloc<lir::Bytecode>();
        move_loaded_class->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_loaded_class->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));
        code_ir->instructions.insert(insert_point, move_loaded_class);

    }


    void do_thread_getContextClassLoader(
            lir::Method *f_thread_getContextClassLoader,
            lir::CodeIr *code_ir,
            dex::u2 reg_thread,
            dex::u2 reg_return,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        auto call_getContextClassLoader = code_ir->Alloc<lir::Bytecode>();
        call_getContextClassLoader->opcode = dex::OP_INVOKE_VIRTUAL;
        auto reg_loader = code_ir->Alloc<lir::VRegList>();
        reg_loader->registers.clear();
        reg_loader->registers.push_back(reg_thread);

        call_getContextClassLoader->operands.push_back(reg_loader);
        call_getContextClassLoader->operands.push_back(f_thread_getContextClassLoader);
        code_ir->instructions.insert(insert_point, call_getContextClassLoader);

        // 保存应用类加载器到 v1
        auto move_loader = code_ir->Alloc<lir::Bytecode>();
        move_loader->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_loader->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));
        code_ir->instructions.insert(insert_point, move_loader);

    }

    /**
    *  method.invoke(Object,Object[]) 这个反射调用得传三个参数
    * m.invoke(null, new Object[]{ Object[],long });
    * m.invoke(null, new Object[]{ Object,long });
    * 方法是静态的，但反射调用按照反射的调用方式来
    * 不管用原来是什么方法，用反射都是 OP_INVOKE_VIRTUAL
    * @param f_Log_d
    * @param code_ir
    * @param reg_method
    * @param reg_obj
    * @param reg_do_args 参数数组两层
    */
    void do_method_invoke(lir::Method *f_method_invoke,
                          lir::CodeIr *code_ir,
                          dex::u2 reg_method, // method 对象
                          dex::u2 reg_obj, // 实例对象 静态函数为 null
                          dex::u2 reg_do_args, // 执行参数
                          dex::u2 reg_return, // 可重复
                          slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        // 调用 currentApplication() 方法
        auto call_currentApp = code_ir->Alloc<lir::Bytecode>();
        call_currentApp->opcode = dex::OP_INVOKE_VIRTUAL;
        auto reg_currentApp = code_ir->Alloc<lir::VRegList>();
        // v1=方法对象, v2=null(实例), v2=null(参数)
        reg_currentApp->registers.clear();
        reg_currentApp->registers.push_back(reg_method);
        reg_currentApp->registers.push_back(reg_obj);
        reg_currentApp->registers.push_back(reg_do_args);

        call_currentApp->operands.push_back(reg_currentApp);
        call_currentApp->operands.push_back(f_method_invoke);
        code_ir->instructions.insert(insert_point, call_currentApp);

        // 保存 Application 实例（Context）到 v3
        auto move_app = code_ir->Alloc<lir::Bytecode>();
        move_app->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_app->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return)); // v3 = Context
        code_ir->instructions.insert(insert_point, move_app);

    }

    /// Object[] newArgs = (Object[]) mhEnter.invokeWithArguments(rawArgs, methodId);
    void do_methodHandle_invokeWithArguments(
            lir::Method *f_methodHandle_invokeWithArguments,
            lir::CodeIr *code_ir,
            dex::u2 reg_mh, // classloader 对象
            dex::u2 reg_do_args, // Object[]
            dex::u2 reg_return, // 可重复
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        // invoke-virtual { mh, args[] }, MethodHandle->invokeWithArguments(Object[]):Object
        auto call = code_ir->Alloc<lir::Bytecode>();
        call->opcode = dex::OP_INVOKE_VIRTUAL;
        auto regs = code_ir->Alloc<lir::VRegList>();
        regs->registers.clear();
        regs->registers.push_back(reg_mh);       // receiver: MethodHandle
        regs->registers.push_back(reg_do_args);  // arg0: Object[]（变参列表）
        call->operands.push_back(regs);
        call->operands.push_back(f_methodHandle_invokeWithArguments);
        code_ir->instructions.insert(insert_point, call);

        // move-result-object v{reg_return}
        auto mres = code_ir->Alloc<lir::Bytecode>();
        mres->opcode = dex::OP_MOVE_RESULT_OBJECT;
        mres->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));
        code_ir->instructions.insert(insert_point, mres);
    }

    /**
     * 插桩绕开 loadClass，直接拿 Class<?> 或走 findClass
     * 重入/递归保护（ThreadLocal）
     * 原理 Thread → ContextClassLoader → loadClass → getDeclaredMethod → Method.invoke
     * ClassLoader#loadClass、Class#getDeclaredMethod、Method#invoke
     *
     * @return
     */
    bool do_sysloader_hook_funs(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp, dex::u2 reg2_tmp, dex::u2 reg3_tmp,//额外临时寄存器
            int reg_method_args, // 可以为null 用于存储方法参数类型数组（Class[]）
            int reg_do_args,// 可以为null 原始参数数组（Object[]）
            dex::u2 reg_return, // 可重复
            std::string &name_class,
            std::string &name_fun,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        /// ActivityThread 方案 不行 ----------------
        // 全局可使用 Thread.currentThread() → 返回线程对象 v0
        lir::Method *f_Thread_currentThread = fir_funs_def::get_Thread_currentThread(code_ir);
        CHECK_OR_RETURN(f_Thread_currentThread, false,
                        "[do_sysloader_hook_funs] f_Thread_currentThread error");

        do_static_0p(f_Thread_currentThread, code_ir, reg1_tmp, insert_point);

        // 全局可使用 thread.getContextClassLoader() classloader 对象 到v1
        lir::Method *f_thread_getContextClassLoader = fir_funs_def::get_thread_getContextClassLoader(
                code_ir);
        CHECK_OR_RETURN(f_thread_getContextClassLoader, false,
                        "[do_sysloader_hook_funs] f_thread_getContextClassLoader error");

        do_thread_getContextClassLoader(f_thread_getContextClassLoader, code_ir,
                                        reg1_tmp, reg1_tmp, insert_point);


        // ------------------ A 方案 ------------------

        // 全局可使用 执行拿到 HookBridge class 对象 到v0
        lir::Method *f_classloader_loadClass = fir_funs_def::get_classloader_loadClass(
                code_ir);
        CHECK_OR_RETURN(f_classloader_loadClass, false,
                        "[do_sysloader_hook_funs] f_classloader_loadClass error");

        do_classloader_loadClass(f_classloader_loadClass, code_ir,
                                 reg1_tmp, reg2_tmp,
                                 reg1_tmp, name_class, insert_point);


        /// Method m = clazz.getDeclaredMethod("onEnter", new Class[]{Object[].class});
        lir::Method *f_class_getDeclaredMethod = fir_funs_def::get_class_getDeclaredMethod(
                code_ir);
        CHECK_OR_RETURN(f_class_getDeclaredMethod, false,
                        "[do_sysloader_hook_funs] f_class_getDeclaredMethod error");


        if (reg_method_args < 0) {
            fir_tools::cre_null_reg(code_ir, reg3_tmp, insert_point);
            reg_method_args = reg3_tmp;
            LOGI("[do_sysloader_hook_funs] reg_method_args= %d ", reg_method_args)
        }

        // 拿到方法对象 v1   reg1_tmp= class对象  reg2_tmp = 方法名  reg3_tmp= 方法参数 class[]
        do_class_getDeclaredMethod(f_class_getDeclaredMethod, code_ir,
                                   reg1_tmp, reg2_tmp, reg_method_args,
                                   reg1_tmp, name_fun, insert_point);
        auto reg_method = reg1_tmp;

        /// Object ret = m.invoke(null, new Object[]{ args });
        lir::Method *f_method_invoke = fir_funs_def::get_method_invoke(code_ir);
        CHECK_OR_RETURN(f_method_invoke, false,
                        "[do_sysloader_hook_funs] f_method_invoke error");


        if (reg_do_args < 0) {
            fir_tools::cre_null_reg(code_ir, reg3_tmp, insert_point);
        }

        fir_tools::cre_null_reg(code_ir, reg2_tmp,
                                insert_point);  // 创建空引用 固定为静态方法 ****************** 前面可以用
        auto reg_obj_null = reg2_tmp;


//         reg1_tmp 是方法对象, reg2_tmp 静态是是 null, reg3_tmp 包装后的参数数组
        do_method_invoke(f_method_invoke, code_ir,
                         reg_method, reg_obj_null, reg_do_args,
                         reg_return, insert_point);


        return true;
    }


    /// MethodHandle mhEnter = lk.findStatic(clazz, "onEnter4fhook", mtEnter);
    void do_lookup_findStatic(
            lir::Method *f_lookup_findStatic,
            lir::CodeIr *code_ir,
            dex::u2 reg_lookup,    // ← Lookup 实例
            dex::u2 reg_class, // 放方法名 String
            dex::u2 reg_name,  // name_class
            dex::u2 reg_mt, // MethodType
            dex::u2 reg_return, // 可以相同 返回 MethodHandle
            std::string &name_fun,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);

        // 1) const-string v{reg_name}, "onEnter4fhook"
        // 准备类名字符串到 reg2_tmp
        auto class_name_str = builder.GetAsciiString(name_fun.c_str());
        auto const_class_name = code_ir->Alloc<lir::Bytecode>();
        const_class_name->opcode = dex::OP_CONST_STRING;
        const_class_name->operands.push_back(code_ir->Alloc<lir::VReg>(reg_name));
        const_class_name->operands.push_back(
                code_ir->Alloc<lir::String>(class_name_str, class_name_str->orig_index));
        code_ir->instructions.insert(insert_point, const_class_name);

        // 2) invoke-virtual { lookup, refcls, name, mt }, Lookup->findStatic(Class, String, MethodType): MethodHandle
        // 调用 loadClass 方法（使用应用类加载器 reg_class 和类名 reg2_tmp）
        auto call = code_ir->Alloc<lir::Bytecode>();
        call->opcode = dex::OP_INVOKE_VIRTUAL;
        auto regs = code_ir->Alloc<lir::VRegList>();
        regs->registers.clear();
        regs->registers.push_back(reg_lookup);  // receiver: MethodHandles$Lookup
        regs->registers.push_back(reg_class);  // arg1: Class<?>
        regs->registers.push_back(reg_name);    // arg2: String (method name)
        regs->registers.push_back(reg_mt);      // arg3: MethodType
        call->operands.push_back(regs);
        call->operands.push_back(f_lookup_findStatic);
        code_ir->instructions.insert(insert_point, call);

        // 3) move-result-object v{reg_return}  // MethodHandle
        auto move = code_ir->Alloc<lir::Bytecode>();
        move->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));
        code_ir->instructions.insert(insert_point, move);
    }


    bool do_get_onenter_method(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp,
            dex::u2 reg2_tmp,
            dex::u2 reg3_tmp,
            dex::u2 reg_clazz,
            dex::u2 reg_return,
            std::string &name_fun,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);

        auto *f_Class_getField = fir_funs_def::get_class_getField(code_ir);
        CHECK_OR_RETURN(f_Class_getField, false, "[C] Class.getField fail");

        // reg2_tmp = "MH_ENTER"
        {
            auto s = builder.GetAsciiString(name_fun.c_str());
            auto bc = code_ir->Alloc<lir::Bytecode>();
            bc->opcode = dex::OP_CONST_STRING;
            bc->operands.push_back(code_ir->Alloc<lir::VReg>(reg2_tmp));
            bc->operands.push_back(code_ir->Alloc<lir::String>(s, s->orig_index));
            code_ir->instructions.insert(insert_point, bc);
        }
        // Field f = clazz.getField("MH_ENTER") → reg3_tmp
        {
            auto call = code_ir->Alloc<lir::Bytecode>();
            call->opcode = dex::OP_INVOKE_VIRTUAL;
            auto regs = code_ir->Alloc<lir::VRegList>();
            regs->registers = {reg_clazz,
                               reg2_tmp};                                   // (receiver, name)
            call->operands.push_back(regs);
            call->operands.push_back(f_Class_getField);
            code_ir->instructions.insert(insert_point, call);

            auto mres = code_ir->Alloc<lir::Bytecode>();
            mres->opcode = dex::OP_MOVE_RESULT_OBJECT;
            mres->operands.push_back(
                    code_ir->Alloc<lir::VReg>(reg3_tmp));               // reg3_tmp = Field
            code_ir->instructions.insert(insert_point, mres);
        }

        auto *f_Field_get = fir_funs_def::get_field_get(code_ir);
        CHECK_OR_RETURN(f_Field_get, false, "[C] Field.get fail");

        // Object mhObj = f.get(null) → reg2_tmp ; check-cast to MethodHandle
        {
            fir_tools::emitValToReg(code_ir, insert_point, reg1_tmp, 0);                 // null
            auto call = code_ir->Alloc<lir::Bytecode>();
            call->opcode = dex::OP_INVOKE_VIRTUAL;
            auto regs = code_ir->Alloc<lir::VRegList>();
            regs->registers = {reg3_tmp, reg1_tmp};
            call->operands.push_back(regs);
            call->operands.push_back(f_Field_get);
            code_ir->instructions.insert(insert_point, call);

            auto mres = code_ir->Alloc<lir::Bytecode>();
            mres->opcode = dex::OP_MOVE_RESULT_OBJECT;
            mres->operands.push_back(
                    code_ir->Alloc<lir::VReg>(reg_return)); // reg2_tmp = Object
            code_ir->instructions.insert(insert_point, mres);

            // reg2_tmp = MethodHandle
            fir_tools::emitCheckCast(code_ir,
                                     "Ljava/lang/invoke/MethodHandle;",
                                     reg_return,
                                     insert_point);
        }

        return true;
    }


    /**
     *
    Object[] rawArgs  = new Object[3];
    rawArgs[0] = null;
    rawArgs[1] = "key_android_id";
    rawArgs[2] = "some-value";
    long methodId = 0x71L;


     1) onEnter4fhook(Object[] args, long methodId) -> Object[]
    MethodType mtEnter = MethodType.methodType(Object[].class, Object[].class, long.class);
     2) onExit4fhook(Object ret, long methodId) -> Object
    MethodType mtExit = MethodType.methodType(Object.class, Object.class, long.class);

    Thread t = Thread.currentThread();
    ClassLoader cl = t.getContextClassLoader();
    Class<?> clazz = Class.forName("top.feadre.fhook.FHook", true, cl);

    MethodHandles.Lookup lk = MethodHandles.publicLookup();
    MethodHandle mhEnter = lk.findStatic(clazz, "onEnter4fhook", mtEnter);
    MethodHandle mhExit = lk.findStatic(clazz, "onExit4fhook", mtExit);

    Object[] newArgs = (Object[]) mhEnter.invokeWithArguments(rawArgs, methodId);
    Object newRet = mhExit.invokeWithArguments(ret, methodId);

     * @return
     */
    bool do_sysloader_hook_funs_B(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp, dex::u2 reg2_tmp, dex::u2 reg3_tmp, dex::u2 reg4_tmp,
            int reg_do_args,// Object[Object[arg0,arg1...],Long]
            dex::u2 reg_return, // 可重复
            std::string &name_class,
            std::string &name_fun,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        fir_funs_do::cre_arr_class_args4onEnter(
                code_ir, reg1_tmp, reg2_tmp, reg3_tmp, insert_point);
        auto reg_method_args = reg3_tmp;

        // --------------------------- 111 -----------------------------
        // MethodType mtEnter = MethodType.methodType(Object[].class, Object[].class, long.class);
        lir::Method *f_get_MethodType_methodType_2p =
                fir_funs_def::get_MethodType_methodType_2p(code_ir);
        CHECK_OR_RETURN(f_get_MethodType_methodType_2p, false,
                        "[do_sysloader_hook_funs_B] f_get_MethodType_methodType_2p error");
        do_MethodType_methodType_2p(f_get_MethodType_methodType_2p, code_ir,
                                    reg1_tmp, reg_method_args,
                                    reg1_tmp,
                                    insert_point);
        auto reg_mt = reg1_tmp;  // *****************

        // -------------------------- 222 -----------------------------

        // 1) Thread t = Thread.currentThread();
        lir::Method *f_Thread_currentThread = fir_funs_def::get_Thread_currentThread(code_ir);
        CHECK_OR_RETURN(f_Thread_currentThread, false,
                        "[do_sysloader_hook_funs_B] f_Thread_currentThread error");
        // thread 对象
        do_static_0p(f_Thread_currentThread, code_ir, reg2_tmp, insert_point);


        // 2) ClassLoader cl = t.getContextClassLoader();
        lir::Method *f_thread_getContextClassLoader =
                fir_funs_def::get_thread_getContextClassLoader(code_ir);
        CHECK_OR_RETURN(f_thread_getContextClassLoader, false,
                        "[do_sysloader_hook_funs_B] f_thread_getContextClassLoader error");
        // 拿到线程的 classloader 对象
        do_thread_getContextClassLoader(f_thread_getContextClassLoader, code_ir,
                                        reg2_tmp, reg2_tmp, insert_point);


        // 3) Class<?> clazz = Class.forName("top.feadre.fhook.FHook", true, cl);
        lir::Method *f_Class_forName_3 = fir_funs_def::get_Class_forName_3p(
                code_ir);
        CHECK_OR_RETURN(f_Class_forName_3, false,
                        "[do_sysloader_hook_funs_B] get_class_forName error");
        /// 拿到 FHook.class 对象  *********** 这个有用   reg1_tmp
        do_Class_forName_3p(f_Class_forName_3, code_ir,
                            reg3_tmp, reg4_tmp, reg2_tmp,
                            reg2_tmp, name_class, insert_point);
        auto reg_class = reg2_tmp; // *******************

        // --------------------------- 333 -----------------------------
        // 准备类名字符串到 reg2_tmp
        // MethodHandles.Lookup lk = MethodHandles.publicLookup();
        lir::Method *f_MethodHandles_publicLookup = fir_funs_def::get_MethodHandles_publicLookup(
                code_ir);
        CHECK_OR_RETURN(f_MethodHandles_publicLookup, false,
                        "[do_sysloader_hook_funs_B] f_MethodHandles_publicLookup error");
        do_static_0p(f_MethodHandles_publicLookup, code_ir, reg3_tmp, insert_point);
        auto reg_lookup = reg3_tmp;

        // MethodHandle mhEnter = lk.findStatic(clazz, "onEnter4fhook", mtEnter);
        lir::Method *f_lookup_findStatic = fir_funs_def::get_lookup_findStatic(code_ir);
        CHECK_OR_RETURN(f_lookup_findStatic, false,
                        "[do_sysloader_hook_funs_B] f_lookup_findStatic error");
        do_lookup_findStatic(f_lookup_findStatic, code_ir,
                             reg_lookup, reg_class, reg4_tmp, reg_mt,
                             reg4_tmp, name_fun, insert_point);
        auto reg_mh = reg4_tmp; // 不能动 其它已可用


        // --------------------------- 444 执行 -----------------------------
        // Object[] newArgs = (Object[]) mhEnter.invokeWithArguments(rawArgs, methodId);
        lir::Method *f_methodHandle_invokeWithArguments =
                fir_funs_def::get_methodHandle_invokeWithArguments(code_ir);
        CHECK_OR_RETURN(f_methodHandle_invokeWithArguments, false,
                        "[do_sysloader_hook_funs_B] f_methodHandle_invokeWithArguments error");

        do_methodHandle_invokeWithArguments(f_methodHandle_invokeWithArguments, code_ir,
                                            reg_mh, reg_do_args,
                                            reg_return,
                                            insert_point);

        return true;
    }

    // 最小可行：系统侧只做 forName + 取 public 静态 MH_ENTER + 调用
    bool do_sysloader_hook_funs_C(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp, dex::u2 reg2_tmp, dex::u2 reg3_tmp, dex::u2 reg4_tmp,
            int reg_do_args,                 // Object[]{ rawArgs, Long(methodId) }
            dex::u2 reg_return,              // out: Object[] (newArgs)
            std::string &name_class,         // "top.feadre.fhook.FHook"
            std::string &name_fun,           // 未用，保留
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);


        // 1) Thread.currentThread().getContextClassLoader()
        auto *f_Thread_currentThread = fir_funs_def::get_Thread_currentThread(code_ir);
        CHECK_OR_RETURN(f_Thread_currentThread, false, "[C] Thread.currentThread fail");
        do_static_0p(f_Thread_currentThread, code_ir, reg2_tmp,
                     insert_point);          // reg2_tmp = Thread

        auto *f_thread_getContextClassLoader = fir_funs_def::get_thread_getContextClassLoader(
                code_ir);
        CHECK_OR_RETURN(f_thread_getContextClassLoader, false, "[C] t.getContextClassLoader fail");
        do_thread_getContextClassLoader(f_thread_getContextClassLoader, code_ir,
                                        reg2_tmp, reg2_tmp,
                                        insert_point);               // reg2_tmp = ClassLoader

        // 2) Class.forName("top.feadre.fhook.FHook", true, cl) → reg2_tmp = FHook.class
        auto *f_Class_forName_3 = fir_funs_def::get_Class_forName_3p(code_ir);
        CHECK_OR_RETURN(f_Class_forName_3, false, "[C] Class.forName fail");
        do_Class_forName_3p(f_Class_forName_3, code_ir,
                            reg3_tmp, reg4_tmp, reg2_tmp,
                            reg2_tmp, name_class, insert_point);
        auto reg_clazz = reg2_tmp;



        // 3) 直接用 public 字段：Field f = clazz.getField("MH_ENTER"); MethodHandle mh = (MethodHandle) f.get(null)



        /// 传了1234 个寄存器
        bool res = do_get_onenter_method(code_ir, reg1_tmp, reg4_tmp, reg3_tmp,
                                         reg_clazz, reg4_tmp,
                                         name_fun, insert_point);
        if (!res)return res;

        auto *f_Class_getField = fir_funs_def::get_class_getField(code_ir);
        CHECK_OR_RETURN(f_Class_getField, false, "[C] Class.getField fail");

//        // reg4_tmp = "MH_ENTER"
//        {
//            auto s = builder.GetAsciiString("MH_ENTER");
//            auto bc = code_ir->Alloc<lir::Bytecode>();
//            bc->opcode = dex::OP_CONST_STRING;
//            bc->operands.push_back(code_ir->Alloc<lir::VReg>(reg4_tmp));
//            bc->operands.push_back(code_ir->Alloc<lir::String>(s, s->orig_index));
//            code_ir->instructions.insert(insert_point, bc);
//        }
//        // Field f = clazz.getField("MH_ENTER") → reg3_tmp
//        {
//            auto call = code_ir->Alloc<lir::Bytecode>();
//            call->opcode = dex::OP_INVOKE_VIRTUAL;
//            auto regs = code_ir->Alloc<lir::VRegList>();
//            regs->registers = {reg_clazz,
//                               reg4_tmp};                                   // (receiver, name)
//            call->operands.push_back(regs);
//            call->operands.push_back(f_Class_getField);
//            code_ir->instructions.insert(insert_point, call);
//
//            auto mres = code_ir->Alloc<lir::Bytecode>();
//            mres->opcode = dex::OP_MOVE_RESULT_OBJECT;
//            mres->operands.push_back(
//                    code_ir->Alloc<lir::VReg>(reg3_tmp));               // reg3_tmp = Field
//            code_ir->instructions.insert(insert_point, mres);
//        }
//
//        auto *f_Field_get = fir_funs_def::get_field_get(code_ir);
//        CHECK_OR_RETURN(f_Field_get, false, "[C] Field.get fail");
//
//        // Object mhObj = f.get(null) → reg4_tmp ; check-cast to MethodHandle
//        {
//            fir_tools::emitValToReg(code_ir, insert_point, reg1_tmp, 0);                 // null
//            auto call = code_ir->Alloc<lir::Bytecode>();
//            call->opcode = dex::OP_INVOKE_VIRTUAL;
//            auto regs = code_ir->Alloc<lir::VRegList>();
//            regs->registers = {reg3_tmp, reg1_tmp};
//            call->operands.push_back(regs);
//            call->operands.push_back(f_Field_get);
//            code_ir->instructions.insert(insert_point, call);
//
//            auto mres = code_ir->Alloc<lir::Bytecode>();
//            mres->opcode = dex::OP_MOVE_RESULT_OBJECT;
//            mres->operands.push_back(
//                    code_ir->Alloc<lir::VReg>(reg4_tmp));               // reg4_tmp = Object
//            code_ir->instructions.insert(insert_point, mres);
//
////            emitCheckCast(reg4_tmp, "Ljava/lang/invoke/MethodHandle;");                  // reg4_tmp = MethodHandle
//            fir_tools::emitCheckCast(code_ir,
//                                     "Ljava/lang/invoke/MethodHandle;",
//                                     reg4_tmp,
//                                     insert_point);
//        }

        auto reg_mh = reg4_tmp; // 不能再覆盖

        // 4) 调用 mh.invokeWithArguments(wrapperArgs)
        auto *f_mh_invoke = fir_funs_def::get_methodHandle_invokeWithArguments(code_ir);
        CHECK_OR_RETURN(f_mh_invoke, false, "[C] MethodHandle.invokeWithArguments fail");

        do_methodHandle_invokeWithArguments(
                f_mh_invoke, code_ir,
                reg_mh,            // MethodHandle
                reg_do_args,       // Object[]{ rawArgs, Long(methodId) }
                reg_return,        // out: Object[] newArgs
                insert_point
        );

        return true;
    }

    /** ----------------- do java frame  ------------------- */



    /** ----------------- 参数区 ------------------- */

    /**
   * 生成 onEnter 的参数类型数组（Class[]{Object[].class,Long.class}）
   * @param code_ir
   * @param reg1_tmp
   * @param reg2_tmp
   * @param reg3_arr
   * @param insert_point
      [6] const/4 v0 #0x1
      [7] new-array v3 v0 [Ljava/lang/Class;
      [8] const-class v1 [Ljava/lang/Object;
      [9] const/4 v0 #0x0
      [10] aput-object v1 v3 v0
   */
    void cre_arr_class_args4onEnter(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp, // array_size 也是索引
            dex::u2 reg2_tmp, // value
            dex::u2 reg3_arr, // array 这个 object[] 对象
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);

        // const reg1_tmp #0x2 → 数组长度2  长度=2：  Class[]{ Object[].class, long.class }
        fir_tools::emitValToReg(code_ir, insert_point, reg1_tmp, 2);

        // 填 index 0: Object[].class new-array reg3_arr reg1_tmp      reg3_arr 是  Object[].class（Class 类型）
        {
            const auto class_base_type = builder.GetType("[Ljava/lang/Class;");

            auto new_arr = code_ir->Alloc<lir::Bytecode>();
            new_arr->opcode = dex::OP_NEW_ARRAY;
            new_arr->operands.push_back(
                    code_ir->Alloc<lir::VReg>(reg3_arr)); // reg3_arr = Class[] 数组
            new_arr->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp)); // 长度=reg1_tmp（2）
            new_arr->operands.push_back(
                    code_ir->Alloc<lir::Type>(class_base_type, class_base_type->orig_index));
            code_ir->instructions.insert(insert_point, new_arr);
        }

        {
            // 生成 const-class reg2_tmp, [Ljava/lang/Object;
            fir_tools::emitReference2Class(code_ir, "[Ljava/lang/Object;",
                                           reg2_tmp, insert_point);

            // 然后再做 index const 和 aput-object (把 reg2_tmp 存到 Class[] 数组)
            fir_tools::emitValToReg(code_ir, insert_point, reg1_tmp, 0);

            auto aput_class = code_ir->Alloc<lir::Bytecode>();
            aput_class->opcode = dex::OP_APUT_OBJECT;
            aput_class->operands.push_back(
                    code_ir->Alloc<lir::VReg>(reg2_tmp)); // value: Object[].class
            aput_class->operands.push_back(code_ir->Alloc<lir::VReg>(reg3_arr)); // Class[] array
            aput_class->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp)); // index 0
            code_ir->instructions.insert(insert_point, aput_class);
        }

        // idx 1 = long.class (Long.TYPE) Ljava/lang/Long;->TYPE:Ljava/lang/Class;
        {
            fir_tools::emitLong2Class(code_ir, reg2_tmp, insert_point);

            fir_tools::emitValToReg(code_ir, insert_point, reg1_tmp, 1);

            auto aput1 = code_ir->Alloc<lir::Bytecode>();
            aput1->opcode = dex::OP_APUT_OBJECT;
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg2_tmp)); // Long.TYPE
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg3_arr)); // Class[]
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp)); // 1
            code_ir->instructions.insert(insert_point, aput1);
        }
    }

    /// 生成 onExit 的参数类型数组（Class[]{Object.class,Long.class}）
    void cre_arr_class_args4onExit(
            lir::CodeIr *code_ir,
            dex::u2 reg1, // index 与 array_size 复用
            dex::u2 reg2, // value 临时（存放 Class 对象）
            dex::u2 reg3_arr, // array 目标寄存器（Class[]）
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);

        // Class[] 长度=2： { Object.class, long.class }
        fir_tools::emitValToReg(code_ir, insert_point, reg1, 2);

        // new-array v<reg3_arr>, v<reg1>, [Ljava/lang/Class;
        {
            const auto class_array_type = builder.GetType("[Ljava/lang/Class;");
            auto new_arr = code_ir->Alloc<lir::Bytecode>();
            new_arr->opcode = dex::OP_NEW_ARRAY;
            new_arr->operands.push_back(
                    code_ir->Alloc<lir::VReg>(reg3_arr)); // 结果 Class[] 存到 reg3_arr
            new_arr->operands.push_back(code_ir->Alloc<lir::VReg>(reg1)); // 长度=2
            new_arr->operands.push_back(
                    code_ir->Alloc<lir::Type>(class_array_type, class_array_type->orig_index));
            code_ir->instructions.insert(insert_point, new_arr);
        }

        // index 0 = Object.class
        {
            // const-class v<reg2>, Ljava/lang/Object;
            fir_tools::emitReference2Class(code_ir, "Ljava/lang/Object;",
                                           reg2, insert_point);

            // index = 0
            fir_tools::emitValToReg(code_ir, insert_point, reg1, 0);

            // aput-object v<reg2> → v<reg3_arr>[v<reg1>]
            auto aput0 = code_ir->Alloc<lir::Bytecode>();
            aput0->opcode = dex::OP_APUT_OBJECT;
            aput0->operands.push_back(code_ir->Alloc<lir::VReg>(reg2)); // Object.class
            aput0->operands.push_back(code_ir->Alloc<lir::VReg>(reg3_arr)); // Class[] 数组
            aput0->operands.push_back(code_ir->Alloc<lir::VReg>(reg1)); // index 0
            code_ir->instructions.insert(insert_point, aput0);
        }

        // index 1 = long.class  —— 通过 java.lang.Long.TYPE 获取
        {
            fir_tools::emitLong2Class(code_ir, reg2, insert_point);

            // index = 1
            fir_tools::emitValToReg(code_ir, insert_point, reg1, 1);

            // aput-object v<reg2> → v<reg3_arr>[v<reg1>]
            auto aput1 = code_ir->Alloc<lir::Bytecode>();
            aput1->opcode = dex::OP_APUT_OBJECT;
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg2)); // Long.TYPE (long.class)
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg3_arr)); // Class[]
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg1)); // index 1
            code_ir->instructions.insert(insert_point, aput1);
        }
    }


    /**
     * 把原返回值弄成 object   这个函数不会报错 需确保 reg_do_arg 不与宽冲突
     * 用于传入 public final static Object onExit4fhook(Object ret, long methodId)
     *
     * @param code_ir
     * @param reg_return_orig
     * @param reg_do_arg
     * @param is_wide_return 带有宽恢复功能
     * @param insert_point
     * @return 是否成功
     */
    bool cre_arr_do_args4onExit(
            lir::CodeIr *code_ir,
            int reg_return_orig,         // 原方法返回值所在寄存器 传入 清空则没有返回值
            dex::u2 reg_do_arg,  // 这个是返回值 必需要一个正常的寄存器，可以相等，需要外面判断完成是不是宽
            bool is_wide_return,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        LOGD("[cre_arr_do_args4onExit] reg_return_orig= %d", reg_return_orig)
        auto ir_method = code_ir->ir_method;
        auto proto = ir_method->decl->prototype;
        auto return_type = proto->return_type;

        // 无返回值或方法已经清空情况
        if (strcmp(return_type->descriptor->c_str(), "V") == 0 || reg_return_orig < 0) {
            // 拿到方法信息
            LOGI("[cre_arr_do_args4onExit] %s.%s%s -> void/null, 插入null到v%d",
                 ir_method->decl->parent->Decl().c_str(),
                 ir_method->decl->name->c_str(),
                 ir_method->decl->prototype->Signature().c_str(),
                 reg_do_arg);
            // 创建空对象
            fir_tools::cre_null_reg(code_ir, reg_do_arg, insert_point);
            return true;
        }

        if (return_type->GetCategory() == ir::Type::Category::Reference) {
            // === 引用直接赋值 ，向上不用转换 ===
            auto move_obj = code_ir->Alloc<lir::Bytecode>();
            move_obj->opcode = dex::OP_MOVE_OBJECT;
            move_obj->operands.push_back(code_ir->Alloc<lir::VReg>(reg_do_arg));
            move_obj->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return_orig));
            code_ir->instructions.insert(insert_point, move_obj);
            return true;
        }

        // === 基本标量类型：装箱成对象 ===
        fir_tools::box_scalar_value(insert_point, code_ir, return_type,
                                    reg_return_orig, reg_do_arg);

        // 如果是寄存器相同 就不能恢复
        if (is_wide_return && reg_return_orig != reg_do_arg) {
            // 恢复宽寄存器
            fir_tools::emitValToReg(code_ir, insert_point, reg_return_orig + 1, 0);
            fir_tools::emitValToReg(code_ir, insert_point, reg_return_orig, 0);
        }

        LOGD("[cre_arr_do_args4onExit] 处理完成：%s → 转换为Object到v%d",
             return_type->descriptor->c_str(), reg_do_arg);

        return true;
    }

    /// 自动找到参数寄存器 把方法的参数强转object 放在 object[arg0,arg1...] 数组中
    void cre_arr_do_args4onEnter(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp_idx, // 这里避开了宽 array_size 也是索引
            dex::u2 reg2_tmp_value, // value
            dex::u2 reg3_arr, // array 这个 object[] 对象
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);
        const auto ir_method = code_ir->ir_method;

        bool is_static = (ir_method->access_flags & dex::kAccStatic) != 0;

        // 判断有没有参数，没有参数就新建一个空参数列表
        auto param_types_list = ir_method->decl->prototype->param_types;
        auto param_types =
                param_types_list != nullptr ? param_types_list->types : std::vector<ir::Type *>();

        //  const v0 #0x2  定义原函数参数 +1 无论是否静态
        auto const_size_op = code_ir->Alloc<lir::Bytecode>(); //  分配一条字节码指令
        const_size_op->opcode = dex::OP_CONST; // 将32位常量存入寄存器
        const_size_op->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp_idx)); // 存数组大小
        const_size_op->operands.push_back(code_ir->Alloc<lir::Const32>(
                1 + param_types.size()));
        code_ir->instructions.insert(insert_point, const_size_op);
//        LOGD("[cre_arr_do_args4onEnter] %s",
//             SmaliPrinter::ToSmali(dynamic_cast<lir::Bytecode *>(*insert_point)).c_str())

        // 创建Object[]数组对象,大小由 v0 决定 new-array v2 v0 [Ljava/lang/Object;
        const auto obj_array_type = builder.GetType("[Ljava/lang/Object;");
        auto allocate_array_op = code_ir->Alloc<lir::Bytecode>();
        allocate_array_op->opcode = dex::OP_NEW_ARRAY;
        allocate_array_op->operands.push_back(code_ir->Alloc<lir::VReg>(reg3_arr)); // 数组对象
        allocate_array_op->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp_idx)); // 数组大小
        allocate_array_op->operands.push_back(
                code_ir->Alloc<lir::Type>(obj_array_type, obj_array_type->orig_index));
        code_ir->instructions.insert(insert_point, allocate_array_op);
//        LOGD("[cre_arr_do_args4onEnter] %s",
//             SmaliPrinter::ToSmali(dynamic_cast<lir::Bytecode *>(*insert_point)).c_str())

        // 数组是4个 类型是3个
        std::vector<ir::Type *> types;
        if (!is_static) {
            // 前面 是直接加1的
            types.push_back(ir_method->decl->parent);
        }
        // parameters
        types.insert(types.end(), param_types.begin(), param_types.end());

        // 这个是参数寄存器初始 *****
        dex::u4 reg_arg = ir_method->code->registers - ir_method->code->ins_count;
        int i = 0;
        // 构建参数数组赋值 --------------
        for (auto type: types) {
            dex::u4 src_reg = 0; // 临时寄存

            if (i == 0 && is_static) i++; // 如果是静态，第一个不处理，为空

            // Void, Scalar, WideScalar, Reference
            if (type->GetCategory() != ir::Type::Category::Reference) {
                /* 非引用类型（基本类型，如 int、long、float）→ 需要装箱
                 * invoke-static/range {v3 .. v3} Ljava/lang/Integer;->valueOf(I)Ljava/lang/Integer;
                    move-result-object v1
                    Integer integerObj = Integer.valueOf(v3Value);
                 */
                fir_tools::box_scalar_value(insert_point, code_ir, type, reg_arg, reg2_tmp_value);
                src_reg = reg2_tmp_value; // 用临时寄存器
                // 宽标量类型（long、double）→ 多加1个寄存器
                reg_arg += 1 + (type->GetCategory() == ir::Type::Category::WideScalar);
            } else {
                // 引用类型（如 String、Object、数组）→ 直接用原参数寄存器
                src_reg = reg_arg;
                reg_arg++;
            }

            //  定义数组索引 const v0 #0x1
            fir_tools::emitValToReg(code_ir, insert_point, reg1_tmp_idx, i);
            i++;
//            LOGD("[cre_arr_do_args4onEnter] %s",
//                 SmaliPrinter::ToSmali(dynamic_cast<lir::Bytecode *>(*insert_point)).c_str())

            //  把v2[v0]=v1    aput-object v1 v2 v0
            auto aput_op = code_ir->Alloc<lir::Bytecode>();
            aput_op->opcode = dex::OP_APUT_OBJECT;
            aput_op->operands.push_back(code_ir->Alloc<lir::VReg>(src_reg)); // 原参数或用v1存放的包箱对象
            aput_op->operands.push_back(code_ir->Alloc<lir::VReg>(reg3_arr)); // 数组对象
            aput_op->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp_idx)); // 动态索引
            code_ir->instructions.insert(insert_point, aput_op);
//            LOGD("[cre_arr_do_args4onEnter] %s",
//                 SmaliPrinter::ToSmali(dynamic_cast<lir::Bytecode *>(*insert_point)).c_str())

        }
    }

    /// 用 Object[Object[],Long] 把指定的参数 放入
    void cre_arr_do_arg_2p(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp_idx, // 索引寄存器（也临时承接 String/Long）
            dex::u2 reg2_value, // 需要打包的第一个参数     临时（用作 aput 索引=1 等）
            dex::u2 reg3_arr, // 外层 Object[] 数组寄存器（输出）
            uint64_t method_id, // 要写入的 methodId  打包的第二个参数
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);
        const auto obj_array_type = builder.GetType("[Ljava/lang/Object;");

        {
            // outer = new Object[2]
            fir_tools::emitValToReg(code_ir, insert_point, reg1_tmp_idx, 2);

            // 创建数组对象 reg4
//        const auto obj_array_type = builder.GetType("[Ljava/lang/Object;");
            auto new_outer_array = code_ir->Alloc<lir::Bytecode>();
            new_outer_array->opcode = dex::OP_NEW_ARRAY;
            new_outer_array->operands.push_back(code_ir->Alloc<lir::VReg>(reg3_arr)); // outer
            new_outer_array->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp_idx));// length=2
            new_outer_array->operands.push_back(code_ir->Alloc<lir::Type>(
                    obj_array_type, obj_array_type->orig_index));  // 正确类型：Object[]
            code_ir->instructions.insert(insert_point, new_outer_array);

            // reg1_tmp_idx 索引为0 outer[0] = reg2_value（内层 Object[]）
            fir_tools::emitValToReg(code_ir, insert_point, reg1_tmp_idx, 0);

            // 先把原参数放进去
            auto aput_outer = code_ir->Alloc<lir::Bytecode>();
            aput_outer->opcode = dex::OP_APUT_OBJECT;
            aput_outer->operands.push_back(code_ir->Alloc<lir::VReg>(reg2_value));  // 原始参数数组
            aput_outer->operands.push_back(code_ir->Alloc<lir::VReg>(reg3_arr));         // 外层数组
            aput_outer->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp_idx));         // 索引0
            code_ir->instructions.insert(insert_point, aput_outer);
        }

        // outer[1] = Long.valueOf(String.valueOf(method_id))
        // 这里走 String 避免使用宽寄存器：Long.valueOf(Ljava/lang/String;)Ljava/lang/Long;
        {
            // const-string v<reg_idx>, "<method_id>"
            std::string mid_str = std::to_string(method_id);
            auto jstr = builder.GetAsciiString(mid_str.c_str());
            auto c_str = code_ir->Alloc<lir::Bytecode>();
            c_str->opcode = dex::OP_CONST_STRING;
            c_str->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp_idx)); // 暂存 String
            c_str->operands.push_back(code_ir->Alloc<lir::String>(jstr, jstr->orig_index));
            code_ir->instructions.insert(insert_point, c_str);

            // Long.valueOf(String) -> Long  放回 reg_idx（复用寄存器承接返回的 Long 对象）
            const auto type_Long = builder.GetType("Ljava/lang/Long;");
            const auto type_Str = builder.GetType("Ljava/lang/String;");
            const auto proto_valof = builder.GetProto(
                    type_Long, builder.GetTypeList({type_Str}));
            auto name_valueOf = builder.GetAsciiString("valueOf");
            auto md_valof = builder.GetMethodDecl(
                    name_valueOf, proto_valof, type_Long);
            auto lr_valof = code_ir->Alloc<lir::Method>(md_valof, md_valof->orig_index);

            auto args = code_ir->Alloc<lir::VRegList>(); // 这个是直接加 寄存器号
            args->registers.clear(); // 无参数
            args->registers.push_back(reg1_tmp_idx); // 参数：String

            auto inv = code_ir->Alloc<lir::Bytecode>();
            inv->opcode = dex::OP_INVOKE_STATIC; // Long.valueOf(String)
            inv->operands.push_back(args);
            inv->operands.push_back(lr_valof);
            code_ir->instructions.insert(insert_point, inv);

            auto mres = code_ir->Alloc<lir::Bytecode>();
            mres->opcode = dex::OP_MOVE_RESULT_OBJECT;
            mres->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp_idx));
            code_ir->instructions.insert(insert_point, mres);

            // idx=1 （此处用 reg2_value 暂存索引，避免覆盖 reg1_tmp_idx 中的 Long）
            fir_tools::emitValToReg(code_ir, insert_point, reg2_value, 1);

            // aput-object <Long>, outer, 1
            auto aput1 = code_ir->Alloc<lir::Bytecode>();
            aput1->opcode = dex::OP_APUT_OBJECT;
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp_idx));    // value = Long对象
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg3_arr));  // array = outer
            aput1->operands.push_back(
                    code_ir->Alloc<lir::VReg>(reg2_value));  // index = 1（注意此时 reg_inner 仅作索引用）
            code_ir->instructions.insert(insert_point, aput1);
        }

    }


    /**
     * 创建参数 Object[] 数组在v2, 包括this 对象，或为null
     * @param code_ir
     * @param builder
     * @param reg1_tmp
     * @param reg2_tmp
     * @param reg_arr
     * @param insert_point
        [0] const v0 #0x2
        [1] new-array v2 v0 [Ljava/lang/Object;
        [2] invoke-static/range {v4 .. v4} Ljava/lang/Integer;->valueOf(I)Ljava/lang/Integer;
        [3] move-result-object v1
        [4] const v0 #0x1
        [5] aput-object v1 v2 v0
     */
    void cre_arr_do_args4onEnter(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp, // array_size 也是索引
            dex::u2 reg2_tmp, // value  array 这个 object[object[]] 再包一层对象
            dex::u2 reg_arr, // array 这个 object[] 对象
            uint64_t method_id, // 要写入的 methodId
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        // 创建 object[] 对象 -》 reg_arr
        cre_arr_do_args4onEnter(code_ir, reg1_tmp, reg_arr,
                                reg2_tmp, insert_point);

        // object[object[]] 再包一层对象
        cre_arr_do_arg_2p(code_ir, reg1_tmp, reg2_tmp,
                          reg_arr, method_id, insert_point);

    }


};