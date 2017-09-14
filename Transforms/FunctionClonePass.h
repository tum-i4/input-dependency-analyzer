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
    using FunctionSet = std::unordered_set<llvm::Function*>;
    using InputDepRes = input_dependency::InputDependencyAnalysis::InputDepResType;
    FunctionSet doClone(const InputDepRes& analiser,
                        llvm::Function* calledF);
    InputDepRes getFunctionInputDepInfo(llvm::Function* F) const;
    std::pair<llvm::Function*, bool> doCloneForArguments(
                                            llvm::Function* calledF,
                                            InputDepRes original_analiser,
                                            FunctionClone& clone,
                                            const input_dependency::FunctionCallDepInfo::ArgumentDependenciesMap& argDeps);

    void dump() const;
    void dumpStatistics(llvm::Module& M);

private:
    input_dependency::InputDependencyAnalysis* IDA;
    using FunctionCloneInfo = std::unordered_map<llvm::Function*, FunctionClone>;
    FunctionCloneInfo m_functionCloneInfo;
    std::unordered_map<llvm::Function*, llvm::Function*> m_clone_to_original;
};

}

