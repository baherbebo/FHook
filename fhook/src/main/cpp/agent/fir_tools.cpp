//
// Created by Administrator on 2025/9/2.
//
#include "../FGlobal_flib.h"

#define TAG FEADRE_TAG("fir_tools")

#include "fir_tools.h"
#include "fsmali_printer.h"
#include "../dexter/slicer/dex_ir_builder.h"

namespace fir_tools {

    struct BytecodeConvertingVisitor : public lir::Visitor {
        lir::Bytecode *out = nullptr;

        bool Visit(lir::Bytecode *bytecode) {
            out = bytecode;
            return true;
        }
    };

    // safe casts: 支持 RTTI 与 非 RTTI 两种情况
    static lir::Bytecode *_safe_cast_to_bytecode(lir::Instruction *instr) {
        if (!instr) return nullptr;
#ifdef RTTI_ENABLED
        return dynamic_cast<lir::Bytecode *>(instr);
#else
        struct BVisitor : public lir::Visitor {
        lir::Bytecode* result = nullptr;
        bool Visit(lir::Bytecode* b) override { result = b; return true; }
    } v;
    instr->Accept(&v);
    return v.result;
#endif
    }

    static lir::VReg *_safe_cast_to_vreg(lir::Operand *op) {
        if (!op) return nullptr;
#ifdef RTTI_ENABLED
        return dynamic_cast<lir::VReg *>(op);
#else
        struct VVisitor : public lir::Visitor {
        lir::VReg* result = nullptr;
        bool Visit(lir::VReg* r) override { result = r; return true; }
    } v;
    op->Accept(&v);
    return v.result;
#endif
    }

    static lir::VRegPair *_safe_cast_to_vregpair(lir::Operand *op) {
        if (!op) return nullptr;
#ifdef RTTI_ENABLED
        return dynamic_cast<lir::VRegPair *>(op);
#else
        struct PVisitor : public lir::Visitor {
        lir::VRegPair* result = nullptr;
        bool Visit(lir::VRegPair* p) override { result = p; return true; }
    } v;
    op->Accept(&v);
    return v.result;
#endif
    }

    // 选择合适的 MOVE 变体，避免非法 opcode/编码越界
    static inline dex::Opcode pick_move_op(ir::Type::Category cat, dex::u2 dst, dex::u2 src) {
        const bool both_u8 = (dst < 256 && src < 256); // 12x
        const bool dst_u8 = (dst < 256);              // 22x

        switch (cat) {
            case ir::Type::Category::Reference:
                if (both_u8) return dex::OP_MOVE_OBJECT;            // 12x
                if (dst_u8) return dex::OP_MOVE_OBJECT_FROM16;     // 22x
                return dex::OP_MOVE_OBJECT_16;                      // 32x
            case ir::Type::Category::Scalar:
                if (both_u8) return dex::OP_MOVE;                   // 12x
                if (dst_u8) return dex::OP_MOVE_FROM16;            // 22x
                return dex::OP_MOVE_16;                             // 32x
            case ir::Type::Category::WideScalar:
                if (both_u8) return dex::OP_MOVE_WIDE;              // 12x
                if (dst_u8) return dex::OP_MOVE_WIDE_FROM16;       // 22x
                return dex::OP_MOVE_WIDE_16;                        // 32x
            default:
                SLICER_FATAL("void parameter type");
        }
    }

    /**
    * 判断 标量类型
    * @param type_descriptor4scalar
    * @return
    */
    static std::string get_scalar_type(const std::string &type_descriptor4scalar) {
        if (type_descriptor4scalar == "I") return "Ljava/lang/Integer;";
        if (type_descriptor4scalar == "Z") return "Ljava/lang/Boolean;";
        if (type_descriptor4scalar == "B") return "Ljava/lang/Byte;";
        if (type_descriptor4scalar == "S") return "Ljava/lang/Short;";
        if (type_descriptor4scalar == "C") return "Ljava/lang/Character;";
        if (type_descriptor4scalar == "J") return "Ljava/lang/Long;";
        if (type_descriptor4scalar == "F") return "Ljava/lang/Float;";
        if (type_descriptor4scalar == "D") return "Ljava/lang/Double;";
        LOGE("[get_scalar_type] 返回空字符 unsupported type descriptor: %s",
             type_descriptor4scalar.c_str())
        return ""; // 非基本类型返回空
    }

    /**
     * 标量 解包方法 包打方法统一为 valueOf
     * @param type_descriptor4scalar
     * @return
     */
    static std::string get_unbox_method(const std::string &type_descriptor4scalar) {
        if (type_descriptor4scalar == "I") return "intValue";
        if (type_descriptor4scalar == "Z") return "booleanValue";
        if (type_descriptor4scalar == "B") return "byteValue";
        if (type_descriptor4scalar == "S") return "shortValue";
        if (type_descriptor4scalar == "C") return "charValue";
        if (type_descriptor4scalar == "J") return "longValue";
        if (type_descriptor4scalar == "F") return "floatValue";
        if (type_descriptor4scalar == "D") return "doubleValue";
        LOGE("[get_unbox_method]: 找不到对应的方法 unsupported type descriptor: %s",
             type_descriptor4scalar.c_str())
        return "";
    }


    // ---------------------------------------


    /**
      * const/4 v2 #0x0 创建一个null引用
      * @param code_ir
      * @param reg1
      */
    void cre_null_reg(lir::CodeIr *code_ir, int reg1,
                      slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        auto const_null1 = code_ir->Alloc<lir::Bytecode>();
        const_null1->opcode = dex::OP_CONST_4;
        const_null1->operands.push_back(code_ir->Alloc<lir::VReg>(reg1)); // v2 = null
        const_null1->operands.push_back(code_ir->Alloc<lir::Const32>(0));
        code_ir->instructions.insert(insert_point, const_null1);
    }

