//
// Created by Administrator on 2025/8/26.
//

#include "fsmali_printer.h"
// 在合适的头文件中声明（如 smali_printer.h）
#include "../dexter/slicer/code_ir.h"
#include "../dexter/slicer/dex_ir.h"
#include "log.h"

#define TAG FEADRE_TAG("smali_printer")

#include <string>
#include <sstream>

// 转换整个 CodeIr 为 Smali 指令列表
std::string SmaliPrinter::CodeIrToSmali(lir::CodeIr *code_ir) {
    std::stringstream ss;
    ss << "===== CodeIr 对应的 Smali 指令 =====" << std::endl;
    size_t index = 0;
    for (auto it = code_ir->instructions.begin();
         it != code_ir->instructions.end(); ++it, ++index) {
        lir::Instruction *instr = *it;  // 通过迭代器获取当前元素
        // 处理字节码指令
        if (auto bytecode = dynamic_cast<lir::Bytecode *>(instr)) {
            ss << "[" << index << "] " << BytecodeToSmali(bytecode) << std::endl;
        }
            // 处理标签（如跳转目标）
        else if (auto label = dynamic_cast<lir::Label *>(instr)) {
            ss << ":label_" << label->offset << "  # 标签偏移量: " << label->offset
               << std::endl;
        }
            // 处理调试信息注解（可选）
        else if (auto dbg_anno = dynamic_cast<lir::DbgInfoAnnotation *>(instr)) {
            ss << "# 调试信息: " << DbgAnnotationToSmali(dbg_anno) << std::endl;
        }
    }
    ss << "===================================" << std::endl;
    return ss.str();
}


void SmaliPrinter::CodeIrToSmali4Print(lir::CodeIr *code_ir) {
    LOGD("[CodeIrToSmali4Print] ===== CodeIr 对应的 Smali 指令 =====");
    size_t index = 0;
    for (auto it = code_ir->instructions.begin();
         it != code_ir->instructions.end(); ++it, ++index) {
        lir::Instruction *instr = *it;  // 通过迭代器获取当前元素
        // 处理字节码指令
        if (auto bytecode = dynamic_cast<lir::Bytecode *>(instr)) {
            LOGD("  [%zu] %s", index, BytecodeToSmali(bytecode).c_str());
        }
            // 处理标签（如跳转目标）
        else if (auto label = dynamic_cast<lir::Label *>(instr)) {
            LOGD("  :label_%d  # 标签偏移量: %d", label->offset, label->offset);
        }
            // 处理调试信息注解（可选）
        else if (auto dbg_anno = dynamic_cast<lir::DbgInfoAnnotation *>(instr)) {
            LOGD("  # 调试信息: %s", DbgAnnotationToSmali(dbg_anno).c_str());
        }
    }
    LOGD("===================================");
}

