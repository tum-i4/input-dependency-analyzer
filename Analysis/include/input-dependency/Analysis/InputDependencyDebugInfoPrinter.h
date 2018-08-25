#pragma once

#include "llvm/Pass.h"

namespace llvm {
class Module;
}

namespace input_dependency {

class InputDependencyDebugInfoPrinterPass : public llvm::ModulePass
{
public:
    static char ID;

    InputDependencyDebugInfoPrinterPass()
        : llvm::ModulePass(ID)
    {
    }

public:
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;
};

}