    /**
     * 普通
     * 主要用于index 赋值指令
     * @param code_ir
     * @param insert_point
     * @param reg_det
     * @param value 根据value 的大小 选择最紧凑的 const 指令
     */
    void EmitConstToReg(lir::CodeIr *code_ir,
                        slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point,
                        dex::u4 reg_det,
                        dex::s4 value) {
        // vA 编码限制：const/4 需要 vA<16；const/16 与 const(31i) 需要 vA<256
        if (reg_det >= 256) {
            LOGE("[EmitConstToReg] unsupported: dest v%u >= 256 (DEX const vA is 8-bit)", reg_det);
            return;
        }

        auto bc = code_ir->Alloc<lir::Bytecode>();

        // 选最紧凑且“可编码”的指令
        if (reg_det < 16 && value >= -8 && value <= 7) {
            bc->opcode = dex::OP_CONST_4;     // 11n: vA<16, lit4∈[-8,7]
        } else if (value >= INT16_MIN && value <= INT16_MAX) {
            bc->opcode = dex::OP_CONST_16;    // 21s: vA<256, lit16
        } else {
            bc->opcode = dex::OP_CONST;       // 31i: vA<256, lit32
        }

        bc->operands.push_back(code_ir->Alloc<lir::VReg>(reg_det));
        bc->operands.push_back(code_ir->Alloc<lir::Const32>((dex::u4) value));
        code_ir->instructions.insert(insert_point, bc);
    }

    /**
     * 核心方法 **********
     * 宽类型一定要用 偶数寄存器，需要在调用时处理
     * @param insert_point
     * @param code_ir
     * @param type
     * @param reg_src
     * @param reg_dst
     */
    void unbox_scalar_value(slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point,
                            lir::CodeIr *code_ir,
                            ir::Type *type,
                            dex::u4 reg_src,   // 存放 Object 的寄存器 (例如 tmp_obj_reg)
                            dex::u4 reg_dst) { // 要写入的目标寄存器（若宽类型 reg_dst 为 base register）
        SLICER_CHECK(code_ir && type);
        ir::Builder builder(code_ir->dex_ir);

        bool is_wide = (type->GetCategory() == ir::Type::Category::WideScalar);

        // --- 范围检查：确保不越界 ---
        const dex::u2 regs_size = code_ir->ir_method->code->registers;
        if (!is_wide) {
            if (reg_dst >= regs_size) {
                LOGE("[unbox_scalar_value] reg_dst out of range: %u / %u", reg_dst, regs_size);
                return;
            }
        } else {
            if (reg_dst + 1 >= regs_size) {
                LOGE("[unbox_scalar_value] wide reg_dst pair out of range: %u,%u / %u", reg_dst,
                     reg_dst + 1, regs_size);
                return;
            }
        }

        std::string orig_return_desc = type->descriptor->c_str();
        // 拿到基础的类型
        std::string boxed_name = get_scalar_type(orig_return_desc);
        std::string unbox_name = get_unbox_method(orig_return_desc);
        SLICER_CHECK(!boxed_name.empty() && !unbox_name.empty());

        auto boxed_ir_type = builder.GetType(boxed_name.c_str());

        // 这里要进行null 判断   创建标签
        auto lbl_is_null = code_ir->Alloc<lir::Label>(lir::kInvalidOffset);
        auto lbl_after = code_ir->Alloc<lir::Label>(lir::kInvalidOffset);

        // if-eqz v<src>, :is_null
        {
            auto bc = code_ir->Alloc<lir::Bytecode>();
            bc->opcode = dex::OP_IF_EQZ;
            bc->operands.push_back(code_ir->Alloc<lir::VReg>(reg_src));
            bc->operands.push_back(code_ir->Alloc<lir::CodeLocation>(lbl_is_null));
            code_ir->instructions.insert(insert_point, bc);
        }

        // --- 非 null 正常路径：check-cast -> invoke-virtual xxxValue() -> move-result* ---
        {
            // 1) check-cast v<reg_src>, <BoxedType> (告诉 verifier 这是 Integer/Long/...)
            auto cc = code_ir->Alloc<lir::Bytecode>();
            cc->opcode = dex::OP_CHECK_CAST;
            cc->operands.push_back(code_ir->Alloc<lir::VReg>(reg_src));
            cc->operands.push_back(code_ir->Alloc<lir::Type>(
                    boxed_ir_type,
                    boxed_ir_type->orig_index));
            code_ir->instructions.insert(insert_point, cc);


            // 2) invoke-virtual { v<reg_src> }, <BoxedType>-><unbox_name>()<prim>
            auto proto = builder.GetProto(type, nullptr); // no params
            auto method_decl = builder.GetMethodDecl(
                    builder.GetAsciiString(unbox_name.c_str()),
                    proto,
                    boxed_ir_type);
            auto lir_method = code_ir->Alloc<lir::Method>(method_decl, method_decl->orig_index);

            // 执行对象方法转换
            // INVOKE_VIRTUAL expects a VRegList (not a single VReg).
            // 构造 VRegList 并把 reg_src 放进去（this）
            auto args_list = code_ir->Alloc<lir::VRegList>();
            args_list->registers.push_back(reg_src);

            auto invoke = code_ir->Alloc<lir::Bytecode>();
            invoke->opcode = dex::OP_INVOKE_VIRTUAL; // invoke-virtual { v_src }, Ljava/lang/Integer;->intValue()I
            invoke->operands.push_back(args_list);
            invoke->operands.push_back(lir_method);
            code_ir->instructions.insert(insert_point, invoke);


            // 3) move-result / move-result-wide 到 reg_dst
            auto move_res = code_ir->Alloc<lir::Bytecode>();
            if (!is_wide) {
                move_res->opcode = dex::OP_MOVE_RESULT;
                move_res->operands.push_back(code_ir->Alloc<lir::VReg>(reg_dst));
            } else {
                // 宽类型要用 VRegPair 作为目标操作数 宽类型一定要用 偶数寄存器，需要在调用时处理
                move_res->opcode = dex::OP_MOVE_RESULT_WIDE;
                move_res->operands.push_back(code_ir->Alloc<lir::VRegPair>(reg_dst));
            }
            code_ir->instructions.insert(insert_point, move_res);

            // 跳过 null 分支
            auto go = code_ir->Alloc<lir::Bytecode>();
            go->opcode = dex::OP_GOTO;
            go->operands.push_back(code_ir->Alloc<lir::CodeLocation>(lbl_after));
            code_ir->instructions.insert(insert_point, go);
        }

        // --- :is_null 分支：写入默认零值 ---
        code_ir->instructions.insert(insert_point, lbl_is_null); // 放置标签
        if (!is_wide) {
            // const/4 v<dst>, #0
            auto c0 = code_ir->Alloc<lir::Bytecode>();
            c0->opcode = dex::OP_CONST_4;
            c0->operands.push_back(code_ir->Alloc<lir::VReg>(reg_dst));
            c0->operands.push_back(code_ir->Alloc<lir::Const32>(0));
            code_ir->instructions.insert(insert_point, c0);
        } else {
            // const-wide v<dst..dst+1>, #0L
            auto c0w = code_ir->Alloc<lir::Bytecode>();
            c0w->opcode = dex::OP_CONST_WIDE;
            c0w->operands.push_back(code_ir->Alloc<lir::VRegPair>(reg_dst));
            c0w->operands.push_back(code_ir->Alloc<lir::Const64>(0));
            code_ir->instructions.insert(insert_point, c0w);
        }
        code_ir->instructions.insert(insert_point, lbl_after); // 放置标签
    }

