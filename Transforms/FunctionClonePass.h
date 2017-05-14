#pragma once

#include "FunctionClone.h"
#include "Analysis/InputDependencyAnalysis.h"

#include "llvm/Pass.h"

namespace input_dependency {
class InputDependencyAnalysis;
class FunctionAnaliser;
}

namespace llvm {
class Function;
class Instruction;
class Module;
}

namespace oh {

class FunctionClonePass : public llvm::ModulePass
{
public:
    static char ID;

    FunctionClonePass()
        : llvm::ModulePass(ID)
    {
    }

public:
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;

private:
    std::unordered_set<llvm::Function*> doClone(const input_dependency::FunctionAnaliser* analiser, llvm::Function* calledF);
    void changeFunctionCall(const llvm::Instruction* instr, llvm::Function* F);
    void cloneFunctionAnalysisInfo(const input_dependency::FunctionAnaliser* analiser,
                                   llvm::Function* Fclone,
                                   const input_dependency::FunctionCallDepInfo::ArgumentDependenciesMap& argumentDeps);

private:
    input_dependency::InputDependencyAnalysis* IDA;
    using FunctionCloneInfo = std::unordered_map<llvm::Function*, FunctionClone>;
    FunctionCloneInfo m_functionCloneInfo;
    input_dependency::InputDependencyAnalysis::InputDependencyAnalysisInfo m_duplicatedAnalysisInfo;
};

}