// 转换单条字节码指令为 Smali
// 转换单条字节码指令为 Smali（完整版修复）
std::string SmaliPrinter::BytecodeToSmali(lir::Bytecode *bytecode) {
    std::string smali;
    // 1. 转换 opcode 为 Smali 指令名
    std::string opcodeName = OpcodeToName(bytecode->opcode);
    smali += opcodeName + " ";

    // 2. 优先处理调用类指令（invoke-*）的参数寄存器列表
    bool isInvokeOpcode = (opcodeName.find("invoke-") == 0);
    if (isInvokeOpcode && !bytecode->operands.empty()) {
        // 调用指令的第一个 operand 必为参数寄存器（VRegRange/VRegList/VReg）
        auto paramOperand = bytecode->operands[0];
        smali += "{ "; // 开始参数寄存器列表

        // 处理寄存器范围（如 {v0 .. v3}）
        if (auto vreg_range = dynamic_cast<lir::VRegRange *>(paramOperand)) {
            smali += "v" + std::to_string(vreg_range->base_reg) + " .. v" +
                     std::to_string(vreg_range->base_reg + vreg_range->count - 1);
        }
            // 处理离散寄存器列表（如 {v0, v2}，对应 VRegList）
        else if (auto vreg_list = dynamic_cast<lir::VRegList *>(paramOperand)) {
            for (size_t i = 0; i < vreg_list->registers.size(); ++i) {
                if (i > 0) smali += ", ";
                smali += "v" + std::to_string(vreg_list->registers[i]);
            }
        }
            // 处理单个寄存器（如 {v0}）
        else if (auto vreg = dynamic_cast<lir::VReg *>(paramOperand)) {
            smali += "v" + std::to_string(vreg->reg);
        }
        // 无参数时，此处会生成空的 "{ }"

        smali += " } "; // 结束参数寄存器列表

        // 3. 处理调用指令的第二个 operand（方法引用）
        if (bytecode->operands.size() > 1) {
            auto methodOperand = bytecode->operands[1];
            if (auto method = dynamic_cast<lir::Method *>(methodOperand)) {
                smali += MethodToSmali(method->ir_method) + " ";
            }
        }
    }
        // 4. 处理非调用类指令（原有逻辑保留，补充 VRegList 支持）
    else {
        for (auto &operand: bytecode->operands) {
            if (auto vreg_range = dynamic_cast<lir::VRegRange *>(operand)) {
                // 寄存器范围（如 {v0 .. v3}）
                smali += "{v" + std::to_string(vreg_range->base_reg) + " .. v" +
                         std::to_string(vreg_range->base_reg + vreg_range->count - 1) + "} ";
            } else if (auto vreg_list = dynamic_cast<lir::VRegList *>(operand)) {
                // 新增：离散寄存器列表（如 {v0, v2}）
                smali += "{ ";
                for (size_t i = 0; i < vreg_list->registers.size(); ++i) {
                    if (i > 0) smali += ", ";
                    smali += "v" + std::to_string(vreg_list->registers[i]);
                }
                smali += " } ";
            } else if (auto vreg = dynamic_cast<lir::VReg *>(operand)) {
                // 单个寄存器（如 v0）
                smali += "v" + std::to_string(vreg->reg) + " ";
            } else if (auto vreg_pair = dynamic_cast<lir::VRegPair *>(operand)) {
                // 宽寄存器对（如 v0/v1，用于 long/double）
                smali += "v" + std::to_string(vreg_pair->base_reg) + "/v" +
                         std::to_string(vreg_pair->base_reg + 1) + " ";
            } else if (auto method = dynamic_cast<lir::Method *>(operand)) {
                // 方法引用（非调用指令场景，如特殊指令）
                smali += MethodToSmali(method->ir_method) + " ";
            } else if (auto type = dynamic_cast<lir::Type *>(operand)) {
                // 类型引用（如 Ljava/lang/Object;）
                smali += type->ir_type->descriptor->c_str() + std::string(" ");
            } else if (auto cst32 = dynamic_cast<lir::Const32 *>(operand)) {
                // 32位常量（优化：区分数字/字符串，避免统一 #0x 前缀）
                if (dynamic_cast<lir::String *>(operand)) {
                    // 若为字符串常量，后续会被覆盖，此处仅容错
                } else {
                    smali += "#0x" + std::to_string(cst32->u.u4_value) + " ";
                }
            } else if (auto cst64 = dynamic_cast<lir::Const64 *>(operand)) {
                // 64位常量
                smali += "#0x" + std::to_string(cst64->u.u8_value) + " ";
            } else if (auto code_loc = dynamic_cast<lir::CodeLocation *>(operand)) {
                // 代码位置（跳转目标标签）
                smali += ":label_" + std::to_string(code_loc->label->offset) + " ";
            } else if (auto str = dynamic_cast<lir::String *>(operand)) {
                // 字符串常量（如 "hello"）
                smali += "\"" + std::string(str->ir_string->c_str()) + "\" ";
            }
        }
    }

    // 移除末尾多余空格（避免格式混乱）
    if (!smali.empty() && smali.back() == ' ') {
        smali.pop_back();
    }

    return smali;
}