    /**
     * 这里只处理基础类型,
     * @param insert_point
     * @param code_ir
     * @param type
     * @param reg_src
     * @param reg_dst
     */
    void box_scalar_value(slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point,
                          lir::CodeIr *code_ir,
                          ir::Type *type,
                          dex::u4 reg_src,
                          dex::u4 reg_dst) {
        // 前置条件检查：确保只处理非引用类型（与调用方逻辑呼应）
        SLICER_CHECK(type != nullptr);
        SLICER_CHECK(code_ir != nullptr);
        SLICER_CHECK(type->GetCategory() != ir::Type::Category::Reference &&
                     "BoxValue方法不应处理引用类型");
        SLICER_CHECK(type->descriptor != nullptr && "类型描述符不能为空");

        // 空类型已经在外面处理了
        bool is_wide = (type->GetCategory() == ir::Type::Category::WideScalar);
        const dex::u2 regs_size = code_ir->ir_method->code->registers;

        // --- 寄存器边界检查 ---
        // 源寄存器（可能是宽）必须完整落在范围内
        if ((!is_wide && reg_src >= regs_size) ||
            (is_wide && (reg_src + 1 >= regs_size))) {
            LOGE("[box_scalar_value] reg_src out of range: v%u (wide=%d) / %u",
                 reg_src, is_wide, regs_size);
            return;
        }
        // 目标寄存器（对象目标）必须在范围内
        if (reg_dst >= regs_size) {
            LOGE("[box_scalar_value] reg_dst out of range: v%u / %u", reg_dst, regs_size);
            return;
        }

        std::string orig_return_desc = type->descriptor->c_str();

        // 拿到基础的类型 如 "Ljava/lang/Integer;"
        std::string boxed_type_name = get_scalar_type(orig_return_desc);
        SLICER_CHECK(!boxed_type_name.empty() && "未识别的基本类型");

        ir::Builder builder(code_ir->dex_ir);
        auto boxed_type = builder.GetType(boxed_type_name.c_str());


        std::vector<ir::Type *> param_types;
        param_types.push_back(type);
        auto ir_proto = builder.GetProto(boxed_type,
                                         builder.GetTypeList(param_types));

        // 统一调用 静态的 valueOf
        auto ir_method_decl = builder.GetMethodDecl(
                builder.GetAsciiString("valueOf"), ir_proto, boxed_type);

        auto boxing_method = code_ir->Alloc<lir::Method>(
                ir_method_decl, ir_method_decl->orig_index);

        // invoke-static/range {reg_src ..} -> Boxed.valueOf
        auto args = code_ir->Alloc<lir::VRegRange>(reg_src, 1 + is_wide);
        auto boxing_invoke = code_ir->Alloc<lir::Bytecode>();
        boxing_invoke->opcode = dex::OP_INVOKE_STATIC_RANGE;
        boxing_invoke->operands.push_back(args);
        boxing_invoke->operands.push_back(boxing_method);
        code_ir->instructions.insert(insert_point, boxing_invoke);

        // move-result-object vX
        auto move_result = code_ir->Alloc<lir::Bytecode>();
        move_result->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_result->operands.push_back(code_ir->Alloc<lir::VReg>(reg_dst));
        code_ir->instructions.insert(insert_point, move_result);
    }

