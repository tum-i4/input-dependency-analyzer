#pragma once

#include "llvm/Pass.h"

namespace input_dependency {

class TransparentCachingPass : public llvm::ModulePass
{
public:
    static char ID;

    TransparentCachingPass()
        : llvm::ModulePass(ID)
    {
    }

public:
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;
};

}