// 转换 opcode 为 Smali 指令名（补充常见指令）
std::string SmaliPrinter::OpcodeToName(dex::Opcode opcode) {
    switch (opcode) {
        // 1. 返回类指令（原有保持不变）
        case dex::OP_RETURN_VOID:
            return "return-void";
        case dex::OP_RETURN:
            return "return";
        case dex::OP_RETURN_OBJECT:
            return "return-object";
        case dex::OP_RETURN_WIDE:
            return "return-wide";
        case dex::OP_RETURN_VOID_NO_BARRIER:
            return "return-void-no-barrier";

            // 2. 移动类指令（原有保持不变）
        case dex::OP_MOVE:
            return "move";
        case dex::OP_MOVE_FROM16:
            return "move/from16";
        case dex::OP_MOVE_16:
            return "move/16";
        case dex::OP_MOVE_WIDE:
            return "move-wide";
        case dex::OP_MOVE_WIDE_FROM16:
            return "move-wide/from16";
        case dex::OP_MOVE_WIDE_16:
            return "move-wide/16";
        case dex::OP_MOVE_OBJECT:
            return "move-object";  // 非静态方法用的关键指令
        case dex::OP_MOVE_OBJECT_FROM16:
            return "move-object/from16";
        case dex::OP_MOVE_OBJECT_16:
            return "move-object/16";
        case dex::OP_MOVE_RESULT:
            return "move-result";
        case dex::OP_MOVE_RESULT_WIDE:
            return "move-result-wide";
        case dex::OP_MOVE_RESULT_OBJECT:
            return "move-result-object";
        case dex::OP_MOVE_EXCEPTION:
            return "move-exception";

            // 3. 常量类指令（原有保持不变）
        case dex::OP_CONST_4:
            return "const/4";  // 静态方法设null用的关键指令
        case dex::OP_CONST_16:
            return "const/16";
        case dex::OP_CONST:
            return "const";
        case dex::OP_CONST_HIGH16:
            return "const/high16";
        case dex::OP_CONST_WIDE_16:
            return "const-wide/16";
        case dex::OP_CONST_WIDE_32:
            return "const-wide/32";
        case dex::OP_CONST_WIDE:
            return "const-wide";
        case dex::OP_CONST_WIDE_HIGH16:
            return "const-wide/high16";
        case dex::OP_CONST_STRING:
            return "const-string";
        case dex::OP_CONST_STRING_JUMBO:
            return "const-string/jumbo";
        case dex::OP_CONST_CLASS:
            return "const-class";
        case dex::OP_CONST_METHOD_HANDLE:
            return "const-method-handle";
        case dex::OP_CONST_METHOD_TYPE:
            return "const-method-type";

            // 4. 类型检查/转换类指令（原有保持不变，补充注释）
        case dex::OP_CHECK_CAST:
            return "check-cast";  // 类型强转（如null转Object）
        case dex::OP_INSTANCE_OF:
            return "instance-of";  // 判断对象是否为某类实例
        case dex::OP_INT_TO_DOUBLE:
            return "int-to-double";  // 0x83：int → double
        case dex::OP_INT_TO_FLOAT:
            return "int-to-float";   // 可选补充：int → float（避免后续类似问题）
        case dex::OP_LONG_TO_DOUBLE:
            return "long-to-double"; // 可选补充：long → double
        case dex::OP_FLOAT_TO_DOUBLE:
            return "float-to-double";// 可选补充：float → double
            // int ↔ double
        case dex::OP_DOUBLE_TO_INT:    // 0x8C：double → int
            return "double-to-int";
            // long ↔ double
        case dex::OP_DOUBLE_TO_LONG:   // 0x8B：double → long（解决0x8B识别）
            return "double-to-long";
            // float ↔ double
        case dex::OP_DOUBLE_TO_FLOAT:  // 0x8D：double → float
            return "double-to-float";
            // int ↔ long/float
        case dex::OP_INT_TO_LONG:      // 0x80：int → long
            return "int-to-long";
        case dex::OP_LONG_TO_INT:      // 0x84：long → int
            return "long-to-int";
        case dex::OP_LONG_TO_FLOAT:    // 0x85：long → float
            return "long-to-float";
        case dex::OP_FLOAT_TO_INT:     // 0x87：float → int
            return "float-to-int";
        case dex::OP_FLOAT_TO_LONG:    // 0x88：float → long
            return "float-to-long";


            // 5. 对象创建类指令（新增：解决new-instance问题，补充相关指令）
        case dex::OP_NEW_INSTANCE:
            return "new-instance";  // 核心补充：创建对象实例（如new Object()）
        case dex::OP_NEW_ARRAY:
            return "new-array";  // 创建数组（如new int[5]）
        case dex::OP_FILLED_NEW_ARRAY:
            return "filled-new-array";  // 创建数组并初始化元素（如new int[]{1,2}）
        case dex::OP_FILLED_NEW_ARRAY_RANGE:
            return "filled-new-array/range";  // filled-new-array的range版本（多参数优化）
        case dex::OP_FILL_ARRAY_DATA:
            return "fill-array-data";  // 用预定义数据填充数组（配合.fill-array-data指令块）

            // 6. 数组访问类指令（原有保持不变，归类调整）
        case dex::OP_ARRAY_LENGTH:
            return "array-length";  // 获取数组长度
        case dex::OP_AGET:
            return "aget";  // 取数组元素（int/short等基本类型）
        case dex::OP_AGET_WIDE:
            return "aget-wide";  // 取数组元素（long/double）
        case dex::OP_AGET_OBJECT:
            return "aget-object";  // 取数组元素（对象类型）
        case dex::OP_AGET_BOOLEAN:
            return "aget-boolean";  // 取数组元素（boolean）
        case dex::OP_AGET_BYTE:
            return "aget-byte";  // 取数组元素（byte）
        case dex::OP_AGET_CHAR:
            return "aget-char";  // 取数组元素（char）
        case dex::OP_AGET_SHORT:
            return "aget-short";  // 取数组元素（short）
        case dex::OP_APUT:
            return "aput";  // 存数组元素（int/short等基本类型）
        case dex::OP_APUT_WIDE:
            return "aput-wide";  // 存数组元素（long/double）
        case dex::OP_APUT_OBJECT:
            return "aput-object";  // 存数组元素（对象类型）
        case dex::OP_APUT_BOOLEAN:
            return "aput-boolean";  // 存数组元素（boolean）
        case dex::OP_APUT_BYTE:
            return "aput-byte";  // 存数组元素（byte）
        case dex::OP_APUT_CHAR:
            return "aput-char";  // 存数组元素（char）
        case dex::OP_APUT_SHORT:
            return "aput-short";  // 存数组元素（short）

            // 7. 方法调用类指令（原有保持不变）
        case dex::OP_INVOKE_VIRTUAL:
            return "invoke-virtual";  // 调用非静态方法（如obj.method()）
        case dex::OP_INVOKE_SUPER:
            return "invoke-super";  // 调用父类方法（如super.method()）
        case dex::OP_INVOKE_DIRECT:
            return "invoke-direct";  // 调用构造方法或私有方法
        case dex::OP_INVOKE_STATIC:
            return "invoke-static";  // 调用静态方法（如Class.method()）
        case dex::OP_INVOKE_STATIC_RANGE:
            return "invoke-static/range";  // invoke-static的range版本（多参数优化）
        case dex::OP_INVOKE_INTERFACE:
            return "invoke-interface";  // 调用接口方法
        case dex::OP_INVOKE_INTERFACE_RANGE:
            return "invoke-interface/range";  // invoke-interface的range版本
        case dex::OP_INVOKE_POLYMORPHIC:
            return "invoke-polymorphic";  // 调用泛型方法（如Method.invoke）
        case dex::OP_INVOKE_POLYMORPHIC_RANGE:
            return "invoke-polymorphic/range";  // invoke-polymorphic的range版本
        case dex::OP_INVOKE_CUSTOM:
            return "invoke-custom";  // 调用自定义方法（如Lambda、注解处理）
        case dex::OP_INVOKE_CUSTOM_RANGE:
            return "invoke-custom/range";  // invoke-custom的range版本

            // 8. 跳转类指令（原有保持不变）
        case dex::OP_GOTO:
            return "goto";  // 无条件跳转
        case dex::OP_GOTO_16:
            return "goto/16";  // 短距离跳转（16位偏移）
        case dex::OP_GOTO_32:
            return "goto/32";  // 长距离跳转（32位偏移）
        case dex::OP_PACKED_SWITCH:
            return "packed-switch";  // 连续值switch（如case 1,2,3）
        case dex::OP_SPARSE_SWITCH:
            return "sparse-switch";  // 离散值switch（如case 1,5,10）
        case dex::OP_IF_EQ:
            return "if-eq";  // 等于则跳转（对比两个寄存器）
        case dex::OP_IF_NE:
            return "if-ne";  // 不等于则跳转
        case dex::OP_IF_LT:
            return "if-lt";  // 小于则跳转
        case dex::OP_IF_GE:
            return "if-ge";  // 大于等于则跳转
        case dex::OP_IF_GT:
            return "if-gt";  // 大于则跳转
        case dex::OP_IF_LE:
            return "if-le";  // 小于等于则跳转
        case dex::OP_IF_EQZ:
            return "if-eqz";  // 等于0/ null则跳转（单寄存器判断）
        case dex::OP_IF_NEZ:
            return "if-nez";  // 不等于0/ null则跳转
        case dex::OP_IF_LTZ:
            return "if-ltz";  // 小于0则跳转
        case dex::OP_IF_GEZ:
            return "if-gez";  // 大于等于0则跳转
        case dex::OP_IF_GTZ:
            return "if-gtz";  // 大于0则跳转
        case dex::OP_IF_LEZ:
            return "if-lez";  // 小于等于0则跳转

            // 9. 其他常用指令（原有保持不变）
        case dex::OP_NOP:
            return "nop";  // 空指令（占位用）
        case dex::OP_MONITOR_ENTER:
            return "monitor-enter";  // 加锁（synchronized块进入）
        case dex::OP_MONITOR_EXIT:
            return "monitor-exit";  // 解锁（synchronized块退出）
        case dex::OP_THROW:
            return "throw";  // 抛出异常

            // 10. 字段访问类指令（原有保持不变）
        case dex::OP_IGET:
            return "iget";  // 取实例字段（int/short等基本类型）
        case dex::OP_IGET_WIDE:
            return "iget-wide";  // 取实例字段（long/double）
        case dex::OP_IGET_OBJECT:
            return "iget-object";  // 取实例字段（对象类型）
        case dex::OP_IGET_BOOLEAN:
            return "iget-boolean";  // 取实例字段（boolean）
        case dex::OP_IGET_BYTE:
            return "iget-byte";  // 取实例字段（byte）
        case dex::OP_IGET_CHAR:
            return "iget-char";  // 取实例字段（char）
        case dex::OP_IGET_SHORT:
            return "iget-short";  // 取实例字段（short）
        case dex::OP_IPUT:
            return "iput";  // 存实例字段（int/short等基本类型）
        case dex::OP_IPUT_WIDE:
            return "iput-wide";  // 存实例字段（long/double）
        case dex::OP_IPUT_OBJECT:
            return "iput-object";  // 存实例字段（对象类型）
        case dex::OP_SGET:
            return "sget";  // 取静态字段（int/short等基本类型）
        case dex::OP_SGET_WIDE:
            return "sget-wide";  // 取静态字段（long/double）
        case dex::OP_SGET_OBJECT:
            return "sget-object";  // 取静态字段（对象类型）

            // double 类型运算指令
        case dex::OP_MUL_DOUBLE:
            return "mul-double";     // 0xCD：double * double
        case dex::OP_ADD_DOUBLE:
            return "add-double";     // 可选补充：double + double
        case dex::OP_SUB_DOUBLE:
            return "sub-double";     // 可选补充：double - double
        case dex::OP_DIV_DOUBLE:
            return "div-double";     // 可选补充：double / double
        case dex::OP_REM_DOUBLE:       // 0xCE：double % double
            return "rem-double";
        case dex::OP_NEG_DOUBLE:       // 0xCF：-double（取反）
            return "neg-double";

            // long 类型运算指令
        case dex::OP_ADD_LONG:         // 0xB8：long + long
            return "add-long";
        case dex::OP_SUB_LONG:         // 0xBA：long - long
            return "sub-long";
        case dex::OP_MUL_LONG:         // 0xBB：long * long
            return "mul-long";
        case dex::OP_DIV_LONG:         // 0xB9：long / long
            return "div-long";
        case dex::OP_REM_LONG:         // 0xBC：long % long
            return "rem-long";
        case dex::OP_NEG_LONG:         // 0xBD：-long
            return "neg-long";


            // 11. 默认情况（优化：用十六进制格式化，避免十进制显示混乱）
        default:
            // 补充：用std::hex确保 opcode 以十六进制输出（符合Dex指令习惯）
            std::stringstream ss;
            ss << "unknown-opcode-0x" << std::hex << std::uppercase << static_cast<int>(opcode);
            return ss.str();
    }
}