    /**
    * 创建返回指令 会自动判断返回值类型进行处理  **** 核心方法
    * @param code_ir
    * @param return_type
    * @param reg_return 指定返回值寄存器编号： 清空 或无返回值为-1 表示自动分配
    */
    void cre_return_code(lir::CodeIr *code_ir,
                         ir::Type *return_type,
                         int reg_return,
                         slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        // 无返回值处理
        if (strcmp(return_type->descriptor->c_str(), "V") == 0) {
            auto ret = code_ir->Alloc<lir::Bytecode>();
            ret->opcode = dex::OP_RETURN_VOID;
            code_ir->instructions.push_back(ret);
            return;
        }

        // 确定操作码和类型特性
        dex::Opcode move_opcode;
        dex::Opcode return_opcode;
        bool is_wide = false;
        bool is_reference = false;

        // 返回值类型判断
        switch (return_type->GetCategory()) {
            case ir::Type::Category::Scalar:  // int, float, boolean等
//                LOGD("[cre_return_code] Scalar，return_type->GetCategory= %s",
//                     return_type->descriptor->c_str())
                break;
            case ir::Type::Category::WideScalar:  // long, double
                is_wide = true;
//                LOGD("[cre_return_code] WideScalar，return_type->GetCategory= %s",
//                     return_type->descriptor->c_str())
                break;
            case ir::Type::Category::Reference:  // 对象引用类型
                is_reference = true;
//                LOGD("[cre_return_code] Reference，return_type->GetCategory= %s",
//                     return_type->descriptor->c_str())
                break;
            default:
                SLICER_CHECK(false && "不支持的返回类型");
                LOGE("[cre_return_code] 不支持的返回类型: %s", return_type->descriptor->c_str())
                return;
        }

        // 方法已确定有返回值
        int reg_num = reg_return;
        if (reg_num == -1) {
            // 随便 --- 选择本地区最后一个寄存器（本地区 = regs - ins_count）
            SLICER_CHECK(code_ir && code_ir->ir_method);
            const auto ir_method = code_ir->ir_method;
            const dex::u4 regs = ir_method->code->registers;
            const dex::u4 ins_count = ir_method->code->ins_count;
            // 本地区大小：
            dex::s4 locals = (dex::s4) regs - (dex::s4) ins_count;
            if (locals <= 0) {
                // 没有本地寄存器可用 —— 这种情况应由调用方扩展寄存器或传入 reg_return
                LOGE("[cre_return_code] no local registers available; caller must provide a target reg");
                SLICER_FATAL("no local regs");
                return;
            }

            // 选最后一个索引作为起点： base = regs - ins_count - 1
            reg_num = (int) (regs - ins_count - 1);
            if (is_wide) {
                // 宽类型需偶数对齐（退位以保证不越界）
                if (reg_num - 1 < 0) {
                    LOGE("[cre_return_code] cannot align wide reg; out of range");
                    SLICER_FATAL("cannot align wide reg");
                    return;
                }
                reg_num -= 1;
            }

            // 创建默认值  直接创建 null 就不需要转换
            if (is_reference) {
                // const/4 vX, #null  -> return-object vX
                EmitConstToReg(code_ir, insert_point, reg_num, 0);

                // return-object vX
                auto ret = code_ir->Alloc<lir::Bytecode>();
                ret->opcode = dex::OP_RETURN_OBJECT;
                ret->operands.push_back(code_ir->Alloc<lir::VReg>(reg_num));
                code_ir->instructions.push_back(ret);
                return;
            }

            if (is_wide) {
                // const-wide vpair, #0L
                auto init = code_ir->Alloc<lir::Bytecode>();
                init->opcode = dex::OP_CONST_WIDE;
                init->operands.push_back(code_ir->Alloc<lir::VRegPair>(reg_num)); // 必须 VRegPair
                init->operands.push_back(code_ir->Alloc<lir::Const64>(0));
                code_ir->instructions.push_back(init);

                // return-wide vpair
                auto ret = code_ir->Alloc<lir::Bytecode>();
                ret->opcode = dex::OP_RETURN_WIDE;
                ret->operands.push_back(code_ir->Alloc<lir::VRegPair>(reg_num)); // 必须 VRegPair
                code_ir->instructions.push_back(ret);
                return;
            }

            // 非宽非引用：普通 scalar
            {
                // 对于小常量优先使用 const/4，但这里用 const/4 (0) 足够
                EmitConstToReg(code_ir, insert_point, reg_num, 0);

                auto ret = code_ir->Alloc<lir::Bytecode>();
                ret->opcode = dex::OP_RETURN;
                ret->operands.push_back(code_ir->Alloc<lir::VReg>(reg_num));
                code_ir->instructions.push_back(ret);
                return;
            }

        }

        // 指定了 reg_return：从 Object -> 真实返回类型，再 return* ----------------------
        // 直接赋值 宽类型
        LOGI("[cre_return_code] 使用指定返回寄存器 reg_return: V%d (类型: %s) ",
             reg_num, return_type->descriptor->c_str());

        // 这里需要转换
        if (is_reference) {
            // 先转再返回
            ir::Builder builder(code_ir->dex_ir);
            auto ret_ir_type = builder.GetType(return_type->descriptor->c_str());

            // check-cast vX, type      check-cast v<dst>, <ReturnRefType>
            auto cc = code_ir->Alloc<lir::Bytecode>();
            cc->opcode = dex::OP_CHECK_CAST;
            cc->operands.push_back(code_ir->Alloc<lir::VReg>(reg_num));
            cc->operands.push_back(code_ir->Alloc<lir::Type>(
                    ret_ir_type, ret_ir_type->orig_index));
            code_ir->instructions.push_back(cc);

            // return-object vX
            auto ret = code_ir->Alloc<lir::Bytecode>();
            ret->opcode = dex::OP_RETURN_OBJECT;
            ret->operands.push_back(code_ir->Alloc<lir::VReg>(reg_num));
            code_ir->instructions.push_back(ret);
            return;
        }

        // 这里已确定是标量 从 Object 解包到 v<dst>
        unbox_scalar_value(insert_point,
                           code_ir,
                           return_type,
                           reg_num,     // src: Object 就在 dst
                           reg_num);    // dst: 覆盖同一个寄存器

        if (is_wide) {
            // return-wide vpair
            auto ret = code_ir->Alloc<lir::Bytecode>();
            ret->opcode = dex::OP_RETURN_WIDE;
            ret->operands.push_back(code_ir->Alloc<lir::VRegPair>(reg_num)); // 必须 VRegPair
            code_ir->instructions.push_back(ret);
            return;
        }

        // 非宽非引用：普通 scalar
        {
            auto ret = code_ir->Alloc<lir::Bytecode>();
            ret->opcode = dex::OP_RETURN;
            ret->operands.push_back(code_ir->Alloc<lir::VReg>(reg_num));
            code_ir->instructions.push_back(ret);
            return;
        }
    }

