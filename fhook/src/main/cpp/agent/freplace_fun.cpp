//
// Created by Administrator on 2025/8/27.
//
#include "log.h"

#define TAG FEADRE_TAG("freplace_fun")

#include "freplace_fun.h"
#include "dexter/slicer/fsmali_printer.h"
#include "dexter/slicer/fir.h"
#include "dexter/slicer/fjava_api.h"


namespace freplace_fun {

    // instrumentation 有 但是是匿名定义的 直接重定义
    struct BytecodeConvertingVisitor : public lir::Visitor {
        lir::Bytecode *out = nullptr;

        bool Visit(lir::Bytecode *bytecode) {
            out = bytecode;
            return true;
        }
    };

    /**
     * 普通
     * 主要用于index 赋值指令
     * @param code_ir
     * @param insert_point
     * @param reg_det
     * @param value 根据value 的大小 选择最紧凑的 const 指令
     */
    static void EmitConstToReg(lir::CodeIr *code_ir,
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
    static void box_scalar_value(slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point,
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
    static void cre_return_code(lir::CodeIr *code_ir,
                                ir::Type *return_type,
                                int reg_return,
                                slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point
    ) {

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
                auto init = code_ir->Alloc<lir::Bytecode>();
                init->opcode = dex::OP_CONST_4;
                init->operands.push_back(code_ir->Alloc<lir::VReg>(reg_num));
                init->operands.push_back(code_ir->Alloc<lir::Const32>(0)); // null
                code_ir->instructions.push_back(init);

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
                auto init = code_ir->Alloc<lir::Bytecode>();
                // 对于小常量优先使用 const/4，但这里用 const/4 (0) 足够
                init->opcode = dex::OP_CONST_4;
                init->operands.push_back(code_ir->Alloc<lir::VReg>(reg_num));
                init->operands.push_back(code_ir->Alloc<lir::Const32>(0));
                code_ir->instructions.push_back(init);

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
     * const/4 v2 #0x0 创建一个null引用
     * @param code_ir
     * @param reg1
     */
    static void cre_null_reg(lir::CodeIr *code_ir, int reg1,
                             slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        auto const_null1 = code_ir->Alloc<lir::Bytecode>();
        const_null1->opcode = dex::OP_CONST_4;
        const_null1->operands.push_back(code_ir->Alloc<lir::VReg>(reg1)); // v2 = null
        const_null1->operands.push_back(code_ir->Alloc<lir::Const32>(0));
        code_ir->instructions.insert(insert_point, const_null1);
    }

    /**
     * 找 return 指令之前插入
     * @param code_ir
     * @param is_empty_fun
     */
    static void clear_original_instructions(lir::CodeIr *code_ir) {
        auto it = code_ir->instructions.begin();
        while (it != code_ir->instructions.end()) {
            auto curr = *it;
            ++it;
            code_ir->instructions.Remove(curr);
        }
    }


    // 辅助方法2：寻找插入点
    static slicer::IntrusiveList<lir::Instruction>::Iterator
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


    /**
     * 找到最后的 return 指令并返回寄存器号；若 is_wide_out 非空则写入是否为宽类型
        返回值：寄存器号（普通返回或宽类型的 base_reg），失败或 return-void 返回 -1
     * @param code_ir
     * @param is_wide_out
     * @return 返回的是起始寄存器号
     *          [replace_fun4ty] return_register= 2, is_wide_out=1
     */
    static int find_return_register(lir::CodeIr *code_ir, bool *is_wide_out = nullptr) {
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


    static slicer::IntrusiveList<lir::Instruction>::Iterator
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
        LOGI("[find_first_code] 没有找到任何指令")
        return insert_point;
    }

    /** ----------------- 静态函数区 ------------------- */
    static void do_ClassLoader_getSystemClassLoader(lir::Method *f_ClassLoader_getSystemClassLoader,
                                                    lir::CodeIr *code_ir,
                                                    dex::u2 reg_tmp_return,
                                                    slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        auto call_getSystemClassLoader = code_ir->Alloc<lir::Bytecode>();
        call_getSystemClassLoader->opcode = dex::OP_INVOKE_STATIC;
        auto reg_list = code_ir->Alloc<lir::VRegList>();
        reg_list->registers.clear(); // 无参数
        call_getSystemClassLoader->operands.push_back(reg_list);
        call_getSystemClassLoader->operands.push_back(f_ClassLoader_getSystemClassLoader);
        code_ir->instructions.insert(insert_point, call_getSystemClassLoader);


        // 把返回值弄到v0 ActivityThread 类到 v0 move-result-object v0
        auto move_at_cls = code_ir->Alloc<lir::Bytecode>();
        move_at_cls->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_at_cls->operands.push_back(code_ir->Alloc<lir::VReg>(reg_tmp_return));
        code_ir->instructions.insert(insert_point, move_at_cls);
    }

    static void do_Thread_currentThread(lir::Method *f_Thread_currentThread,
                                        lir::CodeIr *code_ir,
                                        dex::u2 reg_tmp_return,
                                        slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        // 获取当前线程到 v0    -----------------
        auto call_currentThread = code_ir->Alloc<lir::Bytecode>();
        call_currentThread->opcode = dex::OP_INVOKE_STATIC;
        auto reg_thread = code_ir->Alloc<lir::VRegList>();
        reg_thread->registers.clear(); // 无参数
        call_currentThread->operands.push_back(reg_thread);
        call_currentThread->operands.push_back(f_Thread_currentThread);
        code_ir->instructions.insert(insert_point, call_currentThread);

        // 保存当前线程到 v0
        auto move_thread = code_ir->Alloc<lir::Bytecode>();
        move_thread->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_thread->operands.push_back(code_ir->Alloc<lir::VReg>(reg_tmp_return));
        code_ir->instructions.insert(insert_point, move_thread);

    }

    /// 执行 Class<?> activityThreadClass = Class.forName("android.app.ActivityThread");
    static void do_Class_forName(lir::Method *f_Class_forName,
                                 lir::CodeIr *code_ir,
                                 dex::u2 reg1, dex::u2 reg_tmp_return,
                                 std::string &name_class,
                                 slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point
    ) {
        ir::Builder builder(code_ir->dex_ir);

        // const-string v0 "xxx"
        auto at_cls_str = builder.GetAsciiString(name_class.c_str());
        auto const_at_cls = code_ir->Alloc<lir::Bytecode>();
        const_at_cls->opcode = dex::OP_CONST_STRING;
        const_at_cls->operands.push_back(code_ir->Alloc<lir::VReg>(reg1)); // v0 = 类名
        const_at_cls->operands.push_back(
                code_ir->Alloc<lir::String>(at_cls_str, at_cls_str->orig_index));
        code_ir->instructions.insert(insert_point, const_at_cls);

        auto call_at_cls = code_ir->Alloc<lir::Bytecode>();
        call_at_cls->opcode = dex::OP_INVOKE_STATIC; // 执行方法

        // 参数 寄存器定义
        auto reg_at_cls = code_ir->Alloc<lir::VRegList>();
        reg_at_cls->registers.clear();
        reg_at_cls->registers.push_back(reg1);

        call_at_cls->operands.push_back(reg_at_cls); // 先参数
        call_at_cls->operands.push_back(f_Class_forName);// 再方法
        code_ir->instructions.insert(insert_point, call_at_cls);

        // 把返回值弄到v0 ActivityThread 类到 v0 move-result-object v0
        auto move_at_cls = code_ir->Alloc<lir::Bytecode>();
        move_at_cls->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_at_cls->operands.push_back(code_ir->Alloc<lir::VReg>(reg_tmp_return));
        code_ir->instructions.insert(insert_point, move_at_cls);

    }


    static void do_THook_onExit(lir::Method *f_THook_onExit,
                                lir::CodeIr *code_ir,
                                int reg1_arg,  // onExit object 的参数寄存器（如 v4）
                                dex::u2 reg2_tmp_long, // long 宽寄存器 用于 method_id
                                int reg_tmp_return, // onExit 返回值 object
                                uint64_t method_id, // uint64_t
                                slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {


        ir::Builder builder(code_ir->dex_ir);
        auto *ir_method = code_ir->ir_method;
        auto *code = ir_method->code;
        const dex::u2 regs_size = code->registers;
        const dex::u2 ins_count = code->ins_count;

        // 校验 范围 + 宽寄存器 + 不重叠
        {
            // 超界检测
            auto in_range = [&](int v) { return v >= 0 && (dex::u2) v < regs_size; };
            if (!in_range(reg1_arg) || !in_range(reg_tmp_return) ||
                !in_range(reg2_tmp_long) || !in_range(reg2_tmp_long + 1)) {
                LOGE("[do_THook_onExit] reg 越界: regs=%u, reg1_arg=%d, ret=%d, long={%u,%u}",
                     regs_size, reg1_arg, reg_tmp_return, reg2_tmp_long, reg2_tmp_long + 1);
                return;
            }
            // 宽越界检测
            if (reg2_tmp_long + 1 >= regs_size) {
                LOGE("[do_THook_onExit] reg2_tmp_long hi 越界: %u/%u", reg2_tmp_long + 1,
                     regs_size);
                return;
            }
            // 禁止 {reg2_tmp_long, reg2_tmp_long+1} 与 reg1_arg/reg_tmp_return 重叠
            if (reg1_arg == (int) reg2_tmp_long || reg1_arg == (int) reg2_tmp_long + 1 ||
                reg_tmp_return == (int) reg2_tmp_long ||
                reg_tmp_return == (int) reg2_tmp_long + 1) {
                LOGE("[do_THook_onExit] 重叠: reg1_arg=%d, ret=%d, long={%u,%u}",
                     reg1_arg, reg_tmp_return, reg2_tmp_long, reg2_tmp_long + 1);
                return;
            }

        }


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
        move_res->operands.push_back(code_ir->Alloc<lir::VReg>(reg_tmp_return));  // 接收返回值到 reg1_arg
        code_ir->instructions.insert(insert_point, move_res);

        {
            // 先清高半 v+1
            EmitConstToReg(code_ir, insert_point, reg2_tmp_long + 1, 0);
            // 再清低半 v
            EmitConstToReg(code_ir, insert_point, reg2_tmp_long, 0);
        }

    }

    ///  LOG.d("Feadre_fjtik", "xxx")
    static void do_Log_d(lir::Method *f_Log_d,
                         lir::CodeIr *code_ir,
                         dex::u2 reg1, dex::u2 reg2,
                         std::string &tag,
                         std::string &text,
                         slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point
    ) {
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
        auto reg_list = code_ir->Alloc<lir::VRegList>();
        reg_list->registers.clear();
        reg_list->registers.push_back(reg1);
        reg_list->registers.push_back(reg2);
        invoke->operands.push_back(reg_list);   // 先参数
        invoke->operands.push_back(f_Log_d); // 再方法
        code_ir->instructions.insert(insert_point, invoke);
        // 没有处理 返回值 ****************
    }


    static void do_THook_onEnter(lir::Method *f_THook_onEnter,
                                 lir::CodeIr *code_ir,
                                 dex::u2 reg1_arg, // Object[]
                                 dex::u2 reg2_tmp_long, // long 必须宽寄存器
                                 dex::u2 reg_tmp_return,
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
            if (!in_range(reg1_arg) || !in_range(reg_tmp_return) ||
                !in_range(reg2_tmp_long) || !in_range(reg2_tmp_long + 1)) {
                LOGE("[do_THook_onExit] reg 越界: regs=%u, reg1_arg=%d, ret=%d, long={%u,%u}",
                     regs_size, reg1_arg, reg_tmp_return, reg2_tmp_long, reg2_tmp_long + 1);
                return;
            }
            // 宽越界检测
            if (reg2_tmp_long + 1 >= regs_size) {
                LOGE("[do_THook_onExit] reg2_tmp_long hi 越界: %u/%u", reg2_tmp_long + 1,
                     regs_size);
                return;
            }
            // 禁止 {reg2_tmp_long, reg2_tmp_long+1} 与 reg1_arg/reg_tmp_return 重叠
            if (reg1_arg == (int) reg2_tmp_long || reg1_arg == (int) reg2_tmp_long + 1 ||
                reg_tmp_return == (int) reg2_tmp_long ||
                reg_tmp_return == (int) reg2_tmp_long + 1) {
                LOGE("[do_THook_onExit] 重叠: reg1_arg=%d, ret=%d, long={%u,%u}",
                     reg1_arg, reg_tmp_return, reg2_tmp_long, reg2_tmp_long + 1);
                return;
            }

        }

        // 1) const-wide v<reg_id_lo>, 123456L
        auto cst = code_ir->Alloc<lir::Bytecode>();
        cst->opcode = dex::OP_CONST_WIDE; // 写入 v<reg_id_lo> 与 v<reg_id_lo+1>
        cst->operands.push_back(code_ir->Alloc<lir::VRegPair>(reg2_tmp_long));
        cst->operands.push_back(code_ir->Alloc<lir::Const64>(static_cast<uint64_t>(method_id)));
        code_ir->instructions.insert(insert_point, cst);

        // 构建 invoke-static（void，无参数）
        auto invoke = code_ir->Alloc<lir::Bytecode>();
        invoke->opcode = dex::OP_INVOKE_STATIC;

        auto regs = code_ir->Alloc<lir::VRegList>();
        regs->registers.clear();
        regs->registers.push_back(reg1_arg);   // 传入 Object[] 实参
        regs->registers.push_back(reg2_tmp_long);
        regs->registers.push_back(reg2_tmp_long + 1);

        invoke->operands.push_back(regs);                // 先参数列表
        invoke->operands.push_back(f_THook_onEnter);     // 再方法引用
        code_ir->instructions.insert(insert_point, invoke);

        // move-result-object v<reg1_arg>
        auto mres = code_ir->Alloc<lir::Bytecode>();
        mres->opcode = dex::OP_MOVE_RESULT_OBJECT;
        mres->operands.push_back(code_ir->Alloc<lir::VReg>(reg_tmp_return));
        code_ir->instructions.insert(insert_point, mres);

        {
            // 先清高半 v+1
            EmitConstToReg(code_ir, insert_point, reg2_tmp_long + 1, 0);
            // 再清低半 v
            EmitConstToReg(code_ir, insert_point, reg2_tmp_long, 0);
        }

    }

    static void do_HookBridge_replace_fun(lir::Method *f_HookBridge_replace_fun,
                                          lir::CodeIr *code_ir,
                                          slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point
    ) {

        ir::Builder builder(code_ir->dex_ir);

        // 构建 invoke-static（void，无参数）
        auto invoke = code_ir->Alloc<lir::Bytecode>();
        invoke->opcode = dex::OP_INVOKE_STATIC;

        auto reg_list = code_ir->Alloc<lir::VRegList>();
        reg_list->registers.clear();// 零参数

        invoke->operands.push_back(reg_list);   // 先参数
        invoke->operands.push_back(f_HookBridge_replace_fun); // 再方法
        code_ir->instructions.insert(insert_point, invoke);

    }


    /// Class.forName("xxxxx", true, ClassLoader.getSystemClassLoader)
    static void do_Class_forName4loader(lir::Method *f_Class_forName4loader,
                                        lir::CodeIr *code_ir,
                                        dex::u2 reg1, dex::u2 reg2, dex::u2 reg3,
                                        dex::u2 reg_return,
                                        std::string &name_class,
                                        slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);

        // const-string v0 "xxx"
        auto at_cls_str = builder.GetAsciiString(name_class.c_str());
        auto const_at_cls = code_ir->Alloc<lir::Bytecode>();
        const_at_cls->opcode = dex::OP_CONST_STRING;
        const_at_cls->operands.push_back(code_ir->Alloc<lir::VReg>(reg1)); // v0 = 类名
        const_at_cls->operands.push_back(
                code_ir->Alloc<lir::String>(at_cls_str, at_cls_str->orig_index));
        code_ir->instructions.insert(insert_point, const_at_cls);


        // 准备 Class.forName 的参数：initialize = true (v5)
        EmitConstToReg(code_ir, insert_point, reg2, 1);

        auto call_forName3 = code_ir->Alloc<lir::Bytecode>();
        call_forName3->opcode = dex::OP_INVOKE_STATIC;
        auto reg_forName3 = code_ir->Alloc<lir::VRegList>();
        reg_forName3->registers.clear();// v0=类名, v5=true, v4=应用类加载器
        reg_forName3->registers.push_back(reg1);
        reg_forName3->registers.push_back(reg2);
        reg_forName3->registers.push_back(reg3);
        call_forName3->operands.push_back(reg_forName3); // 先参数
        call_forName3->operands.push_back(f_Class_forName4loader);// 再方法
        code_ir->instructions.insert(insert_point, call_forName3);

        // 保存 HookBridge 类到 v0
        auto move_hb_cls = code_ir->Alloc<lir::Bytecode>();
        move_hb_cls->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_hb_cls->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));
        code_ir->instructions.insert(insert_point, move_hb_cls);

    }

