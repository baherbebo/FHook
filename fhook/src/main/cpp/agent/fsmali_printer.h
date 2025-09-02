//
// Created by Administrator on 2025/8/26.
//

#ifndef RIFT_FSMALI_PRINTER_H
#define RIFT_FSMALI_PRINTER_H


//
// Created by Administrator on 2025/8/26.
//

#include "../dexter/slicer/code_ir.h"
#include "../dexter/slicer/dex_ir.h"


#include <string>
#include <sstream>

class SmaliPrinter {
public:
    // 转换整个 CodeIr 为 Smali 指令列表
    static std::string CodeIrToSmali(lir::CodeIr *code_ir);

    // 把单条 bytecode 转成 smali 风格
    static std::string ToSmali(lir::Bytecode *bc);

    static void CodeIrToSmali4Print(lir::CodeIr *code_ir, std::string &text);

    static void CodeIrToSmali4Print(lir::CodeIr *code_ir);

private:
    // 转换单条字节码指令为 Smali
    static std::string BytecodeToSmali(lir::Bytecode *bytecode);

    // 转换 opcode 为 Smali 指令名（补充常见指令）
    static std::string OpcodeToName(dex::Opcode opcode);

    // 转换方法为 Smali 格式
    static std::string MethodToSmali(ir::MethodDecl *method_decl);

    // 转换调试信息注解（简化版）
    static std::string DbgAnnotationToSmali(lir::DbgInfoAnnotation *dbg_anno);
};

#endif //RIFT_FSMALI_PRINTER_H