    /**
     * 就是把 Object[] 数组还原成参数
     * @param code_ir
     * @param insert_point
     * @param reg_args
     * @param base_param_reg
     * @param reg_tmp_obj
     * @param reg_tmp_idx
     */
    void restore_reg_params4type(lir::CodeIr *code_ir,
                                 dex::u4 reg_args,            // enter 返回的 Object[] 寄存器 (e.g. v0)
                                 dex::u4 base_param_reg,     // 参数区起始寄存器 (num_reg_non_param)
                                 dex::u4 reg_tmp_obj,       // 临时寄存器存 aget-object 结果 (e.g. v1)
                                 dex::u4 reg_tmp_idx,// 临时寄存器做数组索引 (e.g. v2)
                                 slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point
    ) {

        SLICER_CHECK(code_ir && code_ir->ir_method);
        auto ir_method = code_ir->ir_method;
        ir::Builder builder(code_ir->dex_ir);

        // 先把onenter的返回强转为 Object[]
        auto arr_type = builder.GetType("[Ljava/lang/Object;");
        auto check_cast_arr = code_ir->Alloc<lir::Bytecode>();
        check_cast_arr->opcode = dex::OP_CHECK_CAST;
        check_cast_arr->operands.push_back(code_ir->Alloc<lir::VReg>(reg_args));
        check_cast_arr->operands.push_back(
                code_ir->Alloc<lir::Type>(arr_type, arr_type->orig_index));
        code_ir->instructions.insert(insert_point, check_cast_arr);

        // 开始恢复参数数组
        const bool is_static = (ir_method->access_flags & dex::kAccStatic) != 0;
        const auto &proto = ir_method->decl->prototype; // 方法的签名
        const auto param_list = proto->param_types;     // 参数类型列表 (不含 this)
        const dex::u4 param_count = param_list ? (dex::u4) param_list->types.size() : 0u;

        // 槽位游标和数组索引
        dex::u4 slot = 0;         // 写回参数寄存器槽从 base_param_reg + slot
        dex::u4 index_arr = 0;    // 从 reg_args[index_arr] 读取

        // 如果是实例方法，按你打包约定，index_arr=0 对应 this
        if (!is_static) {
            // 把 index 0 的从数组拿出来
            EmitConstToReg(code_ir, insert_point, reg_tmp_idx, (dex::s4) index_arr);

            // aget-object reg_tmp_obj, reg_args, reg_tmp_idx
            {
                auto aget = code_ir->Alloc<lir::Bytecode>();
                aget->opcode = dex::OP_AGET_OBJECT;
                aget->operands.push_back(code_ir->Alloc<lir::VReg>(reg_tmp_obj));
                aget->operands.push_back(code_ir->Alloc<lir::VReg>(reg_args));
                aget->operands.push_back(code_ir->Alloc<lir::VReg>(reg_tmp_idx));
                code_ir->instructions.insert(insert_point, aget);
            }

            // check-cast reg_tmp_obj -> precise type of 'this' (ir_method->decl->parent)
            {
                ir::Type *this_type = ir_method->decl->parent;
                if (this_type) {
                    auto this_ir_type = builder.GetType(this_type->descriptor->c_str());
                    auto cc = code_ir->Alloc<lir::Bytecode>();
                    cc->opcode = dex::OP_CHECK_CAST;
                    cc->operands.push_back(code_ir->Alloc<lir::VReg>(reg_tmp_obj));
                    cc->operands.push_back(
                            code_ir->Alloc<lir::Type>(this_ir_type, this_ir_type->orig_index));
                    code_ir->instructions.insert(insert_point, cc);
                } else {
                    LOGW("[restore_reg_params4type] this_type is 不存在，需要实")
                }
            }

            // move-object base_param_reg+slot, reg_tmp_obj
            {
                auto mv = code_ir->Alloc<lir::Bytecode>();
                mv->opcode = dex::OP_MOVE_OBJECT;
                mv->operands.push_back(code_ir->Alloc<lir::VReg>(base_param_reg + slot));
                mv->operands.push_back(code_ir->Alloc<lir::VReg>(reg_tmp_obj));
                code_ir->instructions.insert(insert_point, mv);
            }

            slot += 1;
            index_arr += 1;
        } else {
            // 若静态方法并且你约定数组第0位是 null 占位 → 从 index_arr = 1 开始
            // 如果没有占位（即数组是紧凑放参数），请把这里设为 0
            index_arr = 1;
        }

        // 逐个参数恢复（按 proto 顺序）  此时 index_arr= 1, slot = 1或0
        for (dex::u4 p = 0; p < param_count; ++p) {
            ir::Type *ptype = param_list->types[p];
            SLICER_CHECK(ptype != nullptr);

            // Load array index
            EmitConstToReg(code_ir, insert_point, reg_tmp_idx, (dex::s4) index_arr);

            // 把值取出来先 aget-object reg_tmp_obj, reg_args, reg_tmp_idx
            {
                auto aget = code_ir->Alloc<lir::Bytecode>();
                aget->opcode = dex::OP_AGET_OBJECT;
                aget->operands.push_back(code_ir->Alloc<lir::VReg>(reg_tmp_obj));
                aget->operands.push_back(code_ir->Alloc<lir::VReg>(reg_args));
                aget->operands.push_back(code_ir->Alloc<lir::VReg>(reg_tmp_idx));
                code_ir->instructions.insert(insert_point, aget);
            }

            auto cat = ptype->GetCategory();
            if (cat == ir::Type::Category::Reference) {
                // 把参数的类型取出来强转
                auto declared_type = builder.GetType(ptype->descriptor->c_str());
                auto cc = code_ir->Alloc<lir::Bytecode>();
                cc->opcode = dex::OP_CHECK_CAST;
                cc->operands.push_back(code_ir->Alloc<lir::VReg>(reg_tmp_obj));
                cc->operands.push_back(
                        code_ir->Alloc<lir::Type>(declared_type, declared_type->orig_index));
                code_ir->instructions.insert(insert_point, cc);

                // move-object 写回
                auto mv = code_ir->Alloc<lir::Bytecode>();
                mv->opcode = dex::OP_MOVE_OBJECT;
                mv->operands.push_back(code_ir->Alloc<lir::VReg>(base_param_reg + slot));
                mv->operands.push_back(code_ir->Alloc<lir::VReg>(reg_tmp_obj));
                code_ir->instructions.insert(insert_point, mv);

                slot += 1;
                index_arr += 1;
            } else if (cat == ir::Type::Category::Scalar) {
                // 基本类型（非宽） object -> int
                unbox_scalar_value(insert_point, code_ir, ptype, reg_tmp_obj,
                                   base_param_reg + slot);
                slot += 1;
                index_arr += 1;
            } else if (cat == ir::Type::Category::WideScalar) {
                // 确保不越过参数槽位（ins_count 是“参数区”总槽数）
                SLICER_CHECK(slot + 2 <= ir_method->code->ins_count);

                // 宽类型 long/double 占两个寄存器槽 (写入 base_param_reg+slot 和 slot+1)
                unbox_scalar_value(insert_point, code_ir, ptype, reg_tmp_obj,
                                   base_param_reg + slot);
                slot += 2;   // 宽类型占 2 个槽
                index_arr += 1;
            } else {
                SLICER_FATAL("Unexpected param category");
            }

            // 防护：不要越过 ins_count
            SLICER_CHECK(slot <= ir_method->code->ins_count);
        }

        // 最后断言槽位数一致
        SLICER_CHECK(slot == ir_method->code->ins_count &&
                     "[restore-params] filled slots != ins_count");
    }

