#pragma once

#include "DependencyAnalysisResult.h"
#include "ReflectingDependencyAnaliser.h"

#include "llvm/ADT/SmallVector.h"

#include <memory>
#include <list>

namespace llvm {
class Loop;
class LoopInfo;
class PostDominatorTree;
}

namespace input_dependency {

class VirtualCallSiteAnalysisResult;
class IndirectCallSitesAnalysisResult;

class LoopAnalysisResult : public ReflectingDependencyAnaliser
{
public:
    LoopAnalysisResult(llvm::Function* F,
                       llvm::AAResults& AAR,
                       const llvm::PostDominatorTree& PDom,
                       const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                       const IndirectCallSitesAnalysisResult& indirectCallsInfo,
                       const Arguments& inputs,
                       const FunctionAnalysisGetter& Fgetter,
                       llvm::Loop& L,
                       llvm::LoopInfo& LI);

    LoopAnalysisResult(const LoopAnalysisResult&) = delete;
    LoopAnalysisResult(LoopAnalysisResult&& ) = delete;
    LoopAnalysisResult& operator =(const LoopAnalysisResult&) = delete;
    LoopAnalysisResult& operator =(LoopAnalysisResult&&) = delete;

    virtual ~LoopAnalysisResult() = default;


public:
    using ReflectingDependencyAnaliserT = std::unique_ptr<ReflectingDependencyAnaliser>;
    using BasicBlockDependencyAnalisersMap = std::unordered_map<llvm::BasicBlock*, ReflectingDependencyAnaliserT>;
    using SuccessorDeps = std::vector<DependencyAnaliser::ValueDependencies>;
    using BlocksVector = llvm::SmallVector<llvm::BasicBlock*, 10>;
    using FCallsArgDeps = DependencyAnaliser::FunctionCallsArgumentDependencies;

    /// \name Interface to start analysis
    /// \{
public:
    void gatherResults() override;
    void finalizeResults(const DependencyAnaliser::ArgumentDependenciesMap& dependentArgs) override;
    void finalizeGlobals(const DependencyAnaliser::GlobalVariableDependencyMap& globalsDeps) override;
    void dumpResults() const override;

    /// \}

    /// \name Abstract interface for getting analysis results
    /// \{
public:
    void setLoopDependencies(const DepInfo& loopDeps);
    void setInitialValueDependencies(const DependencyAnaliser::ValueDependencies& valueDependencies) override;
    void setOutArguments(const DependencyAnaliser::ArgumentDependenciesMap& outArgs) override;
    // make sure call this after finalization
    bool isInputDependent(llvm::BasicBlock* block) const override;
    bool isInputDependent(llvm::Instruction* instr) const override;
    bool isInputIndependent(llvm::Instruction* instr) const override;
    bool hasValueDependencyInfo(llvm::Value* val) const override;
    const DepInfo& getValueDependencyInfo(llvm::Value* val) override;
    DepInfo getInstructionDependencies(llvm::Instruction* instr) const override;
    const DependencyAnaliser::ValueDependencies& getValuesDependencies() const override;
    const DepInfo& getReturnValueDependencies() const override;
    const DependencyAnaliser::ArgumentDependenciesMap& getOutParamsDependencies() const override;
    const FCallsArgDeps& getFunctionsCallInfo() const override;
    const FunctionCallDepInfo& getFunctionCallInfo(llvm::Function* F) const override;
    bool hasFunctionCallInfo(llvm::Function* F) const override;
    const FunctionSet& getCallSitesData() const override;
    const GlobalsSet& getReferencedGlobals() const override;
    const GlobalsSet& getModifiedGlobals() const override;
    const ReflectingDependencyAnaliserT& getAnalysisResult(llvm::BasicBlock* block) const;
    void markAllInputDependent() override;
    /// \}

public:
    void reflect(const DependencyAnaliser::ValueDependencies& dependencies, const DepInfo& mandatory_deps) override;
    bool isReflected() const override
    {
        return m_isReflected;
    }

private:
    bool isSpecialLoopBlock(llvm::BasicBlock* B) const;
    DependencyAnaliser::ValueDependencies getBasicBlockPredecessorsDependencies(llvm::BasicBlock* B);
    DependencyAnaliser::ArgumentDependenciesMap getBasicBlockPredecessorsArguments(llvm::BasicBlock* B);
    void updateLoopDependecies(DepInfo&& depInfo);
    bool checkForLoopDependencies(llvm::BasicBlock* B);
    bool checkForLoopDependencies(const DependencyAnaliser::ValueDependencies& valueDeps);
    bool checkForLoopDependencies(const DependencyAnaliser::ArgumentDependenciesMap& argDeps);
    void updateFunctionCallInfo();
    void updateFunctionCallInfo(llvm::Function* F);
    void updateFunctionCallInfo(llvm::BasicBlock* B);
    void updateCalledFunctionsList();
    void updateReturnValueDependencies();
    void updateOutArgumentDependencies();
    void updateValueDependencies();
    void updateGlobals();
    void updateReferencedGlobals();
    void updateModifiedGlobals();
    void reflect();
    ReflectingDependencyAnaliserT createDependencyAnaliser(llvm::BasicBlock* B);
    ReflectingDependencyAnaliserT createInputDependentAnaliser(llvm::BasicBlock* B);
    void updateLoopDependecies(llvm::BasicBlock* B);
    DepInfo getBlockTerminatingDependencies(llvm::BasicBlock* B);
    DepInfo getBasicBlockDeps(llvm::BasicBlock* B) const;
    DepInfo getBlockTerminatingDependencies(llvm::BasicBlock* B) const;
    void collectLoopBlocks(llvm::Loop* block_loop);

private:
    llvm::Function* m_F;
    llvm::AAResults& m_AAR;
    const llvm::PostDominatorTree& m_postDomTree;
    const VirtualCallSiteAnalysisResult& m_virtualCallsInfo;
    const IndirectCallSitesAnalysisResult& m_indirectCallsInfo;
    Arguments m_inputs;
    const FunctionAnalysisGetter& m_FAG;
    llvm::Loop& m_L;
    llvm::LoopInfo& m_LI;
    std::unordered_set<llvm::BasicBlock*> m_latches;

    DependencyAnaliser::ArgumentDependenciesMap m_outArgDependencies;
    DepInfo m_returnValueDependencies;
    FCallsArgDeps m_functionCallInfo;
    FunctionSet m_calledFunctions;
    DependencyAnaliser::ValueDependencies m_initialDependencies;
    DependencyAnaliser::ValueDependencies m_valueDependencies;
    GlobalsSet m_referencedGlobals;
    GlobalsSet m_modifiedGlobals;
    bool m_globalsUpdated;

    BasicBlockDependencyAnalisersMap m_BBAnalisers;
    // LoopInfo will be invalidated after analisis, instead of keeping copy of it, keep this map.
    // for each block keep its' corresponding loop header block, to lookup for analiser
    std::unordered_map<llvm::BasicBlock*, llvm::BasicBlock*> m_loopBlocks;

    DepInfo m_loopDependencies;
    bool m_isReflected;
    bool m_is_inputDep;
}; // class LoopAnalysisResult

} // namespace input_dependency

