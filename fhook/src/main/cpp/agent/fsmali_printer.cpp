//
// Created by Administrator on 2025/8/26.
//

#include "fsmali_printer.h"
// 在合适的头文件中声明（如 smali_printer.h）
#include "../FGlobal_flib.h"

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


void SmaliPrinter::CodeIrToSmali4Print(lir::CodeIr *code_ir, std::string &text) {
    LOGD("[CodeIrToSmali4Print] ===== %s CodeIr 对应的 Smali 指令 =====", text.c_str());
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

void SmaliPrinter::CodeIrToSmali4Print(lir::CodeIr *code_ir) {
    std::string text = "";
    CodeIrToSmali4Print(code_ir, text);
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
#include <sstream>
#include <iomanip>
#include <cstdint>

std::string SmaliPrinter::OpcodeToName(dex::Opcode opcode) {
    const uint8_t op = static_cast<uint8_t>(opcode);
    switch (op) {
        // 0x00 - 0x0F
        case 0x00: return "nop";
        case 0x01: return "move";
        case 0x02: return "move/from16";
        case 0x03: return "move/16";
        case 0x04: return "move-wide";
        case 0x05: return "move-wide/from16";
        case 0x06: return "move-wide/16";
        case 0x07: return "move-object";
        case 0x08: return "move-object/from16";
        case 0x09: return "move-object/16";
        case 0x0A: return "move-result";
        case 0x0B: return "move-result-wide";
        case 0x0C: return "move-result-object";
        case 0x0D: return "move-exception";
        case 0x0E: return "return-void";
        case 0x0F: return "return";

            // 0x10 - 0x1F
        case 0x10: return "return-wide";
        case 0x11: return "return-object";
        case 0x12: return "const/4";
        case 0x13: return "const/16";
        case 0x14: return "const";
        case 0x15: return "const/high16";
        case 0x16: return "const-wide/16";
        case 0x17: return "const-wide/32";
        case 0x18: return "const-wide";
        case 0x19: return "const-wide/high16";
        case 0x1A: return "const-string";
        case 0x1B: return "const-string/jumbo";
        case 0x1C: return "const-class";
        case 0x1D: return "monitor-enter";
        case 0x1E: return "monitor-exit";
        case 0x1F: return "check-cast";

            // 0x20 - 0x2F
        case 0x20: return "instance-of";
        case 0x21: return "array-length";
        case 0x22: return "new-instance";
        case 0x23: return "new-array";
        case 0x24: return "filled-new-array";
        case 0x25: return "filled-new-array/range";
        case 0x26: return "fill-array-data";
        case 0x27: return "throw";
        case 0x28: return "goto";
        case 0x29: return "goto/16";
        case 0x2A: return "goto/32";
        case 0x2B: return "packed-switch";
        case 0x2C: return "sparse-switch";
        case 0x2D: return "cmpl-float";
        case 0x2E: return "cmpg-float";
        case 0x2F: return "cmpl-double";

            // 0x30 - 0x3F
        case 0x30: return "cmpg-double";
        case 0x31: return "cmp-long";
        case 0x32: return "if-eq";
        case 0x33: return "if-ne";
        case 0x34: return "if-lt";
        case 0x35: return "if-ge";
        case 0x36: return "if-gt";
        case 0x37: return "if-le";
        case 0x38: return "if-eqz";
        case 0x39: return "if-nez";
        case 0x3A: return "if-ltz";
        case 0x3B: return "if-gez";
        case 0x3C: return "if-gtz";
        case 0x3D: return "if-lez";
        case 0x3E: return "unused-3e";
        case 0x3F: return "unused-3f";

            // 0x40 - 0x4F
        case 0x40: return "unused-40";
        case 0x41: return "unused-41";
        case 0x42: return "unused-42";
        case 0x43: return "unused-43";
        case 0x44: return "aget";
        case 0x45: return "aget-wide";
        case 0x46: return "aget-object";
        case 0x47: return "aget-boolean";
        case 0x48: return "aget-byte";
        case 0x49: return "aget-char";
        case 0x4A: return "aget-short";
        case 0x4B: return "aput";
        case 0x4C: return "aput-wide";
        case 0x4D: return "aput-object";
        case 0x4E: return "aput-boolean";
        case 0x4F: return "aput-byte";

            // 0x50 - 0x5F
        case 0x50: return "aput-char";
        case 0x51: return "aput-short";
        case 0x52: return "iget";
        case 0x53: return "iget-wide";
        case 0x54: return "iget-object";
        case 0x55: return "iget-boolean";
        case 0x56: return "iget-byte";
        case 0x57: return "iget-char";
        case 0x58: return "iget-short";
        case 0x59: return "iput";
        case 0x5A: return "iput-wide";
        case 0x5B: return "iput-object";
        case 0x5C: return "iput-boolean";
        case 0x5D: return "iput-byte";
        case 0x5E: return "iput-char";
        case 0x5F: return "iput-short";

            // 0x60 - 0x6F
        case 0x60: return "sget";
        case 0x61: return "sget-wide";
        case 0x62: return "sget-object";
        case 0x63: return "sget-boolean";
        case 0x64: return "sget-byte";
        case 0x65: return "sget-char";
        case 0x66: return "sget-short";
        case 0x67: return "sput";
        case 0x68: return "sput-wide";
        case 0x69: return "sput-object";
        case 0x6A: return "sput-boolean";
        case 0x6B: return "sput-byte";
        case 0x6C: return "sput-char";
        case 0x6D: return "sput-short";
        case 0x6E: return "invoke-virtual";
        case 0x6F: return "invoke-super";

            // 0x70 - 0x7F
        case 0x70: return "invoke-direct";
        case 0x71: return "invoke-static";
        case 0x72: return "invoke-interface";
        case 0x73: return "return-void-no-barrier";
        case 0x74: return "invoke-virtual/range";
        case 0x75: return "invoke-super/range";
        case 0x76: return "invoke-direct/range";
        case 0x77: return "invoke-static/range";
        case 0x78: return "invoke-interface/range";
        case 0x79: return "unused-79";
        case 0x7A: return "unused-7a";
        case 0x7B: return "neg-int";
        case 0x7C: return "not-int";
        case 0x7D: return "neg-long";
        case 0x7E: return "not-long";
        case 0x7F: return "neg-float";

            // 0x80 - 0x8F
        case 0x80: return "neg-double";
        case 0x81: return "int-to-long";
        case 0x82: return "int-to-float";
        case 0x83: return "int-to-double";
        case 0x84: return "long-to-int";
        case 0x85: return "long-to-float";
        case 0x86: return "long-to-double";
        case 0x87: return "float-to-int";
        case 0x88: return "float-to-long";
        case 0x89: return "float-to-double";
        case 0x8A: return "double-to-int";
        case 0x8B: return "double-to-long";
        case 0x8C: return "double-to-float";
        case 0x8D: return "int-to-byte";
        case 0x8E: return "int-to-char";
        case 0x8F: return "int-to-short";

            // 0x90 - 0x9F
        case 0x90: return "add-int";
        case 0x91: return "sub-int";
        case 0x92: return "mul-int";
        case 0x93: return "div-int";
        case 0x94: return "rem-int";
        case 0x95: return "and-int";
        case 0x96: return "or-int";
        case 0x97: return "xor-int";
        case 0x98: return "shl-int";
        case 0x99: return "shr-int";
        case 0x9A: return "ushr-int";
        case 0x9B: return "add-long";
        case 0x9C: return "sub-long";
        case 0x9D: return "mul-long";
        case 0x9E: return "div-long";
        case 0x9F: return "rem-long";

            // 0xA0 - 0xAF
        case 0xA0: return "and-long";
        case 0xA1: return "or-long";
        case 0xA2: return "xor-long";
        case 0xA3: return "shl-long";
        case 0xA4: return "shr-long";
        case 0xA5: return "ushr-long";
        case 0xA6: return "add-float";
        case 0xA7: return "sub-float";
        case 0xA8: return "mul-float";
        case 0xA9: return "div-float";
        case 0xAA: return "rem-float";
        case 0xAB: return "add-double";
        case 0xAC: return "sub-double";
        case 0xAD: return "mul-double";
        case 0xAE: return "div-double";
        case 0xAF: return "rem-double";

            // 0xB0 - 0xBF
        case 0xB0: return "add-int/2addr";
        case 0xB1: return "sub-int/2addr";
        case 0xB2: return "mul-int/2addr";
        case 0xB3: return "div-int/2addr";
        case 0xB4: return "rem-int/2addr";
        case 0xB5: return "and-int/2addr";
        case 0xB6: return "or-int/2addr";
        case 0xB7: return "xor-int/2addr";
        case 0xB8: return "shl-int/2addr";
        case 0xB9: return "shr-int/2addr";
        case 0xBA: return "ushr-int/2addr";
        case 0xBB: return "add-long/2addr";
        case 0xBC: return "sub-long/2addr";
        case 0xBD: return "mul-long/2addr";
        case 0xBE: return "div-long/2addr";
        case 0xBF: return "rem-long/2addr";

            // 0xC0 - 0xCF
        case 0xC0: return "and-long/2addr";
        case 0xC1: return "or-long/2addr";
        case 0xC2: return "xor-long/2addr";
        case 0xC3: return "shl-long/2addr";
        case 0xC4: return "shr-long/2addr";
        case 0xC5: return "ushr-long/2addr";
        case 0xC6: return "add-float/2addr";
        case 0xC7: return "sub-float/2addr";
        case 0xC8: return "mul-float/2addr";
        case 0xC9: return "div-float/2addr";
        case 0xCA: return "rem-float/2addr";
        case 0xCB: return "add-double/2addr";
        case 0xCC: return "sub-double/2addr";
        case 0xCD: return "mul-double/2addr";
        case 0xCE: return "div-double/2addr";
        case 0xCF: return "rem-double/2addr";

            // 0xD0 - 0xDF
        case 0xD0: return "add-int/lit16";
        case 0xD1: return "rsub-int";
        case 0xD2: return "mul-int/lit16";
        case 0xD3: return "div-int/lit16";
        case 0xD4: return "rem-int/lit16";
        case 0xD5: return "and-int/lit16";
        case 0xD6: return "or-int/lit16";
        case 0xD7: return "xor-int/lit16";
        case 0xD8: return "add-int/lit8";
        case 0xD9: return "rsub-int/lit8";
        case 0xDA: return "mul-int/lit8";
        case 0xDB: return "div-int/lit8";
        case 0xDC: return "rem-int/lit8";
        case 0xDD: return "and-int/lit8";
        case 0xDE: return "or-int/lit8";
        case 0xDF: return "xor-int/lit8";

            // 0xE0 - 0xEF
        case 0xE0: return "shl-int/lit8";
        case 0xE1: return "shr-int/lit8";
        case 0xE2: return "ushr-int/lit8";
        case 0xE3: return "iget-quick";
        case 0xE4: return "iget-wide-quick";
        case 0xE5: return "iget-object-quick";
        case 0xE6: return "iput-quick";
        case 0xE7: return "iput-wide-quick";
        case 0xE8: return "iput-object-quick";
        case 0xE9: return "invoke-virtual-quick";
        case 0xEA: return "invoke-virtual/range-quick";
        case 0xEB: return "iput-boolean-quick";
        case 0xEC: return "iput-byte-quick";
        case 0xED: return "iput-char-quick";
        case 0xEE: return "iput-short-quick";
        case 0xEF: return "iget-boolean-quick";

            // 0xF0 - 0xFF
        case 0xF0: return "iget-byte-quick";
        case 0xF1: return "iget-char-quick";
        case 0xF2: return "iget-short-quick";
        case 0xF3: return "unused-f3";
        case 0xF4: return "unused-f4";
        case 0xF5: return "unused-f5";
        case 0xF6: return "unused-f6";
        case 0xF7: return "unused-f7";
        case 0xF8: return "unused-f8";
        case 0xF9: return "unused-f9";
        case 0xFA: return "invoke-polymorphic";
        case 0xFB: return "invoke-polymorphic/range";
        case 0xFC: return "invoke-custom";
        case 0xFD: return "invoke-custom/range";
        case 0xFE: return "const-method-handle";
        case 0xFF: return "const-method-type";

        default: break;
    }

    // 未知兜底（十六进制大写）
    std::ostringstream oss;
    oss << "unknown-opcode-0x" << std::hex << std::uppercase << static_cast<unsigned int>(op);
    return oss.str();
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