    /**
     *
     * 要执行原方法时，必须要在执行前搬运
     假设原总寄存器数 old_regs=5，参数数 ins_count=2，则原参数起始地址是 5-2=3（参数存在 3、4 号寄存器）；
若新增 shift=2 个寄存器，新总寄存器数 new_regs=7，新参数起始地址应为 7-2=5（参数需移到 5、6 号寄存器）；
此时 reg 初始值是 5（新参数起始地址），而原有参数还在旧地址 3、4，需要把它们移到 5、6—— 这就是 reg - shift（5-2=3）的含义：旧参数地址 = 新参数地址 - 新增寄存器数
     * @param code_ir
     * @param position 当前插入指令位置
     * @param shift 新增的寄存器数量 不会小于0
     *
     */
    void restore_reg_params4shift(lir::CodeIr *code_ir,
                                  slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point,
                                  dex::u4 shift) {
        LOGD("[restore_reg_params4shift] start...")

        auto *ir_method = code_ir->ir_method;
        auto *code = ir_method->code;

        const dex::u2 regs = code->registers; // 扩展后的总寄存器数
        const dex::u2 ins = code->ins_count; // 方法的参数总数（含 this）
        SLICER_CHECK(regs >= ins);

        if (shift == 0 || ins == 0) {
            LOGD("[restore_reg_params4shift] shift=%u 或无参数，跳过恢复", shift);
            return;
        }

        // 1) 构建参数类型列表：非静态方法需包含 this 指针（作为第一个参数）
        std::vector<ir::Type *> param_types;
        if ((ir_method->access_flags & dex::kAccStatic) == 0) {
            param_types.push_back(ir_method->decl->parent);
        }
        if (ir_method->decl->prototype->param_types != nullptr) {
            const auto &orig_param_types = ir_method->decl->prototype->param_types->types;
            param_types.insert(param_types.end(), orig_param_types.begin(), orig_param_types.end());
        }

        // 2) 生成移动计划（src=扩展后参数起点，dst=扩展前参数起点）
        struct Plan {
            ir::Type::Category cat;
            dex::u2 src;
            dex::u2 dst;
            dex::u2 width;
        };
        std::vector<Plan> plans;

        const dex::u2 src_base = regs - ins;     // 扩展后参数起点
        SLICER_CHECK(src_base >= shift);         // 防止下溢（理论上 shift=新增本地数）
        const dex::u2 dst_base = src_base - shift;

        dex::u2 src = src_base;
        dex::u2 dst = dst_base;

        for (auto *t: param_types) {
            auto cat = t->GetCategory();
            dex::u2 w = (cat == ir::Type::Category::WideScalar) ? 2 : 1;

            SLICER_CHECK(src + w <= regs);
            SLICER_CHECK(dst + w <= regs);

            plans.push_back({cat, src, dst, w});
            src += w;
            dst += w;
        }

        // 3) 选方向：shift>0 → 正向（从左到右）；shift<0 → 反向
        int i = (shift > 0) ? 0 : (int) plans.size() - 1;
        int end = (shift > 0) ? (int) plans.size() : -1;
        int step = (shift > 0) ? 1 : -1;

        // 4) 执行移动（自动选择 MOVE 变体）
        for (; i != end; i += step) {
            auto &p = plans[i];
            if (p.src == p.dst) continue; // 无需移动

            auto op = pick_move_op(p.cat, p.dst, p.src);
            auto *bc = code_ir->Alloc<lir::Bytecode>();
            bc->opcode = op;

            if (p.cat == ir::Type::Category::WideScalar) {
                bc->operands.push_back(code_ir->Alloc<lir::VRegPair>(p.dst));
                bc->operands.push_back(code_ir->Alloc<lir::VRegPair>(p.src));
            } else {
                bc->operands.push_back(code_ir->Alloc<lir::VReg>(p.dst));
                bc->operands.push_back(code_ir->Alloc<lir::VReg>(p.src));
            }
            code_ir->instructions.insert(insert_point, bc);

            // 5) （可选）在 shift>0 且“下一参数为单槽，且它的目标正好落在本宽参的源 high-half”时，
            //    先对该 high-half 显式写 0（32-bit 定义），以解除 wide 标记，避免随后的单槽类型冲突。
            if (shift > 0 && p.cat == ir::Type::Category::WideScalar) {
                dex::u2 src_hi = p.src + 1;
                if ((i + 1) < (int) plans.size()
                    && plans[i + 1].width == 1
                    && plans[i + 1].dst == src_hi) {
                    EmitConstToReg(code_ir, insert_point, /*reg_det=*/src_hi, /*value=*/0);
                }
            }
        }

        LOGD("[restore_reg_params4shift] done. regs=%u ins=%u shift=%u", regs, ins, shift);
    }

