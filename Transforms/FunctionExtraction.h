#pragma once

#include "llvm/Pass.h"

namespace oh {

class FunctionExtractionPass : public llvm::FunctionPass
{
public:
    static char ID;

    FunctionExtractionPass()
        : llvm::FunctionPass(ID)
    {
    }

public:
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnFunction(llvm::Function& F) override;
};

} // namespace oh

