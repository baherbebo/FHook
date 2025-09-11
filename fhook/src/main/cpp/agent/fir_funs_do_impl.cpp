//
// Created by Administrator on 2025/9/8.
//

#include "../FGlobal_flib.h"

#define TAG FEADRE_TAG("fir_tools")

#include "fir_funs_do_impl.h"

#include "fir_funs_def.h"
#include "fir_funs_do.h"
#include "fir_tools.h"


namespace fir_impl {
    /**
     * 方案 A：ClassLoader + Class.getDeclaredMethod + Method.invoke
         Thread t = Thread.currentThread();
         ClassLoader cl = t.getContextClassLoader();
         Class<?> clazz = Class.forName(name_class, true, cl);
         Method m = clazz.getDeclaredMethod(name_fun, Class<?>... --- new Class[] { ... } );
         m.setAccessible(true);
         Object ret = m.invoke(null,  Object[] args --- do_args);
     * @return
     */
    bool do_sysloader_hook_funs_A(
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
                        "[do_sysloader_hook_funs_A] f_Thread_currentThread error");

        fir_funs_do::do_static_0p(f_Thread_currentThread, code_ir, reg1_tmp, insert_point);

        // 全局可使用 thread.getContextClassLoader() classloader 对象 到v1
        lir::Method *f_thread_getContextClassLoader = fir_funs_def::get_thread_getContextClassLoader(
                code_ir);
        CHECK_OR_RETURN(f_thread_getContextClassLoader, false,
                        "[do_sysloader_hook_funs_A] f_thread_getContextClassLoader error");

        fir_funs_do::do_thread_getContextClassLoader(f_thread_getContextClassLoader, code_ir,
                                                     reg1_tmp, reg1_tmp, insert_point);


        // ------------------ A 方案 ------------------

        // 全局可使用 执行拿到 HookBridge class 对象 到v0
        lir::Method *f_classloader_loadClass = fir_funs_def::get_classloader_loadClass(
                code_ir);
        CHECK_OR_RETURN(f_classloader_loadClass, false,
                        "[do_sysloader_hook_funs_A] f_classloader_loadClass error");

        fir_funs_do::do_classloader_loadClass(f_classloader_loadClass, code_ir,
                                              reg1_tmp, reg2_tmp,
                                              reg1_tmp, name_class, insert_point);


        /// Method m = clazz.getDeclaredMethod("onEnter", new Class[]{Object[].class});
        lir::Method *f_class_getDeclaredMethod = fir_funs_def::get_class_getDeclaredMethod(
                code_ir);
        CHECK_OR_RETURN(f_class_getDeclaredMethod, false,
                        "[do_sysloader_hook_funs_A] f_class_getDeclaredMethod error");


        if (reg_method_args < 0) {
            fir_tools::cre_null_reg(code_ir, reg3_tmp, insert_point);
            reg_method_args = reg3_tmp;
            LOGI("[do_sysloader_hook_funs_A] reg_method_args= %d ", reg_method_args)
        }

        // 拿到方法对象 v1   reg1_tmp= class对象  reg2_tmp = 方法名  reg3_tmp= 方法参数 class[]
        fir_funs_do::do_class_getDeclaredMethod(f_class_getDeclaredMethod, code_ir,
                                                reg1_tmp, reg2_tmp, reg_method_args,
                                                reg1_tmp, name_fun, insert_point);
        auto reg_method = reg1_tmp;

        /// Object ret = m.invoke(null, new Object[]{ args });
        lir::Method *f_method_invoke = fir_funs_def::get_method_invoke(code_ir);
        CHECK_OR_RETURN(f_method_invoke, false,
                        "[do_sysloader_hook_funs_A] f_method_invoke error");


        if (reg_do_args < 0) {
            fir_tools::cre_null_reg(code_ir, reg3_tmp, insert_point);
        }

        fir_tools::cre_null_reg(code_ir, reg2_tmp,
                                insert_point);  // 创建空引用 固定为静态方法 ****************** 前面可以用
        auto reg_obj_null = reg2_tmp;


//         reg1_tmp 是方法对象, reg2_tmp 静态是是 null, reg3_tmp 包装后的参数数组
        fir_funs_do::do_method_invoke(f_method_invoke, code_ir,
                                      reg_method, reg_obj_null, reg_do_args,
                                      reg_return, insert_point);


