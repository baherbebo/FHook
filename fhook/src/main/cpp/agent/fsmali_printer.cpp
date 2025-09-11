//
// Created by Administrator on 2025/8/26.
//

#include "fsmali_printer.h"
// 在合适的头文件中声明（如 smali_printer.h）
#include "../FGlobal_flib.h"

#define TAG FEADRE_TAG("smali_printer")

#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <type_traits>
#include <cstring>   // std::memcpy

// ==========================================================
// 常量显示风格配置 —— 已统一为十进制
// ==========================================================
enum class LiteralRadix {
    Dec, Hex
};
static constexpr LiteralRadix kLiteralRadix = LiteralRadix::Dec;

// ==========================================================
// 轻量 SFINAE 探测（用于兼容不同 LIR 字段命名）
// ==========================================================
template<class T, class = void>
struct has_u8_value : std::false_type {
};
template<class T>
struct has_u8_value<T, std::void_t<decltype(std::declval<const T>().u.u8_value)>> : std::true_type {
};

template<class T, class = void>
struct has_s8_value : std::false_type {
};
template<class T>
struct has_s8_value<T, std::void_t<decltype(std::declval<const T>().u.s8_value)>> : std::true_type {
};

template<class T, class = void>
struct has_double_value : std::false_type {
};
template<class T>
struct has_double_value<T, std::void_t<decltype(std::declval<const T>().u.double_value)>>
        : std::true_type {
};

template<class T, class = void>
struct has_u4_value : std::false_type {
};
template<class T>
struct has_u4_value<T, std::void_t<decltype(std::declval<const T>().u.u4_value)>> : std::true_type {
};

template<class T, class = void>
struct has_s4_value : std::false_type {
};
template<class T>
struct has_s4_value<T, std::void_t<decltype(std::declval<const T>().u.s4_value)>> : std::true_type {
};

template<class T, class = void>
struct has_float_value : std::false_type {
};
template<class T>
struct has_float_value<T, std::void_t<decltype(std::declval<const T>().u.float_value)>>
        : std::true_type {
};

// ==========================================================
// 将数值格式化为 smali 风格字符串
// 规则：十进制 => 有符号视图（带负号）；十六进制 => 无符号 + 0x
// ==========================================================
static std::string FormatLiteralCore(uint64_t uval, int64_t sval, bool is64) {
    std::ostringstream os;
    if (kLiteralRadix == LiteralRadix::Hex) {
        os << "#0x" << std::uppercase << std::hex << uval;
    } else {
        os << "#" << sval;
    }
    return os.str();
}

// ==========================================================
// 32 位常量读取（严格贴合你给的 Const32 定义）：
// union { s4_value, u4_value, float_value }
// ==========================================================
static inline std::string FormatLiteral(const lir::Const32 *c) {
    // 优先十进制有符号
    int64_t sval = 0;
    uint64_t uval = 0;

    if constexpr (has_s4_value<lir::Const32>::value) {
        sval = static_cast<int64_t>(c->u.s4_value);
    } else if constexpr (has_u4_value<lir::Const32>::value) {
        // 没有 s4 就退化为 u4 再转有符号
        sval = static_cast<int64_t>(static_cast<int32_t>(c->u.u4_value));
    } else if constexpr (has_float_value<lir::Const32>::value) {
        // 极端兜底：从 float 原位取 32bit
        uint32_t bits = 0;
        std::memcpy(&bits, &c->u.float_value, sizeof(bits));
        sval = static_cast<int64_t>(static_cast<int32_t>(bits));
    } else {
        // 最后兜底：按 0 处理
        sval = 0;
    }

    if constexpr (has_u4_value<lir::Const32>::value) {
        uval = static_cast<uint64_t>(c->u.u4_value);
    } else if constexpr (has_s4_value<lir::Const32>::value) {
        uval = static_cast<uint64_t>(static_cast<uint32_t>(c->u.s4_value));
    } else if constexpr (has_float_value<lir::Const32>::value) {
        uint32_t bits = 0;
        std::memcpy(&bits, &c->u.float_value, sizeof(bits));
        uval = static_cast<uint64_t>(bits);
    } else {
        uval = 0;
    }

    return FormatLiteralCore(uval, sval, /*is64=*/false);
}

