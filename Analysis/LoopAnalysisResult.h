#pragma once

#include "DependencyAnalysisResult.h"
#include "ReflectingDependencyAnaliser.h"

#include "llvm/ADT/SmallVector.h"

#include <memory>

namespace llvm {
class Loop;
class LoopInfo;
}

namespace input_dependency {

class VirtualCallSiteAnalysisResult;

class LoopAnalysisResult : public DependencyAnalysisResult
{
public:
    LoopAnalysisResult(llvm::Function* F,
                       llvm::AAResults& AAR,
                       const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                       const Arguments& inputs,
                       const FunctionAnalysisGetter& Fgetter,
                       llvm::Loop& L,
                       llvm::LoopInfo& LI);

    LoopAnalysisResult(const LoopAnalysisResult&) = delete;
    LoopAnalysisResult(LoopAnalysisResult&& ) = delete;
    LoopAnalysisResult& operator =(const LoopAnalysisResult&) = delete;
    LoopAnalysisResult& operator =(LoopAnalysisResult&&) = delete;

    virtual ~LoopAnalysisResult() = default;


private:
    using PredValDeps = DependencyAnalysisResult::InitialValueDpendencies;
    using PredArgDeps = DependencyAnalysisResult::InitialArgumentDependencies;
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
    void setInitialValueDependencies(const DependencyAnalysisResult::InitialValueDpendencies& valueDependencies) override;
    void setOutArguments(const InitialArgumentDependencies& outArgs) override;
    bool isInputDependent(llvm::Instruction* instr) const override;
    bool isInputIndependent(llvm::Instruction* instr) const override;
    bool hasValueDependencyInfo(llvm::Value* val) const override;
    const DepInfo& getValueDependencyInfo(llvm::Value* val) const override;
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
    /// \}

private:
    PredValDeps getBasicBlockPredecessorsDependencies(llvm::BasicBlock* B);
    PredArgDeps getBasicBlockPredecessorsArguments(llvm::BasicBlock* B);
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
    ReflectingDependencyAnaliserT createReflectingBasicBlockAnaliser(llvm::BasicBlock* B);
    DepInfo getBasicBlockDeps(llvm::BasicBlock* B) const;
    void addLoopDependencies(llvm::BasicBlock* B, DepInfo& depInfo) const;
    void addLoopDependency(llvm::BasicBlock* B, DepInfo& depInfo) const;
    DepInfo getBlockTerminatingDependencies(llvm::BasicBlock* B) const;

private:
    llvm::Function* m_F;
    llvm::AAResults& m_AAR;
    const VirtualCallSiteAnalysisResult& m_virtualCallsInfo;
    Arguments m_inputs;
    const FunctionAnalysisGetter& m_FAG;
    llvm::Loop& m_L;
    llvm::LoopInfo& m_LI;

    DependencyAnaliser::ArgumentDependenciesMap m_outArgDependencies;
    DepInfo m_returnValueDependencies;
    FCallsArgDeps m_functionCallInfo;
    FunctionSet m_calledFunctions;
    DependencyAnaliser::ValueDependencies m_valueDependencies;
    GlobalsSet m_referencedGlobals;
    GlobalsSet m_modifiedGlobals;
    bool m_globalsUpdated;

    BasicBlockDependencyAnalisersMap m_BBAnalisers;
}; // class LoopAnalysisResult

} // namespace input_dependency