    static void do_ClassLoader_loadClass(lir::Method *f_classloader_loadClass,
                                         lir::CodeIr *code_ir,
                                         dex::u2 reg1, dex::u2 reg2,
                                         dex::u2 reg_return,
                                         std::string &name_class,
                                         slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);

        // const-string v0 "xxx"
        auto class_name_str = builder.GetAsciiString(name_class.c_str());
        auto const_class_name = code_ir->Alloc<lir::Bytecode>();
        const_class_name->opcode = dex::OP_CONST_STRING;
        const_class_name->operands.push_back(code_ir->Alloc<lir::VReg>(reg2));
        const_class_name->operands.push_back(
                code_ir->Alloc<lir::String>(class_name_str, class_name_str->orig_index));
        code_ir->instructions.insert(insert_point, const_class_name);

        // 调用 loadClass 方法加载类到 v4
        auto call_loadClass = code_ir->Alloc<lir::Bytecode>();
        call_loadClass->opcode = dex::OP_INVOKE_VIRTUAL;
        auto load_class_regs = code_ir->Alloc<lir::VRegList>();
        load_class_regs->registers.clear();
        load_class_regs->registers.push_back(reg1);
        load_class_regs->registers.push_back(reg2);

        call_loadClass->operands.push_back(load_class_regs);
        call_loadClass->operands.push_back(f_classloader_loadClass);
        code_ir->instructions.insert(insert_point, call_loadClass);