// ==========================================================
// 64 位常量读取（兼容不同 LIR 命名）：
// 常见 union { s8_value, u8_value, double_value }
// 若字段名不匹配，则从已有 8 字节成员按位读取
// ==========================================================
static inline std::string FormatLiteral(const lir::Const64 *c) {
    int64_t sval = 0;
    uint64_t uval = 0;

    if constexpr (has_s8_value<lir::Const64>::value) {
        sval = static_cast<int64_t>(c->u.s8_value);
    } else if constexpr (has_u8_value<lir::Const64>::value) {
        sval = static_cast<int64_t>(c->u.u8_value); // reinterpret as signed
    } else if constexpr (has_double_value<lir::Const64>::value) {
        static_assert(sizeof(double) == sizeof(uint64_t), "double size unexpected");
        uint64_t bits = 0;
        std::memcpy(&bits, &c->u.double_value, sizeof(bits));
        sval = static_cast<int64_t>(bits);
    } else {
        // 如果你的 LIR 用了别的 8 字段名，这里可以再加一个分支；
        // 退化兜底为 0，不影响编译。
        sval = 0;
    }

    if constexpr (has_u8_value<lir::Const64>::value) {
        uval = static_cast<uint64_t>(c->u.u8_value);
    } else if constexpr (has_s8_value<lir::Const64>::value) {
        uval = static_cast<uint64_t>(c->u.s8_value);
    } else if constexpr (has_double_value<lir::Const64>::value) {
        uint64_t bits = 0;
        std::memcpy(&bits, &c->u.double_value, sizeof(bits));
        uval = bits;
    } else {
        uval = 0;
    }

    return FormatLiteralCore(uval, sval, /*is64=*/true);
}

