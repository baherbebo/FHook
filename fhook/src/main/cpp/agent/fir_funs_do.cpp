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


    /// 包装参数和返回值 MethodType mtEnter = MethodType.methodType(Object[].class, Object[].class, long.class);
    void do_MethodType_methodType_2p(
            lir::Method *f_MethodType_methodType,
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp,
            dex::u2 reg_method_args, // ptypes = new Class[2]; ptypes[0]=Object[].class; ptypes[1]=Long.TYPE;
            dex::u2 reg_return, // 可以重复
            std::string &rtype_name,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        // 创建 rtype = Object[].class  "[Ljava/lang/Object;" "Ljava/lang/Object;"
        fir_tools::emitReference2Class(code_ir, rtype_name.c_str(),
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
    bool do_frame_hfun(
            lir::Method *f_THook_onEnter,
            lir::CodeIr *code_ir,
            dex::u2 reg_arg, // Object[]
            dex::u2 reg_tmp_long, // long 必须宽寄存器
            dex::u2 reg_return, // 可以和reg_arg重复， 注意重叠（reg2_tmp_long 这个是宽）
            uint64_t method_id, // uint64_t
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point
    ) {
        ir::Builder builder(code_ir->dex_ir);

        // 1) const-wide v(reg_tmp_long/reg_tmp_long+1), method_id
        fir_tools::emitValToRegWide(code_ir, insert_point, reg_tmp_long, (int64_t) method_id);

        // 2) 选择调用形式
        const bool all_small =
                (reg_arg <= 15) && (reg_tmp_long <= 15) && ((reg_tmp_long + 1) <= 15);
        const bool contiguous = (reg_arg + 1 == reg_tmp_long);

        if (all_small) {
            // --- 方案 A：非 range（35c） ---
            auto rlist = code_ir->Alloc<lir::VRegList>();
            rlist->registers.push_back(reg_arg);
            rlist->registers.push_back(reg_tmp_long);
            rlist->registers.push_back((dex::u2) (reg_tmp_long + 1));

            auto bc = code_ir->Alloc<lir::Bytecode>();
            bc->opcode = dex::OP_INVOKE_STATIC;      // 35c：寄存器号均需 ≤15
            bc->operands.push_back(rlist);
            bc->operands.push_back(f_THook_onEnter);
            code_ir->instructions.insert(insert_point, bc);
        } else if (contiguous) {
            // --- 方案 B：range（3rc）---
            // 形参连续且顺序为 arg, long_lo, long_hi
            auto rrange = code_ir->Alloc<lir::VRegRange>(reg_arg, 3);
            auto bc = code_ir->Alloc<lir::Bytecode>();
            bc->opcode = dex::OP_INVOKE_STATIC_RANGE; // 3rc：需要连续窗口
            bc->operands.push_back(rrange);
            bc->operands.push_back(f_THook_onEnter);
            code_ir->instructions.insert(insert_point, bc);
        } else {
            // 其它情况：既不全 ≤15，也不连续，不能合法编码
            LOGE("[do_frame_hfun] illegal arg regs for invoke: reg_arg=%u, reg_tmp_long=%u. "
                 "Need either all <=15 (35c) or contiguous {arg, long_lo, long_hi} (3rc).",
                 reg_arg, reg_tmp_long);
            return false;
        }

        {
            // move-result-object v<reg_arg>
            auto mres = code_ir->Alloc<lir::Bytecode>();
            mres->opcode = dex::OP_MOVE_RESULT_OBJECT;
            mres->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));
            code_ir->instructions.insert(insert_point, mres);
        }

//        {
//            // long 临时必须恢复宽寄存器
//            // 先清高半 v+1
//            fir_tools::emitValToReg(code_ir, insert_point, reg_tmp_long + 1, 0);
//            // 再清低半 v
//            fir_tools::emitValToReg(code_ir, insert_point, reg_tmp_long, 0);
//        }

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


    bool do_get_java_method(
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

    /** ----------------- 参数区 ------------------- */

    /// 用 Object[Object[],Long] 把指定的参数 放入
    void cre_arr_do_arg_2p(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp_idx, // 数组大小 也是索引 必须小于16 22c指令
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


};