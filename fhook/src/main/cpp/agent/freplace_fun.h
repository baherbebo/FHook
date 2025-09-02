//
// Created by Administrator on 2025/8/27.
//

#ifndef RIFT_FREPLACE_FUN_H
#define RIFT_FREPLACE_FUN_H

#include <string>
#include "../dexter/slicer/instrumentation.h"

namespace freplace_fun {
    bool replace_fun4return(slicer::DetourHook *thiz, lir::CodeIr *code_ir);

    bool replace_fun4hook_void(slicer::DetourHook *thiz, lir::CodeIr *code_ir);

    bool replace_fun4standard(slicer::DetourHook *thiz, lir::CodeIr *code_ir);

    bool replace_fun4onEnter(slicer::DetourHook *thiz, lir::CodeIr *code_ir);

    bool replace_fun4onExit(slicer::DetourHook *thiz, lir::CodeIr *code_ir);
}


#endif //RIFT_FREPLACE_FUN_H