// 转换方法为 Smali 格式
std::string SmaliPrinter::MethodToSmali(ir::MethodDecl *method_decl) {
    if (!method_decl || !method_decl->parent || !method_decl->prototype) {
        return "unknown_method";
    }
    std::string smali;
    // 1. 类名（如 Ljava/lang/Integer;）
    smali += method_decl->parent->descriptor->c_str();
    // 2. 方法分隔符（->）
    smali += "->";
    // 3. 方法名（如 valueOf）
    smali += method_decl->name->c_str();
    // 4. 方法签名（如 (I)Ljava/lang/Integer;）
    smali += "(";
    auto param_types = method_decl->prototype->param_types;
    if (param_types && !param_types->types.empty()) {
        for (auto param_type: param_types->types) {
            smali += param_type->descriptor->c_str();  // 参数类型（如 I、Ljava/lang/Object;）
        }
    }
    smali += ")";
    // 5. 返回类型（如 Ljava/lang/Integer;、V）
    smali += method_decl->prototype->return_type->descriptor->c_str();
    return smali;
}

// 转换调试信息注解（简化版）
std::string SmaliPrinter::DbgAnnotationToSmali(lir::DbgInfoAnnotation *dbg_anno) {
    switch (dbg_anno->dbg_opcode) {
        case dex::DBG_ADVANCE_LINE:
            return "advance-line";
        case dex::DBG_SET_FILE:
            return "set-file";
        default:
            return "debug-annotation-0x" + std::to_string(dbg_anno->dbg_opcode);
    }
}

