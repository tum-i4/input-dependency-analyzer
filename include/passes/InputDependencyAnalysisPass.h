#pragma once

#include "analysis/InputDependencyAnalysisInterface.h"
#include "llvm/Pass.h"

#include <memory>

namespace input_dependency {

class InputDependencyAnalysisPass : public llvm::ModulePass
{
public:
    using InputDepAnalysisRes = std::shared_ptr<InputDependencyAnalysisInterface>;

public:
    static char ID;
    InputDependencyAnalysisPass()
        : llvm::ModulePass(ID)
    {
    }

    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;

public:
    const InputDepAnalysisRes& getInputDepAnalysisRes() const
    {
        return m_inputDepAnalysisRes;
    }

private:
    void dump_statistics(llvm::Module* M);

private:
    InputDepAnalysisRes m_inputDepAnalysisRes;
}; // class InputDependencyAnalysisPass

} // namespace input_dependency

