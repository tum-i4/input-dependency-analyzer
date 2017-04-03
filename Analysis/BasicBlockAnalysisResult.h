#pragma once

#include "DependencyAnaliser.h"
#include "DependencyAnalysisResult.h"
#include "definitions.h"

namespace llvm {

class BasicBlock;

}

namespace input_dependency {

/**
* \class BasicBlockAnalysisResult
* \brief Implementation of the dependency analyser and results reporter for a basic block
**/
class BasicBlockAnalysisResult : public virtual DependencyAnalysisResult
                               , public DependencyAnaliser
{
public:
    BasicBlockAnalysisResult(llvm::Function* F,
                             llvm::AAResults& AAR,
                             const Arguments& inputs,
                             const FunctionAnalysisGetter& Fgetter,
                             llvm::BasicBlock* BB);

    BasicBlockAnalysisResult(const BasicBlockAnalysisResult&) = delete;
    BasicBlockAnalysisResult(BasicBlockAnalysisResult&& ) = delete;
    BasicBlockAnalysisResult& operator =(const BasicBlockAnalysisResult&) = delete;
    BasicBlockAnalysisResult& operator =(BasicBlockAnalysisResult&&) = delete;

    virtual ~BasicBlockAnalysisResult() = default;

public:
    void gatherResults() override;
    void finalizeResults(const ArgumentDependenciesMap& dependentArgs) override;
    void dumpResults() const override;

public:
    void analize() override;

    /// \name Implementation of DependencyAnaliser interface
    /// \{
protected:
    DepInfo getInstructionDependencies(llvm::Instruction* instr) override;
    DepInfo getValueDependencies(llvm::Value* value) override;
    void updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info) override;
    void updateValueDependencies(llvm::Value* value, const DepInfo& info) override;
    void updateReturnValueDependencies(const DepInfo& info) override;
    DepInfo getDependenciesFromAliases(llvm::Value* val) override;
    void updateAliasesDependencies(llvm::Value* val, const DepInfo& info) override;

    /// \}

    /// \name Implementation of DependencyAnalysisResult interface
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
    const ArgumentDependenciesMap& getOutParamsDependencies() const override;
    const FunctionCallsArgumentDependencies& getFunctionsCallInfo() const override;
    const FunctionCallDepInfo& getFunctionCallInfo(llvm::Function* F) const override;
    bool hasFunctionCallInfo(llvm::Function* F) const override;
    const FunctionSet& getCallSitesData() const override;
    /// \}

private:
    DepInfo getLoadInstrDependencies(llvm::LoadInst* instr) override;
    DepInfo determineInstructionDependenciesFromOperands(llvm::Instruction* instr) override;

private:
    llvm::BasicBlock* m_BB;
}; // class BasicBlockAnalysisResult

} // namespace input_dependency

