//
// Created by Administrator on 2025/9/2.
//

#ifndef FHOOK_FINJECT_H
#define FHOOK_FINJECT_H


#include "transform.h"
#include "../dexter/slicer/code_ir.h"

namespace finject {
    bool do_finject(deploy::Transform *transform,
                    const deploy::MethodHooks &hook_info,
                    lir::CodeIr *code_ir);

};


#endif //FHOOK_FINJECT_H
