#pragma once

#include "DependencyAnaliser.h"
#include "InputDependencyResult.h"

#include "llvm/Pass.h"

#include <memory>
#include <unordered_map>

namespace llvm {
class CallGraph;
class Function;
class Instruction;
class BasicBlock;
class Module;
}

namespace input_dependency {

class InputDependencyAnalysis : public llvm::ModulePass
{
public:
    using InputDepResType = std::shared_ptr<InputDependencyResult>;
    using InputDependencyAnalysisInfo = std::unordered_map<llvm::Function*, InputDepResType>;

public:
    static char ID;

    InputDependencyAnalysis()
        : llvm::ModulePass(ID)
    {
    }

public:
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;

private:
    bool doFinalization(llvm::CallGraph &CG);

public:
    bool isInputDependent(llvm::Function* F, llvm::Instruction* instr) const;
    bool isInputDependent(llvm::Instruction* instr) const;
    bool isInputDependent(llvm::BasicBlock* block) const;

    const InputDependencyAnalysisInfo& getAnalysisInfo() const
    {
        return m_functionAnalisers;
    }

    InputDependencyAnalysisInfo& getAnalysisInfo()
    {
        return m_functionAnalisers;
    }

    InputDepResType getAnalysisInfo(llvm::Function* F);
    const InputDepResType getAnalysisInfo(llvm::Function* F) const;

    bool insertAnalysisInfo(llvm::Function* F, InputDepResType analysis_info);

private:
    void finalizeForArguments(llvm::Function* F, InputDepResType& FA);
    void finalizeForGlobals(llvm::Function* F, InputDepResType& FA);
    using FunctionArgumentsDependencies = std::unordered_map<llvm::Function*, DependencyAnaliser::ArgumentDependenciesMap>;
    void mergeCallSitesData(llvm::Function* caller, const FunctionSet& calledFunctions);
    DependencyAnaliser::ArgumentDependenciesMap getFunctionCallInfo(llvm::Function* F);
    DependencyAnaliser::GlobalVariableDependencyMap getFunctionCallGlobalsInfo(llvm::Function* F);

    template <class DependencyMapType>
    void mergeDependencyMaps(DependencyMapType& mergeTo, const DependencyMapType& mergeFrom);
    void addMissingGlobalsInfo(llvm::Function* F, DependencyAnaliser::GlobalVariableDependencyMap& globalDeps);

private:
    // keep these because function analysis is done with two phases, and need to preserve data
    llvm::Module* m_module;
    InputDependencyAnalysisInfo m_functionAnalisers;
    FunctionArgumentsDependencies m_functionsCallInfo;
    CalleeCallersMap m_calleeCallersInfo;
    std::vector<llvm::Function*> m_moduleFunctions;
};

}

