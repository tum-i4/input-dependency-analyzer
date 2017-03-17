#pragma once

#include "DependencyAnalysisResult.h"
#include "ReflectingDependencyAnaliser.h"

#include "llvm/ADT/SmallVector.h"

#include <memory>

namespace llvm {
class Loop;
}

namespace input_dependency {

// TODO: duplicates most of functionality of FunctionAnaliser
class LoopAnalysisResult : public DependencyAnalysisResult
{
public:
    LoopAnalysisResult(llvm::Function* F,
                       llvm::AAResults& AAR,
                       const Arguments& inputs,
                       const FunctionAnalysisGetter& Fgetter,
                       llvm::Loop& LI);

    LoopAnalysisResult(const LoopAnalysisResult&) = delete;
    LoopAnalysisResult(LoopAnalysisResult&& ) = delete;
    LoopAnalysisResult& operator =(const LoopAnalysisResult&) = delete;
    LoopAnalysisResult& operator =(LoopAnalysisResult&&) = delete;

    virtual ~LoopAnalysisResult() = default;

    /// \name Interface to start analysis
    /// \{
public:
    void gatherResults() override;
    void finalizeResults(const DependencyAnaliser::ArgumentDependenciesMap& dependentArgs) override;
    void dumpResults() const override;

    /// \}

    /// \name Abstract interface for getting analysis results
    /// \{
public:
    void setInitialValueDependencies(const DependencyAnalysisResult::InitialValueDpendencies& valueDependencies) override;
    void setOutArguments(const InitialArgumentDependencies& outArgs) override;
    bool isInputDependent(llvm::Instruction* instr) const override;
    const ArgumentSet& getValueInputDependencies(llvm::Value* val) const override;
    DepInfo getInstructionDependencies(llvm::Instruction* instr) const override;
    const DependencyAnaliser::ValueDependencies& getValuesDependencies() const override;
    //const ValueSet& getValueDependencies(llvm::Value* val) override;
    const DepInfo& getReturnValueDependencies() const override;
    const DependencyAnaliser::ArgumentDependenciesMap& getOutParamsDependencies() const override;
    const DependencyAnaliser::FunctionArgumentsDependencies& getFunctionsCallInfo() const override;
    /// \}

private:
    using PredValDeps = DependencyAnalysisResult::InitialValueDpendencies;
    using PredArgDeps = DependencyAnalysisResult::InitialArgumentDependencies;
    using ReflectingDependencyAnaliserT = std::unique_ptr<ReflectingDependencyAnaliser>;
    using BasicBlockDependencyAnalisersMap = std::unordered_map<llvm::BasicBlock*, ReflectingDependencyAnaliserT>;

private:
    PredValDeps getBasicBlockPredecessorsDependencies(llvm::BasicBlock* B);
    PredArgDeps getBasicBlockPredecessorsArguments(llvm::BasicBlock* B);
    void updateFunctionCallInfo();
    void updateReturnValueDependencies();
    void updateOutArgumentDependencies();
    void updateValueDependencies();
    void reflect();
    using SuccessorDeps = std::vector<DependencyAnaliser::ValueDependencies>;
    using Latches = llvm::SmallVector<llvm::BasicBlock*, 10>;
    SuccessorDeps getBlockSuccessorDependencies(llvm::BasicBlock* B, const Latches& latches) const;
    ReflectingDependencyAnaliserT createReflectingBasicBlockAnaliser(llvm::BasicBlock* B);
    DepInfo getBasicBlockPredecessorInstructionsDeps(llvm::BasicBlock* B) const;

private:
    llvm::Function* m_F;
    llvm::AAResults& m_AAR;
    Arguments m_inputs;
    const FunctionAnalysisGetter& m_FAG;
    llvm::Loop& m_LI;

    DependencyAnaliser::ArgumentDependenciesMap m_outArgDependencies;
    DepInfo m_returnValueDependencies;
    DependencyAnaliser::FunctionArgumentsDependencies m_calledFunctionsInfo;
    DependencyAnaliser::ValueDependencies m_valueDependencies;

    BasicBlockDependencyAnalisersMap m_BBAnalisers;
}; // class LoopAnalysisResult

} // namespace input_dependency