// 用 Visitor 先识别 operand 类型（不依赖 RTTI）
struct OperandTypeDetector : public lir::Visitor {
    enum Kind {
        KUnknown, KVReg, KVRegPair, KVRegList, KVRegRange,
        KConst32, KConst64, KString, KType, KField, KMethod,
        KCodeLocation, KLineNumber
    };
    Kind kind = KUnknown;

    bool Visit(lir::VReg *v) override {
        kind = KVReg;
        return true;
    }

    bool Visit(lir::VRegPair *v) override {
        kind = KVRegPair;
        return true;
    }

    bool Visit(lir::VRegList *v) override {
        kind = KVRegList;
        return true;
    }

    bool Visit(lir::VRegRange *v) override {
        kind = KVRegRange;
        return true;
    }

    bool Visit(lir::Const32 *c) override {
        kind = KConst32;
        return true;
    }

    bool Visit(lir::Const64 *c) override {
        kind = KConst64;
        return true;
    }

    bool Visit(lir::String *s) override {
        kind = KString;
        return true;
    }

    bool Visit(lir::Type *t) override {
        kind = KType;
        return true;
    }

    bool Visit(lir::Field *f) override {
        kind = KField;
        return true;
    }

    bool Visit(lir::Method *m) override {
        kind = KMethod;
        return true;
    }

