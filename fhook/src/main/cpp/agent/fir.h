//
// Created by Administrator on 2025/8/28.
//

#ifndef RIFT_FIR_H
#define RIFT_FIR_H

#include "../dexter/slicer/instrumentation.h"

namespace fir {
    class FIR {
    public:
        static lir::Method *cre_method(lir::CodeIr *code_ir,
                                       const std::string &name_class,
                                       const std::string &name_method,
                                       const std::vector<std::string> &param_types,
                                       const std::string &return_type);
    };
}


#endif //RIFT_FIR_H
