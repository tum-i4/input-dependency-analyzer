#pragma once

#include "llvm/Pass.h"

namespace input_dependency {

class InputDependencyReachabilityAnalysisPass : public llvm::ModulePass
{
public:
    static char ID;

    InputDependencyReachabilityAnalysisPass()
        : llvm::ModulePass(ID)
    {
    }

    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;
}; // class ArgumentReachabilityAnalysisPass

} // namespace input_dependency

