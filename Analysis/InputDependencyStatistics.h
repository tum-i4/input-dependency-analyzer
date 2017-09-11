#pragma once

#include "InputDependencyAnalysis.h"

#include "llvm/Pass.h"

namespace llvm {
class Module;
}


namespace input_dependency {

class InputDependencyStatistics
{
public:
    InputDependencyStatistics() = default;

public:
    void report(llvm::Module& M, const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo) const;
};

/// Collects and calculates statistics on input dependent instructions
class InputDependencyStatisticsPass : public llvm::ModulePass
{
public:
    static char ID;

    InputDependencyStatisticsPass()
        : llvm::ModulePass(ID)
    {
    }

public:
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;
};

} // namespace input_dependency

