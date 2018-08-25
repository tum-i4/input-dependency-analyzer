#pragma once

#include "input-dependency/Analysis/InputDependencyAnalysisInterface.h"

namespace llvm {
class Function;
class Instruction;
class BasicBlock;
class Module;
}

namespace input_dependency {

class CachedInputDependencyAnalysis final : public InputDependencyAnalysisInterface
{
public:
    CachedInputDependencyAnalysis(llvm::Module* M);

public:
    void run() override;

    bool isInputDependent(llvm::Function* F, llvm::Instruction* instr) const override;
    bool isInputDependent(llvm::Instruction* instr) const override;
    bool isInputDependent(llvm::BasicBlock* block) const override;
    bool isControlDependent(llvm::Instruction* I) const override;
    bool isDataDependent(llvm::Instruction* I) const override;

    const InputDependencyAnalysisInfo& getAnalysisInfo() const override
    {
        return m_functionAnalisers;
    }

    InputDependencyAnalysisInfo& getAnalysisInfo() override
    {
        return m_functionAnalisers;
    }

    InputDepResType getAnalysisInfo(llvm::Function* F) override;
    const InputDepResType getAnalysisInfo(llvm::Function* F) const override;

    bool insertAnalysisInfo(llvm::Function* F, InputDepResType analysis_info) override;

private:
    llvm::Module* m_module;
    InputDependencyAnalysisInfo m_functionAnalisers;
};

} // namespace input_dependency