    /// 没有就返回最后
    slicer::IntrusiveList<lir::Instruction>::Iterator
    find_return_code(lir::CodeIr *code_ir) {
        // 找到return指令前的位置  ------------ 后向前找
        auto insert_point = code_ir->instructions.end();
        auto it = insert_point;

        while (it != code_ir->instructions.begin()) {
            --it;
            lir::Instruction *instr = *it;

            BytecodeConvertingVisitor visitor;
            instr->Accept(&visitor);
            lir::Bytecode *bytecode = visitor.out;

            // 跳过转换失败的指令
            if (bytecode == nullptr) {
                continue;
            }

            // 检查是否为返回类型指令
            dex::Opcode opcode = bytecode->opcode;
            if (opcode == dex::OP_RETURN_VOID ||
                opcode == dex::OP_RETURN ||
                opcode == dex::OP_RETURN_OBJECT ||
                opcode == dex::OP_RETURN_WIDE) {
                // 直接返回指向 return 的 iterator
                LOGI("[cre_return_code] 找到返回指令: %s",
                     SmaliPrinter::ToSmali(bytecode).c_str())
                return it;
            }
        }
        LOGI("[cre_return_code] 没有找到 return 指令")
        return insert_point;
    }

    ///没有就返回最开始
    slicer::IntrusiveList<lir::Instruction>::Iterator
    find_first_code(lir::CodeIr *code_ir) {
        // 找到return指令前的位置  ------------ 后向前找
        auto insert_point = code_ir->instructions.begin();
        auto it = insert_point;

        while (it != code_ir->instructions.end()) {
            ++it;
            lir::Instruction *instr = *it;

            BytecodeConvertingVisitor visitor;
            instr->Accept(&visitor);
            lir::Bytecode *bytecode = visitor.out;

            if (bytecode != nullptr) {
                LOGI("[find_first_code] 找到第一条指令: %s",
                     SmaliPrinter::ToSmali(bytecode).c_str())
                // 找到第一条指令
                return it;
            }
        }
        LOGW("[find_first_code] 没有找到任何指令")
        return insert_point;
    }

    /**
     * 动态增加寄存器数量 新增寄存器在前面
         原配置 registers=3，ins_count=2 → 新增后 registers=5，ins_count=2
         v0（本地寄存器）、v1（第 1 个参数）、v2（第 2 个参数，参数在末尾）
         v0、v1（新增的 2 个本地寄存器）、v2（本地寄存器）、v3（第 1 个参数）、v4（第 2 个参数，仍在末尾）
     * @param code_ir
     * @param regs_count
     * @return
     */
    dex::u2 req_reg(lir::CodeIr *code_ir, dex::u2 regs_count) {
//        LOGD("[req_reg] start...")
        const auto ir_method = code_ir->ir_method;

        dex::u2 orig_registers = ir_method->code->registers;
        auto num_reg_non_param = orig_registers - ir_method->code->ins_count;
        bool needsExtraRegs = num_reg_non_param < regs_count;
        dex::u2 num_add_reg = 0; // 返回值 新增的寄存器数

        if (needsExtraRegs) {
            // 扩展总寄存器数：新增的数量 = 插桩需要的数量 - 原局部变量区大小
            num_add_reg = regs_count - num_reg_non_param;
            code_ir->ir_method->code->registers += num_add_reg;
            LOGI("[req_reg] 已扩展  原总寄存器数= %d，扩后寄存器= %d,参数reg= %d, 非参数reg（参数开始index）= %d, 原本地= %d, 需增加= %d",
                 orig_registers,
                 code_ir->ir_method->code->registers,
                 ir_method->code->ins_count,
                 regs_count,
                 num_reg_non_param,
                 num_add_reg)
        } else {
            LOGI("[req_reg] 不需要扩展 总寄存器= %d,参数reg= %d, 非参数reg（参数开始index）= %d, 原本地= %d",
                 code_ir->ir_method->code->registers,
                 ir_method->code->ins_count,
                 regs_count,
                 num_reg_non_param)
        }

        return num_add_reg;

    }

