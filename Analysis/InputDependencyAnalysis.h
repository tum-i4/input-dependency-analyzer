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
class LoopInfo;
class AAResults;
class PostDominatorTree;
class DominatorTree;
}

namespace input_dependency {

class VirtualCallSiteAnalysisResult;
class IndirectCallSitesAnalysisResult;

class InputDependencyAnalysis
{
public:
    using InputDepResType = std::shared_ptr<InputDependencyResult>;
    using InputDependencyAnalysisInfo = std::unordered_map<llvm::Function*, InputDepResType>;

    using LoopInfoGetter = std::function<llvm::LoopInfo* (llvm::Function* F)>;
    using AliasAnalysisInfoGetter = std::function<llvm::AAResults* (llvm::Function* F)>;
    using PostDominatorTreeGetter = std::function<const llvm::PostDominatorTree* (llvm::Function* F)>;
    using DominatorTreeGetter = std::function<const llvm::DominatorTree* (llvm::Function* F)>;

public:
    InputDependencyAnalysis(llvm::Module* M);

    void setCallGraph(llvm::CallGraph& callGraph);
    void setVirtualCallSiteAnalysisResult(const VirtualCallSiteAnalysisResult* virtualCallSiteAnalysisRes);
    void setIndirectCallSiteAnalysisResult(const IndirectCallSitesAnalysisResult* indirectCallSiteAnalysisRes);
    void setAliasAnalysisInfoGetter(const AliasAnalysisInfoGetter& aliasAnalysisInfoGetter);
    void setLoopInfoGetter(const LoopInfoGetter& loopInfoGetter);
    void setPostDominatorTreeGetter(const PostDominatorTreeGetter& postDomTreeGetter);
    void setDominatorTreeGetter(const DominatorTreeGetter& domTreeGetter);

    void run();

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
    void runOnFunction(llvm::Function* F);
    void doFinalization();
    void dump_statistics();
    void cache_input_dependency();

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
    llvm::Module* m_module;
    FunctionAnalysisGetter m_functionAnalysisGetter;
    llvm::CallGraph* m_callGraph;
    const VirtualCallSiteAnalysisResult* m_virtualCallSiteAnalysisRes;
    const IndirectCallSitesAnalysisResult* m_indirectCallSiteAnalysisRes;
    LoopInfoGetter m_loopInfoGetter;
    AliasAnalysisInfoGetter m_aliasAnalysisInfoGetter;
    PostDominatorTreeGetter m_postDomTreeGetter;
    DominatorTreeGetter m_domTreeGetter;
    // keep these because function analysis is done with two phases, and need to preserve data
    InputDependencyAnalysisInfo m_functionAnalisers;
    FunctionArgumentsDependencies m_functionsCallInfo;
    CalleeCallersMap m_calleeCallersInfo;
    std::vector<llvm::Function*> m_moduleFunctions;
}; // class InputDependencyAnalysis

class InputDependencyAnalysisPass : public llvm::ModulePass
{
public:
    using InputDependencyAnalysisType = std::shared_ptr<InputDependencyAnalysis>;

public:
    static char ID;

    InputDependencyAnalysisPass()
        : llvm::ModulePass(ID)
    {
    }

public:
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;

public:
    InputDependencyAnalysisType getInputDependencyAnalysis()
    {
        return m_analysis;
    }

    const InputDependencyAnalysisType& getInputDependencyAnalysis() const
    {
        return m_analysis;
    }
    
private:
    InputDependencyAnalysisType m_analysis;
};

}