        return true;
    }


    /**
     * 方案 B：MethodHandles + MethodType + Lookup.findStatic（动态查找静态 MH）
     *      避免反射 Method.invoke 的一些限制，性能和可见性通常更好
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
            dex::u2 reg1_size_22c, dex::u2 reg3_arr_22c,
            int reg_do_args,// Object[Object[arg0,arg1...],Long]
            dex::u2 reg_return, // 可重复
            std::string &name_class,
            std::string &name_fun,
            std::string name_class_arg, //"[Ljava/lang/Object;"
            std::string rtype_name,  //"[Ljava/lang/Object;"
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

//        std::string name_class_arg = "[Ljava/lang/Object;";  // 普通obj
//        std::string rtype_name = "[Ljava/lang/Object;";

        cre_arr_class_args4frame(
                code_ir, reg1_size_22c, reg2_tmp, reg3_arr_22c,
                reg3_tmp,
                name_class_arg,
                insert_point);
        auto reg_method_args = reg3_tmp;

        // --------------------------- 111 -----------------------------
        // MethodType mtEnter = MethodType.methodType(Object[].class, Object[].class, long.class);
        lir::Method *f_get_MethodType_methodType_2p =
                fir_funs_def::get_MethodType_methodType_2p(code_ir);
        CHECK_OR_RETURN(f_get_MethodType_methodType_2p, false,
                        "[do_sysloader_hook_funs_B] f_get_MethodType_methodType_2p error");
        fir_funs_do::do_MethodType_methodType_2p(f_get_MethodType_methodType_2p, code_ir,
                                                 reg1_tmp, reg_method_args,
                                                 reg1_tmp,
                                                 rtype_name,
                                                 insert_point);
        auto reg_mt = reg1_tmp;  // *****************

        // -------------------------- 222 -----------------------------

        // 1) Thread t = Thread.currentThread();
        lir::Method *f_Thread_currentThread = fir_funs_def::get_Thread_currentThread(code_ir);
        CHECK_OR_RETURN(f_Thread_currentThread, false,
                        "[do_sysloader_hook_funs_B] f_Thread_currentThread error");
        // thread 对象
        fir_funs_do::do_static_0p(f_Thread_currentThread, code_ir, reg2_tmp, insert_point);


        // 2) ClassLoader cl = t.getContextClassLoader();
        lir::Method *f_thread_getContextClassLoader =
                fir_funs_def::get_thread_getContextClassLoader(code_ir);
        CHECK_OR_RETURN(f_thread_getContextClassLoader, false,
                        "[do_sysloader_hook_funs_B] f_thread_getContextClassLoader error");
        // 拿到线程的 classloader 对象
        fir_funs_do::do_thread_getContextClassLoader(f_thread_getContextClassLoader, code_ir,
                                                     reg2_tmp, reg2_tmp, insert_point);


        // 3) Class<?> clazz = Class.forName("top.feadre.fhook.FHook", true, cl);
        lir::Method *f_Class_forName_3 = fir_funs_def::get_Class_forName_3p(
                code_ir);
        CHECK_OR_RETURN(f_Class_forName_3, false,
                        "[do_sysloader_hook_funs_B] get_class_forName error");
        /// 拿到 FHook.class 对象  *********** 这个有用   reg1_tmp
        fir_funs_do::do_Class_forName_3p(f_Class_forName_3, code_ir,
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
        fir_funs_do::do_static_0p(f_MethodHandles_publicLookup, code_ir, reg3_tmp, insert_point);
        auto reg_lookup = reg3_tmp;

        // MethodHandle mhEnter = lk.findStatic(clazz, "onEnter4fhook", mtEnter);
        lir::Method *f_lookup_findStatic = fir_funs_def::get_lookup_findStatic(code_ir);
        CHECK_OR_RETURN(f_lookup_findStatic, false,
                        "[do_sysloader_hook_funs_B] f_lookup_findStatic error");
        fir_funs_do::do_lookup_findStatic(f_lookup_findStatic, code_ir,
                                          reg_lookup, reg_class, reg4_tmp, reg_mt,
                                          reg4_tmp, name_fun, insert_point);
        auto reg_mh = reg4_tmp; // 不能动 其它已可用


        // --------------------------- 444 执行 -----------------------------
        // Object[] newArgs = (Object[]) mhEnter.invokeWithArguments(rawArgs, methodId);
        lir::Method *f_methodHandle_invokeWithArguments =
                fir_funs_def::get_methodHandle_invokeWithArguments(code_ir);
        CHECK_OR_RETURN(f_methodHandle_invokeWithArguments, false,
                        "[do_sysloader_hook_funs_B] f_methodHandle_invokeWithArguments error");

        fir_funs_do::do_methodHandle_invokeWithArguments(f_methodHandle_invokeWithArguments,
                                                         code_ir,
                                                         reg_mh, reg_do_args,
                                                         reg_return,
                                                         insert_point);

        return true;
    }

    /**
     * 方案 C：Class.forName + 直接取 public 静态字段 MethodHandle（如 MH_ENTER/MH_EXIT）
     *      系统侧无需拼 MethodType；字段已在业务侧初始化完成，可靠性高、速度快、代码最简单
 *          避免每次查找/构造签名
     *
            Thread t = Thread.currentThread();
            ClassLoader cl = t.getContextClassLoader();
            Class<?> clazz = Class.forName("top.feadre.fhook.FHook", true, cl);
            Field f = clazz.getField("MH_ENTER"); // 或 "MH_EXIT"
            MethodHandle mh = (MethodHandle) f.get(null);
            Object out = mh.invokeWithArguments(wrapperArgs);

     * @return
     */
    bool do_sysloader_hook_funs_C(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp, dex::u2 reg2_tmp, dex::u2 reg3_tmp, dex::u2 reg4_tmp,
            int reg_do_args,                 // Object[]{ rawArgs, Long(methodId) }
            dex::u2 reg_return,              // out: Object[] (newArgs)
            std::string &name_class,         // "top.feadre.fhook.FHook"
            std::string &name_fun,           //  这里分区是走哪一个
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);


        // 1) Thread.currentThread().getContextClassLoader()
        auto *f_Thread_currentThread = fir_funs_def::get_Thread_currentThread(code_ir);
        CHECK_OR_RETURN(f_Thread_currentThread, false, "[C] Thread.currentThread fail");
        fir_funs_do::do_static_0p(f_Thread_currentThread, code_ir, reg2_tmp,
                                  insert_point);          // reg2_tmp = Thread

        auto *f_thread_getContextClassLoader = fir_funs_def::get_thread_getContextClassLoader(
                code_ir);
        CHECK_OR_RETURN(f_thread_getContextClassLoader, false, "[C] t.getContextClassLoader fail");
        fir_funs_do::do_thread_getContextClassLoader(f_thread_getContextClassLoader, code_ir,
                                                     reg2_tmp, reg2_tmp,
                                                     insert_point);               // reg2_tmp = ClassLoader

        // 2) Class.forName("top.feadre.fhook.FHook", true, cl) → reg2_tmp = FHook.class
        auto *f_Class_forName_3 = fir_funs_def::get_Class_forName_3p(code_ir);
        CHECK_OR_RETURN(f_Class_forName_3, false, "[C] Class.forName fail");
        fir_funs_do::do_Class_forName_3p(f_Class_forName_3, code_ir,
                                         reg3_tmp, reg4_tmp, reg2_tmp,
                                         reg2_tmp, name_class, insert_point);
        auto reg_clazz = reg2_tmp;


        // 3) 直接用 public 字段：Field f = clazz.getField("MH_ENTER"); MethodHandle mh = (MethodHandle) f.get(null)
        /// 传了1234 个寄存器
        bool res = fir_funs_do::do_get_java_method(code_ir, reg1_tmp, reg4_tmp, reg3_tmp,
                                                   reg_clazz, reg4_tmp,
                                                   name_fun, insert_point);
        if (!res)return res;
        auto reg_mh = reg4_tmp; // 不能再覆盖

        // 4) 调用 mh.invokeWithArguments(wrapperArgs)
        auto *f_mh_invoke = fir_funs_def::get_methodHandle_invokeWithArguments(code_ir);
        CHECK_OR_RETURN(f_mh_invoke, false, "[C] MethodHandle.invokeWithArguments fail");

        fir_funs_do::do_methodHandle_invokeWithArguments(
                f_mh_invoke, code_ir,
                reg_mh,            // MethodHandle
                reg_do_args,       // Object[]{ rawArgs, Long(methodId) }
                reg_return,        // out: Object[] newArgs
                insert_point
        );

        return true;
    }

    /** ----------------- 参数区 ------------------- */

    /// 生成的参数类型数组（Class[]{Object.class,Long.class}） （Class[]{Object[].class,Long.class}）
    void cre_arr_class_args4frame(
            lir::CodeIr *code_ir,
            dex::u2 reg1_size_22c, // 数组大小 也是索引 必须小于16 22c指令
            dex::u2 reg2, // value 临时（存放 Class 对象）
            dex::u2 reg3_arr_22c, // array 目标寄存器（Class[]）
            dex::u2 reg3_arr_return,  // 这里不能重复 最终接收
            std::string &name_class_arg,
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);

        // Class[] 长度=2： { Object.class, long.class }
        fir_tools::emitValToReg(code_ir, insert_point, reg1_size_22c, 2);

        dex::u2 reg_arr = reg3_arr_return < 16 ? reg3_arr_return : reg3_arr_22c;


        // new-array v<reg3_arr_22c>, v<reg1_size_22c>, [Ljava/lang/Class;
        {
            const auto class_array_type = builder.GetType("[Ljava/lang/Class;");
            auto new_arr = code_ir->Alloc<lir::Bytecode>();
            new_arr->opcode = dex::OP_NEW_ARRAY;
            new_arr->operands.push_back(
                    code_ir->Alloc<lir::VReg>(reg_arr)); // 结果 Class[] 存到 reg3_arr_22c
            new_arr->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_size_22c)); // 长度=2
            new_arr->operands.push_back(
                    code_ir->Alloc<lir::Type>(class_array_type, class_array_type->orig_index));
            code_ir->instructions.insert(insert_point, new_arr);
        }

        // index 0 = Object.class
        {
            // const-class v<reg2>, Ljava/lang/Object;
            fir_tools::emitReference2Class(code_ir, name_class_arg.c_str(),
                                           reg2, insert_point);

            // index = 0
            fir_tools::emitValToReg(code_ir, insert_point, reg1_size_22c, 0);

            // aput-object v<reg2> → v<reg3_arr_22c>[v<reg1_size_22c>]
            auto aput0 = code_ir->Alloc<lir::Bytecode>();
            aput0->opcode = dex::OP_APUT_OBJECT;
            aput0->operands.push_back(code_ir->Alloc<lir::VReg>(reg2)); // Object.class
            aput0->operands.push_back(code_ir->Alloc<lir::VReg>(reg_arr)); // Class[] 数组
            aput0->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_size_22c)); // index 0
            code_ir->instructions.insert(insert_point, aput0);
        }

        // index 1 = long.class  —— 通过 java.lang.Long.TYPE 获取
        {
            fir_tools::emitLong2Class(code_ir, reg2, insert_point);

            // index = 1
            fir_tools::emitValToReg(code_ir, insert_point, reg1_size_22c, 1);

            // aput-object v<reg2> → v<reg3_arr_22c>[v<reg1_size_22c>]
            auto aput1 = code_ir->Alloc<lir::Bytecode>();
            aput1->opcode = dex::OP_APUT_OBJECT;
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg2)); // Long.TYPE (long.class)
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg_arr)); // Class[]
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_size_22c)); // index 1
            code_ir->instructions.insert(insert_point, aput1);
        }

        // 恢复寄存
        if (reg3_arr_return >= 16) {
            fir_tools::emit_move_obj(code_ir, reg3_arr_return, reg_arr, insert_point);
        }
    }


    /// 自动找到参数寄存器 把方法的参数强转object 放在 object[arg0,arg1...] 数组中
    void cre_arr_do_args4onEnter(
            lir::CodeIr *code_ir,
            dex::u2 reg1_size_22c,       // 数组大小 也是索引 必须小于16 22c指令
            dex::u2 reg2_tmp_value,     // value
            dex::u2 reg3_arr_22c,           // array 这个 object[] 对象
            dex::u2 reg3_arr_return,  // 最终接收
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);
        const auto ir_method = code_ir->ir_method;

        bool is_static = (ir_method->access_flags & dex::kAccStatic) != 0;

        // 判断有没有参数，新建一个空参数列表
        auto param_types_list = ir_method->decl->prototype->param_types;
        auto param_types =
                param_types_list != nullptr ? param_types_list->types : std::vector<ir::Type *>();

        // === 1) 计算数组大小并 new-array === 设计：数组长度 = 参数个数 + （实例方法时的 this 1 位；静态时占位留空）
        const int arr_size = 1 + (int) param_types.size();
        LOGI("[cre_arr_do_args4onEnter] arr_size = %d", arr_size)
        fir_tools::emitValToReg(code_ir, insert_point, reg1_size_22c, arr_size);

        // 这里判断 22c
        dex::u2 reg_arr = reg3_arr_return < 16 ? reg3_arr_return : reg3_arr_22c;

        // 创建Object[]数组对象,大小由 v0 决定 new-array v2 v0 [Ljava/lang/Object;
        const auto obj_array_type = builder.GetType("[Ljava/lang/Object;");
        {
            auto allocate_array_op = code_ir->Alloc<lir::Bytecode>();
            allocate_array_op->opcode = dex::OP_NEW_ARRAY;
            allocate_array_op->operands.push_back(code_ir->Alloc<lir::VReg>(reg_arr)); // 数组对象
            allocate_array_op->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_size_22c)); // 数组大小
            allocate_array_op->operands.push_back(
                    code_ir->Alloc<lir::Type>(obj_array_type, obj_array_type->orig_index));
            code_ir->instructions.insert(insert_point, allocate_array_op);
        }

        // 填充数组  数组是4个 类型是3个
        std::vector<ir::Type *> types;
        if (!is_static) {
            // 例方法：index 0 填 this
            types.push_back(ir_method->decl->parent);
        }
        //  静态方法：index 0 已插入
        types.insert(types.end(), param_types.begin(), param_types.end());

        // 这个是参数寄存器初始 位置 （第一个参数/this）
        dex::u4 reg_arg = ir_method->code->registers - ir_method->code->ins_count;

        int arr_idx = is_static ? 1 : 0;

        // 构建参数数组赋值 --------------
        for (auto type: types) {
            dex::u4 src_reg = 0; // 临时寄存

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
            fir_tools::emitValToReg(code_ir, insert_point, reg1_size_22c, arr_idx);
//            LOGD("[cre_arr_do_args4onEnter] %s",
//                 SmaliPrinter::ToSmali(dynamic_cast<lir::Bytecode *>(*insert_point)).c_str())

            //  把v2[v0]=v1    aput-object v1 v2 v0
            auto aput_op = code_ir->Alloc<lir::Bytecode>();
            aput_op->opcode = dex::OP_APUT_OBJECT;
            aput_op->operands.push_back(code_ir->Alloc<lir::VReg>(src_reg)); // 原参数或用v1存放的包箱对象
            aput_op->operands.push_back(code_ir->Alloc<lir::VReg>(reg_arr)); // 数组对象
            aput_op->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_size_22c)); // 动态索引
            code_ir->instructions.insert(insert_point, aput_op);