    /**
     * 找到最后的 return 指令并返回寄存器号；若 is_wide_out 非空则写入是否为宽类型
        返回值：寄存器号（普通返回或宽类型的 base_reg），失败或 return-void 返回 -1
     * @param code_ir
     * @param is_wide_out
     * @return 返回的是起始寄存器号
     *          [replace_fun4ty] return_register= 2, is_wide_out=1
     */
    int find_return_register(lir::CodeIr *code_ir, bool *is_wide_out) {
        if (!code_ir) return -1;
        if (is_wide_out) *is_wide_out = false;

        // 如果 instruction 列表为空
        if (code_ir->instructions.begin() == code_ir->instructions.end()) {
            LOGE("[find_return_register] code_ir 为空")
            return -1;
        }

        // 从后向前查找最后一个 return 指令
        auto it = code_ir->instructions.end();
        while (it != code_ir->instructions.begin()) {
            --it;
            lir::Instruction *instr = *it;

            // 把 instruction 安全转换为 bytecode（兼容 RTTI / 非 RTTI）
            lir::Bytecode *bytecode = _safe_cast_to_bytecode(instr);
            if (!bytecode) continue; // 不是 Bytecode，跳过

            auto opcode = bytecode->opcode;

            // 处理各种 return
            switch (opcode) {
                case dex::OP_RETURN_VOID:
                    // 无返回值
                    if (is_wide_out) *is_wide_out = false;
                    return -1;

                case dex::OP_RETURN:
                case dex::OP_RETURN_OBJECT: {
                    // 期望一个 VReg operand
                    if (bytecode->operands.size() < 1) {
                        LOGW("[find_return_register] return-object/return operand 缺失，继续回溯");
                        continue;   // <-- 改这里
                    }
                    lir::Operand *op = bytecode->operands[0];
                    lir::VReg *vr = _safe_cast_to_vreg(op);
                    if (!vr) {
                        LOGW("[find_return_register] return operand 不是 VReg（期待普通返回寄存器）");
                        return -1;
                    }
                    if (is_wide_out) *is_wide_out = false;
                    LOGI("[find_return_register] return 找到返回寄存器: %u", vr->reg);
                    return static_cast<int>(vr->reg);
                }

                case dex::OP_RETURN_WIDE: {
                    if (bytecode->operands.size() < 1) {
                        LOGW("[find_return_register] return-wide operand 缺失，继续回溯");
                        continue;
                    }
                    lir::Operand *op = bytecode->operands[0];
                    lir::VRegPair *vp = _safe_cast_to_vregpair(op);
                    if (!vp) {
                        LOGW("[find_return_register] return-wide operand 不是 VRegPair");
                        return -1;
                    }
                    if (is_wide_out) *is_wide_out = true;
                    return static_cast<int>(vp->base_reg);
                }

                default:
                    // 不是 return 指令，继续向前寻找
                    continue;
            } // switch
        } // while

        // 没找到任何 return
        LOGE("[find_return_register] 没有找到 return 指令")
        return -1;
    }

    /**
    * 找 return 指令之前插入
    * @param code_ir
    * @param is_empty_fun
    */
    void clear_original_instructions(lir::CodeIr *code_ir) {
        auto it = code_ir->instructions.begin();
        while (it != code_ir->instructions.end()) {
            auto curr = *it;
            ++it;
            code_ir->instructions.Remove(curr);
        }
    }

    /// 自动选一个不冲突，可选是否重复，的普通寄存器
    int pick_reg4one(lir::CodeIr *code_ir,
                     int reg_target,        // 返回不能与这个寄存器冲突
                     bool is_wide_target,   // reg_target 是否为宽寄存器起点（需要避开 reg_target 与 reg_target+1）
                     bool is_repetition)    // 是否允许返回与 reg_target 相同（仅在不冲突前提下）
    {
        SLICER_CHECK(code_ir && code_ir->ir_method && code_ir->ir_method->code);
        const auto code = code_ir->ir_method->code;

        const dex::u2 regs_size = code->registers;   // 总寄存器数
        const dex::u2 ins_size = code->ins_count;   // 参数寄存器数（高位）
        const int locals = static_cast<int>(regs_size - ins_size);
        if (locals <= 0) {
            LOGE("[pick_reg4one] locals=0，无可用本地寄存器");
            return -1;
        }

        auto conflict_with_target = [&](int v) -> bool {
            if (reg_target < 0) return false;
            if (is_wide_target) {
                // 目标为宽寄存器：v 不能等于 reg_target 或 reg_target+1
                return (v == reg_target) || (v == reg_target + 1);
            } else {
                // 目标为普通寄存器：v 不能等于 reg_target 才算“冲突”
                return (v == reg_target);
            }
        };

        // 若允许“重复”，且不与 reg_target 冲突，优先返回 reg_target（仅当其在 locals 内）
        if (is_repetition && reg_target >= 0 && reg_target < locals &&
            !conflict_with_target(reg_target)) {
            return reg_target;
        }

        // 从 v0 起找第一个不冲突的普通寄存器
        for (int v = 0; v < locals; ++v) {
            if (!conflict_with_target(v)) {
                // 若不允许重复，还需排除 v == reg_target（仅在窄目标场景有意义）
                if (!is_repetition && reg_target >= 0 && !is_wide_target && v == reg_target) {
                    continue;
                }
                return v;
            }
        }

        LOGE("[pick_reg4one] 无可用普通寄存器 (locals=%d, reg_target=%d, wide_target=%d, repetition=%d)",
             locals, reg_target, is_wide_target, is_repetition);
        return -1;
    }


    /// 自动选一个不冲突且不能重复的宽寄存器（返回起始号 v，占 v 与 v+1）
    int pick_reg4wide(lir::CodeIr *code_ir,
                      int reg_target,
                      bool is_wide_target) {
        SLICER_CHECK(code_ir && code_ir->ir_method && code_ir->ir_method->code);
        const auto code = code_ir->ir_method->code;

        const dex::u2 regs_size = code->registers;
        const dex::u2 ins_size = code->ins_count;
        const int locals = static_cast<int>(regs_size - ins_size);
        if (locals <= 1) {
            LOGE("[pick_reg4wide] locals<=1，无法容纳宽寄存器对");
            return -1;
        }

        auto conflict_with_target_pair = [&](int v) -> bool {
            if (reg_target < 0) return false;
            const int a_lo = v, a_hi = v + 1; // 我们申请的 (v, v+1)
            if (is_wide_target) {
                // 目标也是宽寄存器：避开区间重叠
                const int b_lo = reg_target, b_hi = reg_target + 1;
                return !(a_hi < b_lo || b_hi < a_lo);
            } else {
                // 目标是普通寄存器：v 或 v+1 不能命中 reg_target
                return (reg_target == a_lo) || (reg_target == a_hi);
            }
        };

        for (int v = 0; v + 1 < locals; ++v) {
            // “不能重复”：起点 v 不得等于 reg_target
            if (v == reg_target) continue;

            // 不冲突检查
            if (!conflict_with_target_pair(v)) {
                return v;
            }
        }

        LOGE("[pick_reg4wide] 无可用宽寄存器对 (locals=%d, reg_target=%d, wide_target=%d)",
             locals, reg_target, is_wide_target);
        return -1;
    }


};