// ==========================================================
// 整个 CodeIr -> 文本
// ==========================================================
std::string SmaliPrinter::CodeIrToSmali(lir::CodeIr *code_ir) {
    std::stringstream ss;
    ss << "===== CodeIr 对应的 Smali 指令 =====" << std::endl;
    size_t index = 0;
    for (auto it = code_ir->instructions.begin();
         it != code_ir->instructions.end(); ++it, ++index) {
        lir::Instruction *instr = *it;
        if (auto bytecode = dynamic_cast<lir::Bytecode *>(instr)) {
            ss << "[" << index << "] " << BytecodeToSmali(bytecode) << std::endl;
        } else if (auto label = dynamic_cast<lir::Label *>(instr)) {
            ss << ":label_" << label->offset << "  # 标签偏移量: " << label->offset
               << std::endl;
        } else if (auto dbg_anno = dynamic_cast<lir::DbgInfoAnnotation *>(instr)) {
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
        lir::Instruction *instr = *it;
        if (auto bytecode = dynamic_cast<lir::Bytecode *>(instr)) {
            LOGD("  [%zu] %s", index, BytecodeToSmali(bytecode).c_str());
        } else if (auto label = dynamic_cast<lir::Label *>(instr)) {
            LOGD("  :label_%d  # 标签偏移量: %d", label->offset, label->offset);
        } else if (auto dbg_anno = dynamic_cast<lir::DbgInfoAnnotation *>(instr)) {
            LOGD("  # 调试信息: %s", DbgAnnotationToSmali(dbg_anno).c_str());
        }
    }
    LOGD("===================================");
}

void SmaliPrinter::CodeIrToSmali4Print(lir::CodeIr *code_ir) {
    std::string text = "";
    CodeIrToSmali4Print(code_ir, text);
}

// ==========================================================
// 单条 Bytecode -> 文本（带 invoke-* 的特化）
// ==========================================================
std::string SmaliPrinter::BytecodeToSmali(lir::Bytecode *bytecode) {
    std::string smali;
    // 1) 指令名
    std::string opcodeName = OpcodeToName(bytecode->opcode);
    smali += opcodeName + " ";

    // 2) invoke-*：第一个操作数是寄存器集
    bool isInvokeOpcode = (opcodeName.find("invoke-") == 0);
    if (isInvokeOpcode && !bytecode->operands.empty()) {
        auto paramOperand = bytecode->operands[0];
        smali += "{ ";
        if (auto vreg_range = dynamic_cast<lir::VRegRange *>(paramOperand)) {
            smali += "v" + std::to_string(vreg_range->base_reg) + " .. v" +
                     std::to_string(vreg_range->base_reg + vreg_range->count - 1);
        } else if (auto vreg_list = dynamic_cast<lir::VRegList *>(paramOperand)) {
            for (size_t i = 0; i < vreg_list->registers.size(); ++i) {
                if (i > 0) smali += ", ";
                smali += "v" + std::to_string(vreg_list->registers[i]);
            }
        } else if (auto vreg = dynamic_cast<lir::VReg *>(paramOperand)) {
            smali += "v" + std::to_string(vreg->reg);
        }
        smali += " } ";

        // 第二个操作数为方法引用
        if (bytecode->operands.size() > 1) {
            if (auto method = dynamic_cast<lir::Method *>(bytecode->operands[1])) {
                smali += MethodToSmali(method->ir_method) + " ";
            }
        }
    } else {
        // 3) 其它操作数逐个打印
        for (auto &operand: bytecode->operands) {
            if (auto vreg_range = dynamic_cast<lir::VRegRange *>(operand)) {
                smali += "{v" + std::to_string(vreg_range->base_reg) + " .. v" +
                         std::to_string(vreg_range->base_reg + vreg_range->count - 1) + "} ";
            } else if (auto vreg_list = dynamic_cast<lir::VRegList *>(operand)) {
                smali += "{ ";
                for (size_t i = 0; i < vreg_list->registers.size(); ++i) {
                    if (i > 0) smali += ", ";
                    smali += "v" + std::to_string(vreg_list->registers[i]);
                }
                smali += " } ";
            } else if (auto vreg = dynamic_cast<lir::VReg *>(operand)) {
                smali += "v" + std::to_string(vreg->reg) + " ";
            } else if (auto vreg_pair = dynamic_cast<lir::VRegPair *>(operand)) {
                smali += "v" + std::to_string(vreg_pair->base_reg) + "/v" +
                         std::to_string(vreg_pair->base_reg + 1) + " ";
            } else if (auto method = dynamic_cast<lir::Method *>(operand)) {
                smali += MethodToSmali(method->ir_method) + " ";
            } else if (auto type = dynamic_cast<lir::Type *>(operand)) {
                smali += type->ir_type->descriptor->c_str() + std::string(" ");
            } else if (auto cst32 = dynamic_cast<lir::Const32 *>(operand)) {
                smali += FormatLiteral(cst32) + " ";
            } else if (auto cst64 = dynamic_cast<lir::Const64 *>(operand)) {
                smali += FormatLiteral(cst64) + " ";
            } else if (auto code_loc = dynamic_cast<lir::CodeLocation *>(operand)) {
                smali += ":label_" + std::to_string(code_loc->label->offset) + " ";
            } else if (auto str = dynamic_cast<lir::String *>(operand)) {
                smali += "\"" + std::string(str->ir_string->c_str()) + "\" ";
            }
        }
    }

    if (!smali.empty() && smali.back() == ' ') smali.pop_back();
    return smali;
}

// ==========================================================
// opcode -> 指令名
// ==========================================================
std::string SmaliPrinter::OpcodeToName(dex::Opcode opcode) {
    const uint8_t op = static_cast<uint8_t>(opcode);
    switch (op) {
        // 0x00 - 0x0F
        case 0x00:
            return "nop";
        case 0x01:
            return "move";
        case 0x02:
            return "move/from16";
        case 0x03:
            return "move/16";
        case 0x04:
            return "move-wide";
        case 0x05:
            return "move-wide/from16";
        case 0x06:
            return "move-wide/16";
        case 0x07:
            return "move-object";
        case 0x08:
            return "move-object/from16";
        case 0x09:
            return "move-object/16";
        case 0x0A:
            return "move-result";
        case 0x0B:
            return "move-result-wide";
        case 0x0C:
            return "move-result-object";
        case 0x0D:
            return "move-exception";
        case 0x0E:
            return "return-void";
        case 0x0F:
            return "return";

            // 0x10 - 0x1F
        case 0x10:
            return "return-wide";
        case 0x11:
            return "return-object";
        case 0x12:
            return "const/4";
        case 0x13:
            return "const/16";
        case 0x14:
            return "const";
        case 0x15:
            return "const/high16";
        case 0x16:
            return "const-wide/16";
        case 0x17:
            return "const-wide/32";
        case 0x18:
            return "const-wide";
        case 0x19:
            return "const-wide/high16";
        case 0x1A:
            return "const-string";
        case 0x1B:
            return "const-string/jumbo";
        case 0x1C:
            return "const-class";
        case 0x1D:
            return "monitor-enter";
        case 0x1E:
            return "monitor-exit";
        case 0x1F:
            return "check-cast";

            // 0x20 - 0x2F
        case 0x20:
            return "instance-of";
        case 0x21:
            return "array-length";
        case 0x22:
            return "new-instance";
        case 0x23:
            return "new-array";
        case 0x24:
            return "filled-new-array";
        case 0x25:
            return "filled-new-array/range";
        case 0x26:
            return "fill-array-data";
        case 0x27:
            return "throw";
        case 0x28:
            return "goto";
        case 0x29:
            return "goto/16";
        case 0x2A:
            return "goto/32";
        case 0x2B:
            return "packed-switch";
        case 0x2C:
            return "sparse-switch";
        case 0x2D:
            return "cmpl-float";
        case 0x2E:
            return "cmpg-float";
        case 0x2F:
            return "cmpl-double";

            // 0x30 - 0x3F
        case 0x30:
            return "cmpg-double";
        case 0x31:
            return "cmp-long";
        case 0x32:
            return "if-eq";
        case 0x33:
            return "if-ne";
        case 0x34:
            return "if-lt";
        case 0x35:
            return "if-ge";
        case 0x36:
            return "if-gt";
        case 0x37:
            return "if-le";
        case 0x38:
            return "if-eqz";
        case 0x39:
            return "if-nez";
        case 0x3A:
            return "if-ltz";
        case 0x3B:
            return "if-gez";
        case 0x3C:
            return "if-gtz";
        case 0x3D:
            return "if-lez";
        case 0x3E:
            return "unused-3e";
        case 0x3F:
            return "unused-3f";

            // 0x40 - 0x4F
        case 0x40:
            return "unused-40";
        case 0x41:
            return "unused-41";
        case 0x42:
            return "unused-42";
        case 0x43:
            return "unused-43";
        case 0x44:
            return "aget";
        case 0x45:
            return "aget-wide";
        case 0x46:
            return "aget-object";
        case 0x47:
            return "aget-boolean";
        case 0x48:
            return "aget-byte";
        case 0x49:
            return "aget-char";
        case 0x4A:
            return "aget-short";
        case 0x4B:
            return "aput";
        case 0x4C:
            return "aput-wide";
        case 0x4D:
            return "aput-object";
        case 0x4E:
            return "aput-boolean";
        case 0x4F:
            return "aput-byte";

            // 0x50 - 0x5F
        case 0x50:
            return "aput-char";
        case 0x51:
            return "aput-short";
        case 0x52:
            return "iget";
        case 0x53:
            return "iget-wide";
        case 0x54:
            return "iget-object";
        case 0x55:
            return "iget-boolean";
        case 0x56:
            return "iget-byte";
        case 0x57:
            return "iget-char";
        case 0x58:
            return "iget-short";
        case 0x59:
            return "iput";
        case 0x5A:
            return "iput-wide";
        case 0x5B:
            return "iput-object";
        case 0x5C:
            return "iput-boolean";
        case 0x5D:
            return "iput-byte";
        case 0x5E:
            return "iput-char";
        case 0x5F:
            return "iput-short";

            // 0x60 - 0x6F
        case 0x60:
            return "sget";
        case 0x61:
            return "sget-wide";
        case 0x62:
            return "sget-object";
        case 0x63:
            return "sget-boolean";
        case 0x64:
            return "sget-byte";
        case 0x65:
            return "sget-char";
        case 0x66:
            return "sget-short";
        case 0x67:
            return "sput";
        case 0x68:
            return "sput-wide";
        case 0x69:
            return "sput-object";
        case 0x6A:
            return "sput-boolean";
        case 0x6B:
            return "sput-byte";
        case 0x6C:
            return "sput-char";
        case 0x6D:
            return "sput-short";
        case 0x6E:
            return "invoke-virtual";
        case 0x6F:
            return "invoke-super";

            // 0x70 - 0x7F
        case 0x70:
            return "invoke-direct";
        case 0x71:
            return "invoke-static";
        case 0x72:
            return "invoke-interface";
        case 0x73:
            return "return-void-no-barrier";
        case 0x74:
            return "invoke-virtual/range";
        case 0x75:
            return "invoke-super/range";
        case 0x76:
            return "invoke-direct/range";
        case 0x77:
            return "invoke-static/range";
        case 0x78:
            return "invoke-interface/range";
        case 0x79:
            return "unused-79";
        case 0x7A:
            return "unused-7a";
        case 0x7B:
            return "neg-int";
        case 0x7C:
            return "not-int";
        case 0x7D:
            return "neg-long";
        case 0x7E:
            return "not-long";
        case 0x7F:
            return "neg-float";

            // 0x80 - 0x8F
        case 0x80:
            return "neg-double";
        case 0x81:
            return "int-to-long";
        case 0x82:
            return "int-to-float";
        case 0x83:
            return "int-to-double";
        case 0x84:
            return "long-to-int";
        case 0x85:
            return "long-to-float";
        case 0x86:
            return "long-to-double";
        case 0x87:
            return "float-to-int";
        case 0x88:
            return "float-to-long";
        case 0x89:
            return "float-to-double";
        case 0x8A:
            return "double-to-int";
        case 0x8B:
            return "double-to-long";
        case 0x8C:
            return "double-to-float";
        case 0x8D:
            return "int-to-byte";
        case 0x8E:
            return "int-to-char";
        case 0x8F:
            return "int-to-short";

            // 0x90 - 0x9F
        case 0x90:
            return "add-int";
        case 0x91:
            return "sub-int";
        case 0x92:
            return "mul-int";
        case 0x93:
            return "div-int";
        case 0x94:
            return "rem-int";
        case 0x95:
            return "and-int";
        case 0x96:
            return "or-int";
        case 0x97:
            return "xor-int";
        case 0x98:
            return "shl-int";
        case 0x99:
            return "shr-int";
        case 0x9A:
            return "ushr-int";
        case 0x9B:
            return "add-long";
        case 0x9C:
            return "sub-long";
        case 0x9D:
            return "mul-long";
        case 0x9E:
            return "div-long";
        case 0x9F:
            return "rem-long";

            // 0xA0 - 0xAF
        case 0xA0:
            return "and-long";
        case 0xA1:
            return "or-long";
        case 0xA2:
            return "xor-long";
        case 0xA3:
            return "shl-long";
        case 0xA4:
            return "shr-long";
        case 0xA5:
            return "ushr-long";
        case 0xA6:
            return "add-float";
        case 0xA7:
            return "sub-float";
        case 0xA8:
            return "mul-float";
        case 0xA9:
            return "div-float";
        case 0xAA:
            return "rem-float";
        case 0xAB:
            return "add-double";
        case 0xAC:
            return "sub-double";
        case 0xAD:
            return "mul-double";
        case 0xAE:
            return "div-double";
        case 0xAF:
            return "rem-double";

            // 0xB0 - 0xBF
        case 0xB0:
            return "add-int/2addr";
        case 0xB1:
            return "sub-int/2addr";
        case 0xB2:
            return "mul-int/2addr";
        case 0xB3:
            return "div-int/2addr";
        case 0xB4:
            return "rem-int/2addr";
        case 0xB5:
            return "and-int/2addr";
        case 0xB6:
            return "or-int/2addr";
        case 0xB7:
            return "xor-int/2addr";
        case 0xB8:
            return "shl-int/2addr";
        case 0xB9:
            return "shr-int/2addr";
        case 0xBA:
            return "ushr-int/2addr";
        case 0xBB:
            return "add-long/2addr";
        case 0xBC:
            return "sub-long/2addr";
        case 0xBD:
            return "mul-long/2addr";
        case 0xBE:
            return "div-long/2addr";
        case 0xBF:
            return "rem-long/2addr";

            // 0xC0 - 0xCF
        case 0xC0:
            return "and-long/2addr";
        case 0xC1:
            return "or-long/2addr";
        case 0xC2:
            return "xor-long/2addr";
        case 0xC3:
            return "shl-long/2addr";
        case 0xC4:
            return "shr-long/2addr";
        case 0xC5:
            return "ushr-long/2addr";
        case 0xC6:
            return "add-float/2addr";
        case 0xC7:
            return "sub-float/2addr";
        case 0xC8:
            return "mul-float/2addr";
        case 0xC9:
            return "div-float/2addr";
        case 0xCA:
            return "rem-float/2addr";
        case 0xCB:
            return "add-double/2addr";
        case 0xCC:
            return "sub-double/2addr";
        case 0xCD:
            return "mul-double/2addr";
        case 0xCE:
            return "div-double/2addr";
        case 0xCF:
            return "rem-double/2addr";

            // 0xD0 - 0xDF
        case 0xD0:
            return "add-int/lit16";
        case 0xD1:
            return "rsub-int";
        case 0xD2:
            return "mul-int/lit16";
        case 0xD3:
            return "div-int/lit16";
        case 0xD4:
            return "rem-int/lit16";
        case 0xD5:
            return "and-int/lit16";
        case 0xD6:
            return "or-int/lit16";
        case 0xD7:
            return "xor-int/lit16";
        case 0xD8:
            return "add-int/lit8";
        case 0xD9:
            return "rsub-int/lit8";
        case 0xDA:
            return "mul-int/lit8";
        case 0xDB:
            return "div-int/lit8";
        case 0xDC:
            return "rem-int/lit8";
        case 0xDD:
            return "and-int/lit8";
        case 0xDE:
            return "or-int/lit8";
        case 0xDF:
            return "xor-int/lit8";

            // 0xE0 - 0xEF
        case 0xE0:
            return "shl-int/lit8";
        case 0xE1:
            return "shr-int/lit8";
        case 0xE2:
            return "ushr-int/lit8";
        case 0xE3:
            return "iget-quick";
        case 0xE4:
            return "iget-wide-quick";
        case 0xE5:
            return "iget-object-quick";
        case 0xE6:
            return "iput-quick";
        case 0xE7:
            return "iput-wide-quick";
        case 0xE8:
            return "iput-object-quick";
        case 0xE9:
            return "invoke-virtual-quick";
        case 0xEA:
            return "invoke-virtual/range-quick";
        case 0xEB:
            return "iput-boolean-quick";
        case 0xEC:
            return "iput-byte-quick";
        case 0xED:
            return "iput-char-quick";
        case 0xEE:
            return "iput-short-quick";
        case 0xEF:
            return "iget-boolean-quick";

            // 0xF0 - 0xFF
        case 0xF0:
            return "iget-byte-quick";
        case 0xF1:
            return "iget-char-quick";
        case 0xF2:
            return "iget-short-quick";
        case 0xF3:
            return "unused-f3";
        case 0xF4:
            return "unused-f4";
        case 0xF5:
            return "unused-f5";
        case 0xF6:
            return "unused-f6";
        case 0xF7:
            return "unused-f7";
        case 0xF8:
            return "unused-f8";
        case 0xF9:
            return "unused-f9";
        case 0xFA:
            return "invoke-polymorphic";
        case 0xFB:
            return "invoke-polymorphic/range";
        case 0xFC:
            return "invoke-custom";
        case 0xFD:
            return "invoke-custom/range";
        case 0xFE:
            return "const-method-handle";
        case 0xFF:
            return "const-method-type";
        default:
            break;
    }
    std::ostringstream oss;
    oss << "unknown-opcode-0x" << std::hex << std::uppercase << static_cast<unsigned int>(op);
    return oss.str();
}

// ==========================================================
std::string SmaliPrinter::MethodToSmali(ir::MethodDecl *method_decl) {
    if (!method_decl || !method_decl->parent || !method_decl->prototype) {
        return "unknown_method";
    }
    std::string smali;
    smali += method_decl->parent->descriptor->c_str();
    smali += "->";
    smali += method_decl->name->c_str();
    smali += "(";
    auto param_types = method_decl->prototype->param_types;
    if (param_types && !param_types->types.empty()) {
        for (auto param_type: param_types->types) {
            smali += param_type->descriptor->c_str();
        }
    }
    smali += ")";
    smali += method_decl->prototype->return_type->descriptor->c_str();
    return smali;
}

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

// ==========================================================
// Visitor 版本（可与 BytecodeToSmali 并存）
// ==========================================================
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

std::string SmaliPrinter::ToSmali(lir::Bytecode *bc) {
    if (bc == nullptr) return "null";

    std::ostringstream ss;
    ss << OpcodeToName(bc->opcode);

    for (size_t i = 0; i < bc->operands.size(); ++i) {
        lir::Operand *op = bc->operands[i];
        if (!op) {
            ss << " null";
            continue;
        }

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
                ss << " v" << rp->base_reg << "/v" << (rp->base_reg + 1);
                break;
            }
            case OperandTypeDetector::KVRegList: {
                auto *rl = static_cast<lir::VRegList *>(op);
                ss << " {";
                for (size_t k = 0; k < rl->registers.size(); ++k) {
                    if (k > 0) ss << ",";
                    ss << "v" << rl->registers[k];
                }
                ss << "}";
                break;
            }
            case OperandTypeDetector::KVRegRange: {
                auto *rr = static_cast<lir::VRegRange *>(op);
                ss << " {v" << rr->base_reg << " .. v" << (rr->base_reg + 1ULL * rr->count - 1)
                   << "}";
                break;
            }
            case OperandTypeDetector::KConst32: {
                auto *c = static_cast<lir::Const32 *>(op);
                ss << " " << FormatLiteral(c);
                break;
            }
            case OperandTypeDetector::KConst64: {
                auto *c = static_cast<lir::Const64 *>(op);
                ss << " " << FormatLiteral(c);
                break;
            }
            case OperandTypeDetector::KString: {
                auto *s = static_cast<lir::String *>(op);
                if (s->ir_string) ss << " \"" << s->ir_string->c_str() << "\"";
                else ss << " string@" << s->index;
                break;
            }
            case OperandTypeDetector::KType: {
                auto *t = static_cast<lir::Type *>(op);
                if (t->ir_type) ss << " " << t->ir_type->descriptor->c_str();
                else ss << " type@" << t->index;
                break;
            }
            case OperandTypeDetector::KField: {
                auto *f = static_cast<lir::Field *>(op);
                if (f->ir_field) {
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
                if (m->ir_method) ss << " " << MethodToSmali(m->ir_method);
                else ss << " method@" << m->index;
                break;
            }
            case OperandTypeDetector::KCodeLocation: {
                auto *cl = static_cast<lir::CodeLocation *>(op);
                if (cl->label) ss << " :label_" << cl->label->offset;
                else ss << " :label?";
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
        }
    }

    return ss.str();
}