        // 保存加载的类到 v4
        auto move_loaded_class = code_ir->Alloc<lir::Bytecode>();
        move_loaded_class->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_loaded_class->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));
        code_ir->instructions.insert(insert_point, move_loaded_class);

    }

    /** ----------------- 对象函数区 ------------------- */

    static void do_classloader_loadClass(lir::Method *f_classloader_loadClass,
                                         lir::CodeIr *code_ir,
                                         dex::u2 reg1, dex::u2 reg2,
                                         dex::u2 reg_return,
                                         std::string name_class,
                                         slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {
        ir::Builder builder(code_ir->dex_ir);

        // 准备类名字符串到 reg2
        auto class_name_str = builder.GetAsciiString(name_class.c_str());
        auto const_class_name = code_ir->Alloc<lir::Bytecode>();
        const_class_name->opcode = dex::OP_CONST_STRING;
        const_class_name->operands.push_back(code_ir->Alloc<lir::VReg>(reg2));
        const_class_name->operands.push_back(
                code_ir->Alloc<lir::String>(class_name_str, class_name_str->orig_index));
        code_ir->instructions.insert(insert_point, const_class_name);


        // 调用 loadClass 方法（使用应用类加载器 reg1 和类名 reg2）
        auto call_loadClass = code_ir->Alloc<lir::Bytecode>();
        call_loadClass->opcode = dex::OP_INVOKE_VIRTUAL;
        auto load_class_regs = code_ir->Alloc<lir::VRegList>();
        load_class_regs->registers.clear();
        load_class_regs->registers.push_back(reg1);
        load_class_regs->registers.push_back(reg2);
        call_loadClass->operands.push_back(load_class_regs);
        call_loadClass->operands.push_back(f_classloader_loadClass);
        code_ir->instructions.insert(insert_point, call_loadClass);


        // 保存加载的类到 v0
        auto move_loaded_class = code_ir->Alloc<lir::Bytecode>();
        move_loaded_class->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_loaded_class->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));
        code_ir->instructions.insert(insert_point, move_loaded_class);

    }

    static void do_thread_getContextClassLoader(lir::Method *f_thread_getContextClassLoader,
                                                lir::CodeIr *code_ir,
                                                dex::u2 reg1,
                                                dex::u2 reg_return,
                                                slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        auto call_getContextClassLoader = code_ir->Alloc<lir::Bytecode>();
        call_getContextClassLoader->opcode = dex::OP_INVOKE_VIRTUAL;
        auto reg_loader = code_ir->Alloc<lir::VRegList>();
        reg_loader->registers.clear();
        reg_loader->registers.push_back(reg1);
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
     * public Method getDeclaredMethod(String name, Class<?>... parameterTypes)
     * Method m = clazz.getDeclaredMethod("onEnter", new Class[]{Object[].class});
     * ------ 这个目前只能找到无参数的方法
     * @param f_class_getDeclaredMethod
     * @param code_ir
     * @param builder
     * @param reg1 需要一个class对象
     * @param reg2 装 name_fun 的寄存器
     * @param reg3 装 parameterTypes 的寄存器
     * @param reg_return
     * @param name_fun
     */
    static void do_class_getDeclaredMethod(lir::Method *f_class_getDeclaredMethod,
                                           lir::CodeIr *code_ir,
                                           dex::u2 reg1, // class对象
                                           dex::u2 reg2, // 方法名
                                           dex::u2 reg3, // 方法参数
                                           dex::u2 reg_return,
                                           std::string &name_fun,
                                           slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);

        // reg2 放入方法名
        auto const_currentApp = builder.GetAsciiString(name_fun.c_str());
        auto const_ca_str = code_ir->Alloc<lir::Bytecode>();
        const_ca_str->opcode = dex::OP_CONST_STRING;
        const_ca_str->operands.push_back(code_ir->Alloc<lir::VReg>(reg2));
        const_ca_str->operands.push_back(code_ir->Alloc<lir::String>(
                const_currentApp, const_currentApp->orig_index));
        code_ir->instructions.insert(insert_point, const_ca_str);


        // 调用 getDeclaredMethod("currentApplication", null)
        auto call_getMethod_ca = code_ir->Alloc<lir::Bytecode>();
        call_getMethod_ca->opcode = dex::OP_INVOKE_VIRTUAL;
        auto reg_getMethod_ca = code_ir->Alloc<lir::VRegList>();
//        reg_getMethod_ca->registers = {0, 1, 2}; // v0=ActivityThread类, v1=方法名, v2=null
        reg_getMethod_ca->registers.clear();
        reg_getMethod_ca->registers.push_back(reg1);
        reg_getMethod_ca->registers.push_back(reg2);
        reg_getMethod_ca->registers.push_back(reg3);
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
     * 动态增加寄存器数量 新增寄存器在前面
         原配置 registers=3，ins_count=2 → 新增后 registers=5，ins_count=2
         v0（本地寄存器）、v1（第 1 个参数）、v2（第 2 个参数，参数在末尾）
         v0、v1（新增的 2 个本地寄存器）、v2（本地寄存器）、v3（第 1 个参数）、v4（第 2 个参数，仍在末尾）
     * @param code_ir
     * @param regs_count
     * @return
     */
    static dex::u2 req_reg(lir::CodeIr *code_ir, dex::u2 regs_count) {
        LOGD("[req_reg] start...")
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
     * public static Application currentApplication()
     * Object application = currentAppMethod.invoke(null); // 静态方法调用
     * m.invoke(null, new Object[]{ args });
     * 方法是静态的，但反射调用按照反射的调用方式来
     * 不管用原来是什么方法，用反射都是 OP_INVOKE_VIRTUAL
     * @param f_Log_d
     * @param code_ir
     * @param ir_method
     * @param builder
     * @param reg1
     * @param reg2
     * @param reg3
     */
    static void do_method_invoke(lir::Method *f_method_invoke,
                                 lir::CodeIr *code_ir,
                                 dex::u2 reg1, dex::u2 reg2, dex::u2 reg3, dex::u2 reg_return,
                                 slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        // 调用 currentApplication() 方法
        auto call_currentApp = code_ir->Alloc<lir::Bytecode>();
        call_currentApp->opcode = dex::OP_INVOKE_VIRTUAL;
        auto reg_currentApp = code_ir->Alloc<lir::VRegList>();
        // v1=方法对象, v2=null(实例), v2=null(参数)
        reg_currentApp->registers.clear();
        reg_currentApp->registers.push_back(reg1);
        reg_currentApp->registers.push_back(reg2);
        reg_currentApp->registers.push_back(reg3);

        call_currentApp->operands.push_back(reg_currentApp);
        call_currentApp->operands.push_back(f_method_invoke);
        code_ir->instructions.insert(insert_point, call_currentApp);

        // 保存 Application 实例（Context）到 v3
        auto move_app = code_ir->Alloc<lir::Bytecode>();
        move_app->opcode = dex::OP_MOVE_RESULT_OBJECT;
        move_app->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return)); // v3 = Context
        code_ir->instructions.insert(insert_point, move_app);

    }


    /// 支持 各种参数反射执行
    static bool do_apploader_static_fun(lir::CodeIr *code_ir,
                                        dex::u2 reg1, dex::u2 reg2, dex::u2 reg3,
                                        int reg_method_arg, // 可以为null 用于存储方法参数类型数组（Class[]）
                                        int reg_do_args,// 可以为null 原始参数数组（Object[]）
                                        dex::u2 reg_return,
                                        std::string &name_class,
                                        std::string &name_fun,
                                        slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point
    ) {

        /// ActivityThread 方案 不行 ----------------
        // Thread.currentThread() → 返回线程对象 v0
        lir::Method *f_Thread_currentThread = fjava_api::get_Thread_currentThread(code_ir);
        if (!f_Thread_currentThread) {
            LOGE("[do_apploader_static_fun] f_Thread_currentThread error");
            return false;
        }
        do_Thread_currentThread(f_Thread_currentThread, code_ir, reg1, insert_point);

        // thread.getContextClassLoader() classloader 对象 到v1
        lir::Method *f_thread_getContextClassLoader = fjava_api::get_thread_getContextClassLoader(
                code_ir);
        if (!f_thread_getContextClassLoader) {
            LOGE("[do_apploader_static_fun] f_thread_getContextClassLoader error");
            return false;
        }
        do_thread_getContextClassLoader(f_thread_getContextClassLoader, code_ir,
                                        reg1, reg1, insert_point);

        // 执行拿到 HookBridge class 对象 到v0
        lir::Method *f_classloader_loadClass = fjava_api::get_classloader_loadClass(
                code_ir);
        if (!f_classloader_loadClass) {
            LOGE("[do_apploader_static_fun] f_classloader_loadClass error");
            return false;
        }
        do_classloader_loadClass(f_classloader_loadClass, code_ir,
                                 reg1, reg2, reg1, name_class, insert_point);


        /// Method m = clazz.getDeclaredMethod("onEnter", new Class[]{Object[].class});
        lir::Method *f_class_getDeclaredMethod = fjava_api::get_class_getDeclaredMethod(
                code_ir);
        if (!f_class_getDeclaredMethod) {
            LOGE("[do_apploader_static_fun] f_class_getDeclaredMethod error");
            return false;
        }

        if (reg_method_arg < 0) {
            cre_null_reg(code_ir, reg3, insert_point);
            reg_method_arg = reg3;
            LOGI("[do_apploader_static_fun] reg_method_arg= %d ", reg_method_arg)
        }

        // 拿到方法对象 v1   reg1= class对象  reg2 = 方法名  reg3= 方法参数 class[]
        do_class_getDeclaredMethod(f_class_getDeclaredMethod, code_ir,
                                   reg1, reg2, reg_method_arg,
                                   reg1, name_fun, insert_point);

        /// Object ret = m.invoke(null, new Object[]{ args });
        lir::Method *f_method_invoke = fjava_api::get_method_invoke(code_ir);
        if (!f_method_invoke) {
            LOGE("[do_apploader_static_fun] f_method_invoke error");
            return false;
        }

        if (reg_do_args < 0) {
            cre_null_reg(code_ir, reg3, insert_point);
        } else {
            reg3 = reg_do_args;
        }

        cre_null_reg(code_ir, reg2, insert_point);  // 创建空引用 固定为静态方法 ****************** 前面可以用

//         reg1 是方法对象, reg2 静态是是 null, reg3 包装后的参数数组
        do_method_invoke(f_method_invoke, code_ir, reg1, reg2,
                         reg3, reg_return, insert_point);


        return true;
    }

    /**
     * 就是把 Object[] 数组还原成参数
     * @param code_ir
     * @param insert_point
     * @param reg_arr
     * @param base_param_reg
     * @param reg_tmp_obj
     * @param reg_tmp_idx
     */
    static void restore_reg_params4type(lir::CodeIr *code_ir,
                                        dex::u4 reg_arr,            // enter 返回的 Object[] 寄存器 (e.g. v0)
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
        check_cast_arr->operands.push_back(code_ir->Alloc<lir::VReg>(reg_arr));
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
        dex::u4 index_arr = 0;    // 从 reg_arr[index_arr] 读取

        // 如果是实例方法，按你打包约定，index_arr=0 对应 this
        if (!is_static) {
            // 把 index 0 的从数组拿出来
            EmitConstToReg(code_ir, insert_point, reg_tmp_idx, (dex::s4) index_arr);

            // aget-object reg_tmp_obj, reg_arr, reg_tmp_idx
            {
                auto aget = code_ir->Alloc<lir::Bytecode>();
                aget->opcode = dex::OP_AGET_OBJECT;
                aget->operands.push_back(code_ir->Alloc<lir::VReg>(reg_tmp_obj));
                aget->operands.push_back(code_ir->Alloc<lir::VReg>(reg_arr));
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

            // 把值取出来先 aget-object reg_tmp_obj, reg_arr, reg_tmp_idx
            {
                auto aget = code_ir->Alloc<lir::Bytecode>();
                aget->opcode = dex::OP_AGET_OBJECT;
                aget->operands.push_back(code_ir->Alloc<lir::VReg>(reg_tmp_obj));
                aget->operands.push_back(code_ir->Alloc<lir::VReg>(reg_arr));
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
     * 生成 onEnter 的参数类型数组（Class[]{Object[].class}） 只有一个参数
     * @param code_ir
     * @param builder
     * @param reg1
     * @param reg2
     * @param reg3_arr
     * @param insert_point
        [6] const/4 v0 #0x1
        [7] new-array v3 v0 [Ljava/lang/Class;
        [8] const-class v1 [Ljava/lang/Object;
        [9] const/4 v0 #0x0
        [10] aput-object v1 v3 v0
     */
    static void cre_arr_class_args4onEnter(
            lir::CodeIr *code_ir,
            dex::u2 reg1, // array_size 也是索引
            dex::u2 reg2, // value
            dex::u2 reg3_arr, // array 这个 object[] 对象
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);

        // const reg1 #0x2 → 数组长度2  长度=2：  Class[]{ Object[].class, long.class }
        EmitConstToReg(code_ir, insert_point, reg1, 2);

        // 填 index 0: Object[].class new-array reg3_arr reg1      reg3_arr 是  Object[].class（Class 类型）
        {
            const auto class_base_type = builder.GetType("[Ljava/lang/Class;");

            auto new_arr = code_ir->Alloc<lir::Bytecode>();
            new_arr->opcode = dex::OP_NEW_ARRAY;
            new_arr->operands.push_back(
                    code_ir->Alloc<lir::VReg>(reg3_arr)); // reg3_arr = Class[] 数组
            new_arr->operands.push_back(code_ir->Alloc<lir::VReg>(reg1)); // 长度=reg1（2）
            new_arr->operands.push_back(
                    code_ir->Alloc<lir::Type>(class_base_type, class_base_type->orig_index));
            code_ir->instructions.insert(insert_point, new_arr);

            /// 假设 obj_array_type = builder.GetType("[Ljava/lang/Object;")
            const auto obj_array_type = builder.GetType("[Ljava/lang/Object;");

            // 生成 const-class reg2, [Ljava/lang/Object;
            auto const_class_op = code_ir->Alloc<lir::Bytecode>();
            const_class_op->opcode = dex::OP_CONST_CLASS; // 注意：使用 CONST_CLASS 指令
            const_class_op->operands.push_back(
                    code_ir->Alloc<lir::VReg>(reg2)); // reg2 <- Class object
            const_class_op->operands.push_back(
                    code_ir->Alloc<lir::Type>(obj_array_type, obj_array_type->orig_index));
            code_ir->instructions.insert(insert_point, const_class_op);

            // 然后再做 index const 和 aput-object (把 reg2 存到 Class[] 数组)
            EmitConstToReg(code_ir, insert_point, reg1, 0);

            auto aput_class = code_ir->Alloc<lir::Bytecode>();
            aput_class->opcode = dex::OP_APUT_OBJECT;
            aput_class->operands.push_back(
                    code_ir->Alloc<lir::VReg>(reg2)); // value: Object[].class
            aput_class->operands.push_back(code_ir->Alloc<lir::VReg>(reg3_arr)); // Class[] array
            aput_class->operands.push_back(code_ir->Alloc<lir::VReg>(reg1)); // index 0
            code_ir->instructions.insert(insert_point, aput_class);
        }

        // idx 1 = long.class (Long.TYPE) Ljava/lang/Long;->TYPE:Ljava/lang/Class;
        {
            const auto java_lang_Long = builder.GetType("Ljava/lang/Long;");
            const auto java_lang_Class = builder.GetType("Ljava/lang/Class;");
            auto name_TYPE = builder.GetAsciiString("TYPE");
            auto field_TYPE_decl = builder.GetFieldDecl(name_TYPE, java_lang_Class, java_lang_Long);

            // sget-object v<reg2>, Ljava/lang/Long;->TYPE:Ljava/lang/Class;
            auto sget = code_ir->Alloc<lir::Bytecode>();
            sget->opcode = dex::OP_SGET_OBJECT;
            sget->operands.push_back(code_ir->Alloc<lir::VReg>(reg2));
            sget->operands.push_back(code_ir->Alloc<lir::Field>(
                    field_TYPE_decl, field_TYPE_decl->orig_index));
            code_ir->instructions.insert(insert_point, sget);

            EmitConstToReg(code_ir, insert_point, reg1, 1);

            auto aput1 = code_ir->Alloc<lir::Bytecode>();
            aput1->opcode = dex::OP_APUT_OBJECT;
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg2)); // Long.TYPE
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg3_arr)); // Class[]
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg1)); // 1
            code_ir->instructions.insert(insert_point, aput1);
        }
    }

    static void cre_arr_class_args4onExit(
            lir::CodeIr *code_ir,
            dex::u2 reg1, // index 与 array_size 复用
            dex::u2 reg2, // value 临时（存放 Class 对象）
            dex::u2 reg3_arr, // array 目标寄存器（Class[]）
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        ir::Builder builder(code_ir->dex_ir);

        // Class[] 长度=2： { Object.class, long.class }
        EmitConstToReg(code_ir, insert_point, reg1, 2);

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
            const auto obj_type = builder.GetType("Ljava/lang/Object;");

            // const-class v<reg2>, Ljava/lang/Object;
            auto const_class_obj = code_ir->Alloc<lir::Bytecode>();
            const_class_obj->opcode = dex::OP_CONST_CLASS;
            const_class_obj->operands.push_back(code_ir->Alloc<lir::VReg>(reg2));
            const_class_obj->operands.push_back(
                    code_ir->Alloc<lir::Type>(obj_type, obj_type->orig_index));
            code_ir->instructions.insert(insert_point, const_class_obj);

            // index = 0
            EmitConstToReg(code_ir, insert_point, reg1, 0);

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
            const auto java_lang_Long = builder.GetType("Ljava/lang/Long;");
            const auto java_lang_Class = builder.GetType("Ljava/lang/Class;");
            auto name_TYPE = builder.GetAsciiString("TYPE");
            auto field_TYPE_decl = builder.GetFieldDecl(name_TYPE, java_lang_Class, java_lang_Long);

            // sget-object v<reg2>, Ljava/lang/Long;->TYPE:Ljava/lang/Class;
            auto sget = code_ir->Alloc<lir::Bytecode>();
            sget->opcode = dex::OP_SGET_OBJECT;
            sget->operands.push_back(code_ir->Alloc<lir::VReg>(reg2));
            sget->operands.push_back(
                    code_ir->Alloc<lir::Field>(field_TYPE_decl, field_TYPE_decl->orig_index));
            code_ir->instructions.insert(insert_point, sget);

            // index = 1
            EmitConstToReg(code_ir, insert_point, reg1, 1);

            // aput-object v<reg2> → v<reg3_arr>[v<reg1>]
            auto aput1 = code_ir->Alloc<lir::Bytecode>();
            aput1->opcode = dex::OP_APUT_OBJECT;
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg2)); // Long.TYPE (long.class)
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg3_arr)); // Class[]
            aput1->operands.push_back(code_ir->Alloc<lir::VReg>(reg1)); // index 1
            code_ir->instructions.insert(insert_point, aput1);
        }
    }


    /// 生成 object 参数
    // 包装返回值到 Object（放到 reg1_tmp_arg），便于后续注入 onExit(Object)
    static void cre_arr_do_args4onExit(
            lir::CodeIr *code_ir,
            int reg_return,         // 原方法返回值所在寄存器 清空则没有返回值
            dex::u2 reg1_tmp_arg,       // 返回结果 目标寄存器（一定要是 object 类型寄存器）
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        SLICER_CHECK(code_ir != nullptr);
        auto ir_method = code_ir->ir_method;
        SLICER_CHECK(ir_method != nullptr);
        auto proto = ir_method->decl->prototype;
        SLICER_CHECK(proto != nullptr);
        auto return_type = proto->return_type;
        SLICER_CHECK(return_type != nullptr);

        // 无返回值或方法已经清空情况
        if (strcmp(return_type->descriptor->c_str(), "V") == 0 || reg_return < 0) {
            // 拿到方法信息
            LOGI("[cre_arr_do_args4onExit] %s.%s%s -> void/null, 插入null到v%d",
                 ir_method->decl->parent->Decl().c_str(),
                 ir_method->decl->name->c_str(),
                 ir_method->decl->prototype->Signature().c_str(),
                 reg1_tmp_arg);
            // 创建空对象
            cre_null_reg(code_ir, reg1_tmp_arg, insert_point);
            return;
        }

        if (return_type->GetCategory() == ir::Type::Category::Reference) {
            // === 引用直接赋值 ，向上不用转换 ===
            auto move_obj = code_ir->Alloc<lir::Bytecode>();
            move_obj->opcode = dex::OP_MOVE_OBJECT;
            move_obj->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp_arg));
            move_obj->operands.push_back(code_ir->Alloc<lir::VReg>(reg_return));
            code_ir->instructions.insert(insert_point, move_obj);
            return;
        }

        // === 基本标量类型：装箱成对象 ===
        box_scalar_value(insert_point, code_ir, return_type,
                         reg_return, reg1_tmp_arg);


        LOGD("[cre_arr_do_args4onExit] 处理完成：%s → 转换为Object到v%d",
             return_type->descriptor->c_str(), reg1_tmp_arg);
    }

    /// 把方法的参数强转object 放在 object[] 数组中
    static void cre_arr_object0(
            lir::CodeIr *code_ir,
            dex::u2 reg1_tmp_idx, // array_size 也是索引
            dex::u2 reg2_value, // value
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

        // 这个是参数寄存器初始
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
                box_scalar_value(insert_point, code_ir, type, reg_arg, reg2_value);
                src_reg = reg2_value; // 用临时寄存器
                // 宽标量类型（long、double）→ 多加1个寄存器
                reg_arg += 1 + (type->GetCategory() == ir::Type::Category::WideScalar);
            } else {
                // 引用类型（如 String、Object、数组）→ 直接用原参数寄存器
                src_reg = reg_arg;
                reg_arg++;
            }

            //  定义数组索引 const v0 #0x1
            EmitConstToReg(code_ir, insert_point, reg1_tmp_idx, i);
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

    /// 用 Object[] 把指定的参数 放入
    static void cre_arr_object1(
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
            EmitConstToReg(code_ir, insert_point, reg1_tmp_idx, 2);

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
            EmitConstToReg(code_ir, insert_point, reg1_tmp_idx, 0);

            // 先把原参数放进去
            auto aput_outer = code_ir->Alloc<lir::Bytecode>();
            aput_outer->opcode = dex::OP_APUT_OBJECT;
            aput_outer->operands.push_back(code_ir->Alloc<lir::VReg>(reg2_value));  // 原始参数数组
            aput_outer->operands.push_back(code_ir->Alloc<lir::VReg>(reg3_arr));         // 外层数组
            aput_outer->operands.push_back(code_ir->Alloc<lir::VReg>(reg1_tmp_idx));         // 索引0
            code_ir->instructions.insert(insert_point, aput_outer);
        }

        // outer[1] = Long.valueOf(String.valueOf(method_id))
        // 这里走 String 免使用宽寄存器：Long.valueOf(Ljava/lang/String;)Ljava/lang/Long;
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
            EmitConstToReg(code_ir, insert_point, reg2_value, 1);

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
     * @param reg1
     * @param reg2
     * @param reg3
     * @param insert_point
        [0] const v0 #0x2
        [1] new-array v2 v0 [Ljava/lang/Object;
        [2] invoke-static/range {v4 .. v4} Ljava/lang/Integer;->valueOf(I)Ljava/lang/Integer;
        [3] move-result-object v1
        [4] const v0 #0x1
        [5] aput-object v1 v2 v0
     */
    static void cre_arr_do_args4onEnter(
            lir::CodeIr *code_ir,
            dex::u2 reg1, // array_size 也是索引
            dex::u2 reg2, // value  array 这个 object[object[]] 再包一层对象
            dex::u2 reg3, // array 这个 object[] 对象
            uint64_t method_id, // 要写入的 methodId
            slicer::IntrusiveList<lir::Instruction>::Iterator &insert_point) {

        // 创建 object[] 对象 -》 reg3
        cre_arr_object0(code_ir, reg1, reg3, reg2, insert_point);

        // object[object[]] 再包一层对象
        cre_arr_object1(code_ir, reg1, reg2, reg3, method_id, insert_point);

    }

    /**
     * 仅支持 void 返回方法 打印LOG直接返回 无需额外方法
     * @param code_ir
     * @return 替换完成后的指令  不支持中文
        [0] const-string v0 "Feadre_fjtik"
        [1] const-string v1 "[replace_fun4return] C++ Ljava/lang/System; exit (I)V ------"
        [2] invoke-static Landroid/util/Log;->d(Ljava/lang/String;Ljava/lang/String;)I
        [3] return-void
     *
     */
    bool replace_fun4return(slicer::DetourHook *thiz, lir::CodeIr *code_ir) {
        LOGD("[replace_fun4return] start ....")

        const auto ir_method = code_ir->ir_method;
        if (!ir_method || !ir_method->code) {
            LOGE("[replace_fun4return] no ir_method %s %s %s",
                 thiz->orig_method_id_.class_descriptor,
                 thiz->orig_method_id_.method_name,
                 thiz->orig_method_id_.signature);
            return false;
        }

        auto orig_return_type = ir_method->decl->prototype->return_type;
        ir::Builder builder(code_ir->dex_ir);

        int return_register = -1;
        bool is_clear = false;
        if (is_clear) {
            clear_original_instructions(code_ir);
        } else {
            bool is_wide_out = false;
            return_register = find_return_register(code_ir, &is_wide_out);
            LOGI("[replace_fun4return] return_register= %d, is_wide_out=%d", return_register,
                 is_wide_out);
        };
        auto insert_point = find_first_code(code_ir);

        // ******************************************************************

        dex::u2 num_add_reg = req_reg(code_ir, 2); // 需要两个寄存器
        auto num_reg_non_param = ir_method->code->registers - ir_method->code->ins_count;

        int v0 = 0;
        int v1 = 1;

        lir::Method *f_Log_d = fjava_api::get_log_method(code_ir);
        if (!f_Log_d) {
            LOGE("[replace_fun4return] get_log_method error");
            return false;
        }

        std::string _text = "[replace_fun4return] C++ ";
        _text += thiz->orig_method_id_.class_descriptor;
        _text += " ";
        _text += thiz->orig_method_id_.method_name;
        _text += " ";
        _text += thiz->orig_method_id_.signature;
        _text += " ------";
        std::string tag = "Feadre_fjtik";
        do_Log_d(f_Log_d, code_ir, v0, v1,
                 tag, _text, insert_point);

        // ******************************************************************

        if (num_add_reg > 0) {
            //扩展了几个寄存器 = 需要多少本地寄存器 - 原有多少个本地寄存器
            restore_reg_params4shift(code_ir, insert_point, num_add_reg);
        }
        if (is_clear) {
            cre_return_code(code_ir, orig_return_type, -1, insert_point);
        }

        LOGD("[replace_return] 完成 %s %s %s",
             thiz->orig_method_id_.class_descriptor,
             thiz->orig_method_id_.method_name,
             thiz->orig_method_id_.signature);
        return true;
    }

    /**
     * 仅支持 void  挂接无参数方法
     * @param thiz
     * @param code_ir
     * @return
[0] const-string v0 "Feadre_fjtik"
[1] const-string v1 "[replace_fun4return] C++ Ljava/lang/System; exit (I)V ------"
[2] invoke-static Landroid/util/Log;->d(Ljava/lang/String;Ljava/lang/String;)I
[3] const-string v0 "android.app.ActivityThread"
[4] invoke-static Ljava/lang/Class;->forName(Ljava/lang/String;)Ljava/lang/Class;
[5] move-result-object v0
[6] const/4 v2 #0x0
[7] const-string v1 "currentApplication"
[8] invoke-virtual Ljava/lang/Class;->getDeclaredMethod(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;
[9] move-result-object v1
[10] invoke-virtual Ljava/lang/reflect/Method;->invoke(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;
[11] move-result-object v3
[12] const-string v0 "android.content.Context"
[13] invoke-static Ljava/lang/Class;->forName(Ljava/lang/String;)Ljava/lang/Class;
[14] move-result-object v0
[15] const-string v1 "getClassLoader"
[16] invoke-virtual Ljava/lang/Class;->getDeclaredMethod(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;
[17] move-result-object v1
[18] invoke-virtual Ljava/lang/reflect/Method;->invoke(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;
[19] move-result-object v4
[20] const-string v0 "com.zxc.jtik.HookBridge"
[21] const/4 v5 #0x1
[22] invoke-static Ljava/lang/Class;->forName(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;
[23] move-result-object v0
[24] const-string v1 "replace_fun"
[25] invoke-virtual Ljava/lang/Class;->getDeclaredMethod(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;
[26] move-result-object v1
[27] invoke-virtual Ljava/lang/reflect/Method;->invoke(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;
[28] move-result-object v0
[29] return-void
     */
    bool replace_fun4hook_void(slicer::DetourHook *thiz, lir::CodeIr *code_ir) {
        LOGD("[replace_fun4hook_void] start ....")

        const auto ir_method = code_ir->ir_method;
        if (!ir_method || !ir_method->code) {
            LOGE("[replace_fun4hook_void] no ir_method %s %s %s",
                 thiz->orig_method_id_.class_descriptor,
                 thiz->orig_method_id_.method_name,
                 thiz->orig_method_id_.signature);
            return false;
        }

        auto orig_return_type = ir_method->decl->prototype->return_type;
        ir::Builder builder(code_ir->dex_ir);

        int return_register = -1;
        bool is_clear = false;
        if (is_clear) {
            clear_original_instructions(code_ir);
        } else {
            bool is_wide_out = false;
            return_register = find_return_register(code_ir, &is_wide_out);
            LOGI("[replace_fun4onExit] return_register= %d, is_wide_out=%d", return_register,
                 is_wide_out);
        };
        auto insert_point = find_first_code(code_ir);

        // ******************************************************************

        //返回运行hook方法需要申请3个寄存器
        dex::u2 num_add_reg = req_reg(code_ir, 3);
        auto num_reg_non_param = ir_method->code->registers - ir_method->code->ins_count;

        dex::u2 v0 = 0;
        dex::u2 v1 = 1;
        dex::u2 v2 = 2;

        lir::Method *f_Log_d = fjava_api::get_log_method(code_ir);
        if (!f_Log_d) {
            LOGE("[replace_fun4hook_void] get_log_method error");
            return false;
        }

        std::string _text = "[replace_fun4hook_void] C++ ";
        _text += thiz->orig_method_id_.class_descriptor;
        _text += " ";
        _text += thiz->orig_method_id_.method_name;
        _text += " ";
        _text += thiz->orig_method_id_.signature;
        _text += " ------";
        std::string tag = "Feadre_fjtik";
        do_Log_d(f_Log_d, code_ir, v0, v1,
                 tag, _text, insert_point);


        bool is_apploader = true;

        if (is_apploader) {
            auto f_HookBridge_replace_fun = fjava_api::get_HookBridge_replace_fun(code_ir);
            do_HookBridge_replace_fun(f_HookBridge_replace_fun, code_ir, insert_point);

        } else {
            // 是 系统 loader
            /// ActivityThread 方案 不行 ----------------
            std::string name_class_HookBridge = "com.zxc.jtik.HookBridge";
            std::string name_fun_replace_fun = "replace_fun";
            do_apploader_static_fun(code_ir, v0, v1, v2,
                                    -1, -1, v0,
                                    name_class_HookBridge, name_fun_replace_fun,
                                    insert_point);
        }

        // ******************************************************************
        if (num_add_reg > 0) {
            //扩展了几个寄存器 = 需要多少本地寄存器 - 原有多少个本地寄存器
            restore_reg_params4shift(code_ir, insert_point, num_add_reg);
        }
        if (is_clear) {
            cre_return_code(code_ir, orig_return_type, -1, insert_point);
        }
        LOGD("[replace_fun4hook_void] 完成 %s %s %s",
             thiz->orig_method_id_.class_descriptor,
             thiz->orig_method_id_.method_name,
             thiz->orig_method_id_.signature);
        return true;
    }


    /**
 * 这个是标准替换， 类型需要与 静态方法一一对应
 * @param thiz
 * @param code_ir
 * @return
 * 替换完成 Ljava/lang/System; exit ===== CodeIr 对应的 Smali 指令 =====
[0] invoke-static/range {v1 .. v2} Ltop/feadre/finject/ftest/THook;->ht06(Ljava/lang/String;Ljava/lang/String;)I
[1] move-result v1
[2] return v1

E  FATAL EXCEPTION: main
Process: top.feadre.rift, PID: 8414
java.lang.NoClassDefFoundError: Class not found using the boot class loader; no stack trace available
 */
    bool replace_fun4standard(slicer::DetourHook *thiz, lir::CodeIr *code_ir) {
        LOGD("[replace_fun4standard] start ....")

        const auto ir_method = code_ir->ir_method;
        if (!ir_method || !ir_method->code) {
            LOGE("[replace_return] no ir_method %s %s %s",
                 thiz->orig_method_id_.class_descriptor,
                 thiz->orig_method_id_.method_name,
                 thiz->orig_method_id_.signature);
            return false;
        }

        ir::Builder builder(code_ir->dex_ir);
        clear_original_instructions(code_ir);

        lir::Method *f_detour_method = fjava_api::get_detour_method(thiz, code_ir);

        auto orig_proto = ir_method->decl->prototype;
        auto orig_return_type = orig_proto->return_type;

        // 6. 计算参数寄存器范围
        dex::u4 regs = ir_method->code->registers; // 总寄存器数 regs=5
        dex::u4 ins_count = ir_method->code->ins_count; // 参数数量 ins_count=2
        dex::u4 param_base_reg = regs - ins_count;  // param_base_reg=3  参数寄存器是 5-2=3 和 4 范围是 [3, 4]

        // 执行 方法 ***************
        auto invoke = code_ir->Alloc<lir::Bytecode>();
        // 用这个不需要处理参数个数问题
        invoke->opcode = dex::OP_INVOKE_STATIC_RANGE;  // RANGE 通过寄存器范围传递（即指定起始寄存器和参数数量，而非逐个指定寄存器），适合参数较多的场景
        auto args = code_ir->Alloc<lir::VRegRange>(param_base_reg, ins_count);
        invoke->operands.push_back(args);
        invoke->operands.push_back(f_detour_method);
        code_ir->instructions.push_back(invoke);

        // 8. 处理返回值（根据原方法返回类型生成对应的move和return指令） void 返回
        if (strcmp(orig_return_type->descriptor->c_str(), "V") == 0) {
            // 无返回值
            auto ret = code_ir->Alloc<lir::Bytecode>();
            ret->opcode = dex::OP_RETURN_VOID;
            code_ir->instructions.push_back(ret);
        } else {
            // 有返回值：根据类型获取对应的move_result指令
            dex::Opcode move_opcode;
            dex::Opcode return_opcode;

            switch (orig_return_type->GetCategory()) {
                case ir::Type::Category::Scalar: // 标量类型 int、float、boolean、byte、short、char
                    move_opcode = dex::OP_MOVE_RESULT;
                    return_opcode = dex::OP_RETURN;
                    break;
                case ir::Type::Category::WideScalar: // 宽标量类型 long、double。
                    move_opcode = dex::OP_MOVE_RESULT_WIDE;
                    return_opcode = dex::OP_RETURN_WIDE;
                    break;
                case ir::Type::Category::Reference: // 引用类型 String、Object 自定义类 数组类型：int[]、String[]、Object[][]等
                    move_opcode = dex::OP_MOVE_RESULT_OBJECT;
                    return_opcode = dex::OP_RETURN_OBJECT;
                    break;
                default:
                    SLICER_CHECK(false && "不支持的返回类型");
                    return false;
            }

            // 移动返回值到原方法的返回寄存器
            auto move_result = code_ir->Alloc<lir::Bytecode>();
            move_result->opcode = move_opcode;
            move_result->operands.push_back(
                    code_ir->Alloc<lir::VReg>(param_base_reg));  // 使用第一个参数寄存器暂存返回值
            code_ir->instructions.push_back(move_result);

            // 生成返回指令
            auto ret = code_ir->Alloc<lir::Bytecode>();
            ret->opcode = return_opcode;
            ret->operands.push_back(code_ir->Alloc<lir::VReg>(param_base_reg));
            code_ir->instructions.push_back(ret);
        }
        LOGD("[replace_fun] 替换完成 %s %s %s",
             thiz->orig_method_id_.class_descriptor,
             thiz->orig_method_id_.method_name,
             thiz->orig_method_id_.signature
        );

        return true;
    }


    bool replace_fun4onEnter(slicer::DetourHook *thiz, lir::CodeIr *code_ir) {
        LOGD("[replace_fun4onEnter] start ....")

        const auto ir_method = code_ir->ir_method;
        if (!ir_method || !ir_method->code) {
            LOGE("[replace_fun4onEnter] no ir_method %s %s %s",
                 thiz->orig_method_id_.class_descriptor,
                 thiz->orig_method_id_.method_name,
                 thiz->orig_method_id_.signature);
            return false;
        }

        auto orig_return_type = ir_method->decl->prototype->return_type;
        ir::Builder builder(code_ir->dex_ir);

        int return_register = -1;
        bool is_clear = false;
        if (is_clear) {
            clear_original_instructions(code_ir);
        } else {
            bool is_wide_out = false;
            return_register = find_return_register(code_ir, &is_wide_out);
            LOGI("[replace_fun4onEnter] return_register= %d, is_wide_out=%d", return_register,
                 is_wide_out);
        };
        // 确定插入点（MOD: 修复返回点找到逻辑）
        auto insert_point = find_first_code(code_ir);

        // ******************************************************************

        dex::u2 num_add_reg = req_reg(code_ir, 5);
        auto num_reg_non_param_new = ir_method->code->registers - ir_method->code->ins_count;

        dex::u2 v0 = 0;
        dex::u2 v1 = 1;
        dex::u2 v2 = 2; // 调用参数数组
        dex::u2 v3 = 3; // 方法参数数组
        dex::u2 v4 = 4; // 方法参数数组

        long methodId = 123456;
        bool is_apploader = false;
        if (is_apploader) {
            auto f_THook_onEnter = fjava_api::get_THook_onEnter(code_ir);
            cre_arr_object0(code_ir, v0, v1, v2, insert_point);
            do_THook_onEnter(f_THook_onEnter, code_ir, v2, v3,
                             v2, methodId, insert_point);

            // 把 v0 赋值给参数寄存器
            restore_reg_params4type(code_ir,
                                    v2,  //arr_reg
                                    num_reg_non_param_new, //base_param_reg
                                    v0, v1,
                                    insert_point);
        } else {
            // 创建参数 Object[Object[]] 包层在v3,  Object[arg0,arg1] 数组 在v2 , 参数不要动只转换
            cre_arr_do_args4onEnter(code_ir, v0, v1, v3, methodId, insert_point);

            // 调用 onEnter 函数
            std::string name_class_THook = "top.feadre.finject.ftest.THook";
            std::string name_fun_onEnter = "onEnter";
            // 创建参数 Class[] 在 v2 （Class[]{Object[].class,Long.class}）
            cre_arr_class_args4onEnter(code_ir, v0, v1, v2, insert_point);

            // 执行结果返回到 v0`
            do_apploader_static_fun(code_ir, v0, v1, v4,
                                    v2, v3, v0,
                                    name_class_THook, name_fun_onEnter,
                                    insert_point);

            // 把 v0 赋值给参数寄存器
            restore_reg_params4type(code_ir,
                                    v0,  //arr_reg
                                    num_reg_non_param_new, //base_param_reg
                                    v1, v2,
                                    insert_point);
        }


        // ******************************************************************
        if (num_add_reg > 0) {
            //扩展了几个寄存器 = 需要多少本地寄存器 - 原有多少个本地寄存器
            restore_reg_params4shift(code_ir, insert_point, num_add_reg);
        }

        if (is_clear) {
            cre_return_code(code_ir, orig_return_type, -1, insert_point);
        }

        LOGD("[replace_fun4onEnter] 完成 %s %s %s",
             thiz->orig_method_id_.class_descriptor,
             thiz->orig_method_id_.method_name,
             thiz->orig_method_id_.signature);
        return true;
    }

    bool replace_fun4onExit(slicer::DetourHook *thiz, lir::CodeIr *code_ir) {
        LOGD("[replace_fun4onExit] start ....")

        const auto ir_method = code_ir->ir_method;
        if (!ir_method || !ir_method->code) {
            LOGE("[replace_fun4onExit] no ir_method %s %s %s",
                 thiz->orig_method_id_.class_descriptor,
                 thiz->orig_method_id_.method_name,
                 thiz->orig_method_id_.signature);
            return false;
        }

        auto orig_return_type = ir_method->decl->prototype->return_type;
        LOGD("[replace_fun4onExit] 原方法的类型 orig_return_type %s",
             orig_return_type->descriptor->c_str());
        ir::Builder builder(code_ir->dex_ir);

        // ******************************************************************

        int return_register = -1;

        bool is_clear = false;
        if (is_clear) {
            clear_original_instructions(code_ir);
        } else {
            // 如果没有清理原数据需要找到返回寄存器，进行hook
            bool is_wide_out = false;
            return_register = find_return_register(code_ir, &is_wide_out);
            LOGI("[replace_fun4onExit] return_register= %d, is_wide_out= %d", return_register,
                 is_wide_out);
        };

        // 确定插入点（MOD: 修复返回点找到逻辑）
        auto insert_point = find_first_code(code_ir);

        // ******************************************************************

        dex::u2 num_add_reg = req_reg(code_ir, 5);
        auto num_reg_non_param_new = ir_method->code->registers - ir_method->code->ins_count;

        dex::u2 v0 = 0;
        dex::u2 v1 = 1;
        dex::u2 v2 = 2; // 调用参数数组
        dex::u2 v3 = 3; // 方法参数数组
        dex::u2 v4 = 4; // 方法参数数组

        if (!is_clear) {
            // 如果原方法要执行，需要先恢复参数
            if (num_add_reg > 0) {
                //扩展了几个寄存器 = 需要多少本地寄存器 - 原有多少个本地寄存器
                restore_reg_params4shift(code_ir, insert_point, num_add_reg);
            }
        }


        int reg_return = -1; // 用于返回创建

        long method_id = 123456;

//        insert_point = find_return_code(code_ir);
        auto insert_return = find_return_code(code_ir);
        insert_point = insert_return;
        ++insert_point;
        code_ir->instructions.Remove(*insert_return);

        bool is_apploader = false;
        if (is_apploader) {
            cre_arr_do_args4onExit(code_ir,
                                   return_register, v4, insert_point);
            auto f_THook_onExit = fjava_api::get_THook_onExit(code_ir);
            do_THook_onExit(f_THook_onExit, code_ir, v4, v0,
                            v4, method_id, insert_point);
//            cre_return_code(code_ir, orig_return_type, v4, insert_point);
            reg_return = v4;
        } else {
            cre_arr_do_args4onExit(code_ir,
                                   return_register, v0, insert_point);
            cre_arr_object1(code_ir, v1, v0, v3, method_id, insert_point);


            // 调用 onEnter 函数
            std::string name_class_THook = "top.feadre.finject.ftest.THook";
            std::string name_fun_onExit = "onExit";
            // 创建参数 Class[] 在 v2 （Class[]{Object[].class,Long.class}）
            cre_arr_class_args4onExit(code_ir, v0, v1, v2, insert_point);

            do_apploader_static_fun(code_ir, v0, v1, v4,
                                    v2, v3, v0,
                                    name_class_THook, name_fun_onExit,
                                    insert_point);

            reg_return = v0;
        }

        // 执行 f_THook_onExit 结果到 v0
//        if (is_clear) {
//            cre_arr_do_args4onExit(code_ir, -1, v4, insert_point);
//        } else {
//            cre_arr_do_args4onExit(code_ir, return_register, v4, insert_point);
//        }
//        auto f_THook_onExit = fjava_api::get_THook_onExit(code_ir, builder);
//        do_THook_onExit(f_THook_onExit, code_ir, v4,
//                        v4, insert_point);


        // ******************************************************************
        // 不需要恢复参数
//        if (num_add_reg > 0) {
//            //扩展了几个寄存器 = 需要多少本地寄存器 - 原有多少个本地寄存器
//            restore_reg_params4shift(code_ir, insert_point, num_add_reg);
//        }

        // 根据 reg_return 寄存器处理
        cre_return_code(code_ir, orig_return_type, reg_return, insert_point);


        LOGD("[replace_fun4onExit] 完成 %s %s %s",
             thiz->orig_method_id_.class_descriptor,
             thiz->orig_method_id_.method_name,
             thiz->orig_method_id_.signature);
        return true;
    }

}