    bool Visit(lir::CodeLocation *cl) override {
        kind = KCodeLocation;
        return true;
    }

    bool Visit(lir::LineNumber *ln) override {
        kind = KLineNumber;
        return true;
    }
};

// 把单条 bytecode 转成 smali 风格
// 把单条 bytecode 转成 smali 风格（完整版修复）
std::string SmaliPrinter::ToSmali(lir::Bytecode *bc) {
    if (bc == nullptr) return "null";

    std::ostringstream ss;
    // 优先使用自定义的 OpcodeToName（确保指令名统一），而非 dex::GetOpcodeName
    ss << OpcodeToName(bc->opcode);

    for (size_t i = 0; i < bc->operands.size(); ++i) {
        lir::Operand *op = bc->operands[i];
        if (!op) {
            ss << " null";
            continue;
        }

        // 通过 Visitor 识别 operand 类型（不依赖 RTTI）
        OperandTypeDetector det;
        op->Accept(&det);

        switch (det.kind) {
            case OperandTypeDetector::KVReg: {
                auto *r = static_cast<lir::VReg *>(op);
                ss << " v" << r->reg;
                break;
            }
            case OperandTypeDetector::KVRegPair: {
                auto *rp = static_cast<lir::VRegPair *>(op);
                // 优化：统一为 Smali 风格的 "v0/v1"，而非 "v0:pair"
                ss << " v" << rp->base_reg << "/v" << (rp->base_reg + 1);
                break;
            }
            case OperandTypeDetector::KVRegList: {
                auto *rl = static_cast<lir::VRegList *>(op);
                ss << " {";
                // 修复：标红的 regs 替换为实际成员变量 registers
                for (size_t k = 0; k < rl->registers.size(); ++k) {
                    if (k > 0) ss << ",";
                    ss << "v" << rl->registers[k];
                }
                ss << "}";
                break;
            }
            case OperandTypeDetector::KVRegRange: {
                auto *rr = static_cast<lir::VRegRange *>(op);
                ss << " {v" << rr->base_reg << " .. v" << (rr->base_reg + rr->count - 1) << "}";
                break;
            }
            case OperandTypeDetector::KConst32: {
                auto *c = static_cast<lir::Const32 *>(op);
                // 优化：显示十六进制（符合 Smali 习惯），而非十进制
                ss << " #0x" << std::hex << std::uppercase << c->u.u4_value;
                break;
            }
            case OperandTypeDetector::KConst64: {
                auto *c = static_cast<lir::Const64 *>(op);
                ss << " #0x" << std::hex << std::uppercase << c->u.u8_value;
                break;
            }
            case OperandTypeDetector::KString: {
                auto *s = static_cast<lir::String *>(op);
                // 优化：直接显示字符串内容（而非 string@index），更直观
                if (s->ir_string) {
                    ss << " \"" << s->ir_string->c_str() << "\"";
                } else {
                    ss << " string@" << s->index;
                }
                break;
            }
            case OperandTypeDetector::KType: {
                auto *t = static_cast<lir::Type *>(op);
                if (t->ir_type) {
                    // 显示 Smali 风格的类型描述符（如 Ljava/lang/Thread;）
                    ss << " " << t->ir_type->descriptor->c_str();
                } else {
                    ss << " type@" << t->index;
                }
                break;
            }
            case OperandTypeDetector::KField: {
                auto *f = static_cast<lir::Field *>(op);
                if (f->ir_field) {
                    // 显示 Smali 风格的字段引用（如 Lcom/xxx;->field:I）
                    ss << " " << f->ir_field->parent->descriptor->c_str()
                       << "->" << f->ir_field->name->c_str()
                       << ":" << f->ir_field->type->descriptor->c_str();
                } else {
                    ss << " field@" << f->index;
                }
                break;
            }
            case OperandTypeDetector::KMethod: {
                auto *m = static_cast<lir::Method *>(op);
                if (m->ir_method) {
                    // 显示 Smali 风格的方法引用（复用 MethodToSmali，确保统一）
                    ss << " " << MethodToSmali(m->ir_method);
                } else {
                    ss << " method@" << m->index;
                }
                break;
            }
            case OperandTypeDetector::KCodeLocation: {
                auto *cl = static_cast<lir::CodeLocation *>(op);
                if (cl->label) {
                    ss << " :label_" << cl->label->offset;
                } else {
                    ss << " :label?";
                }
                break;
            }
            case OperandTypeDetector::KLineNumber: {
                auto *ln = static_cast<lir::LineNumber *>(op);
                ss << " .line " << ln->line;
                break;
            }
            default:
                ss << " ?";
                break;
        } // end switch
    } // end for operands

    return ss.str();
}

