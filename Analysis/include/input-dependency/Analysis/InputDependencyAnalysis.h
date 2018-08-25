#pragma once

#include "input-dependency/Analysis/InputDependencyAnalysisInterface.h"
#include "input-dependency/Analysis/DependencyAnaliser.h"

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

class InputDependencyAnalysis final : public InputDependencyAnalysisInterface
{
public:
    using LoopInfoGetter = std::function<llvm::LoopInfo* (llvm::Function* F)>;
    using PostDominatorTreeGetter = std::function<const llvm::PostDominatorTree* (llvm::Function* F)>;
    using DominatorTreeGetter = std::function<const llvm::DominatorTree* (llvm::Function* F)>;

public:
    InputDependencyAnalysis(llvm::Module* M);

    void setCallGraph(llvm::CallGraph* callGraph);
    void setVirtualCallSiteAnalysisResult(const VirtualCallSiteAnalysisResult* virtualCallSiteAnalysisRes);
    void setIndirectCallSiteAnalysisResult(const IndirectCallSitesAnalysisResult* indirectCallSiteAnalysisRes);
    void setAliasAnalysisInfoGetter(const AliasAnalysisInfoGetter& aliasAnalysisInfoGetter);
    void setLoopInfoGetter(const LoopInfoGetter& loopInfoGetter);
    void setPostDominatorTreeGetter(const PostDominatorTreeGetter& postDomTreeGetter);
    void setDominatorTreeGetter(const DominatorTreeGetter& domTreeGetter);

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
    void runOnFunction(llvm::Function* F);
    void doFinalization();

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
    std::unordered_set<llvm::Function*> m_processedInputDepFunctions;
}; // class InputDependencyAnalysis


} // namespace input_dependency