//            LOGD("[cre_arr_do_args4onEnter] %s",
//                 SmaliPrinter::ToSmali(dynamic_cast<lir::Bytecode *>(*insert_point)).c_str())
            arr_idx++;
        }

        if (reg3_arr_return >= 16) {
            fir_tools::emit_move_obj(code_ir, reg3_arr_return, reg_arr, insert_point);
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
    bool cre_do_arg_2obj4onExit(
            lir::CodeIr *code_ir,
            int reg_return_orig,   // 原方法返回值所在寄存器  清空则传入-1 -> 直接给对象空 null
            dex::u2 reg_do_arg,  // 这个是返回值 单寄存器
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        LOGD("[cre_do_arg_2obj4onExit] reg_return_orig= %d", reg_return_orig)
        auto ir_method = code_ir->ir_method;
        auto proto = ir_method->decl->prototype;
        auto return_type = proto->return_type;

        // 无返回值或方法已经清空情况
        if (strcmp(return_type->descriptor->c_str(), "V") == 0 || reg_return_orig < 0) {
            // 拿到方法信息
            LOGI("[cre_do_arg_2obj4onExit] %s.%s%s -> void/null, 插入null到v%d",
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


        LOGD("[cre_do_arg_2obj4onExit] 处理完成：%s → 转换为Object到v%d",
             return_type->descriptor->c_str(), reg_do_arg);

        return true;
    }
}