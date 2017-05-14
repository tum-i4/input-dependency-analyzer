#pragma once

#include "FunctionAnaliser.h"

#include "llvm/Pass.h"

#include <unordered_map>

namespace llvm {
class CallGraph;
class Function;
class Instruction;
class Module;
}

namespace input_dependency {

class InputDependencyAnalysis : public llvm::ModulePass
{
public:
    using InputDependencyAnalysisInfo = std::unordered_map<llvm::Function*, FunctionAnaliser>;

public:
    static char ID;

    InputDependencyAnalysis()
        : llvm::ModulePass(ID)
    {
    }

public:
    bool runOnModule(llvm::Module& M) override;
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;

private:
    bool doFinalization(llvm::CallGraph &CG);

public:
    bool isInputDependent(llvm::Function* F, llvm::Instruction* instr) const;
    bool isInputDependent(llvm::Instruction* instr) const;

    const InputDependencyAnalysisInfo& getAnalysisInfo() const
    {
        return m_functionAnalisers;
    }

    InputDependencyAnalysisInfo& getAnalysisInfo()
    {
        return m_functionAnalisers;
    }

    FunctionAnaliser* getAnalysisInfo(llvm::Function* F);
    const FunctionAnaliser* getAnalysisInfo(llvm::Function* F) const;


private:
    void finalizeForArguments(llvm::Function* F, FunctionAnaliser& FA);
    void finalizeForGlobals(llvm::Function* F, FunctionAnaliser& FA);
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
    // TODO: try to find out how iterate trough cfg in reverse order
    std::vector<llvm::Function*> m_moduleFunctions;
};

}

