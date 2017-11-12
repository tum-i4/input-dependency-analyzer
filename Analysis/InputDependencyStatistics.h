#pragma once

#include "InputDependencyAnalysis.h"

#include "llvm/Pass.h"

namespace llvm {
class Module;
}


namespace input_dependency {

class Statistics
{
public:
    virtual void report(llvm::Module& M,
                        const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo) const = 0;
};

class CoverageStatistics : public Statistics
{
public:
    CoverageStatistics() = default;

public:
    void report(llvm::Module& M,
                const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo) const override;
};

class InputDependencyStatistics : public Statistics
{
public:
    InputDependencyStatistics() = default;

public:
    void report(llvm::Module& M,
                const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo) const override;
};

/// Collects and calculates statistics on input dependent instructions
class InputDependencyStatisticsPass : public llvm::ModulePass
{
public:
    enum Type {
        INPUT_DEP,
        COVERAGE,
        UNKNOWN
    };

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

