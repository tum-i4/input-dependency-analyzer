#pragma once

#include "DependencyAnaliser.h"
#include "DependencyAnalysisResult.h"
#include "definitions.h"

namespace llvm {

class BasicBlock;

}

namespace input_dependency {

class VirtualCallSiteAnalysisResult;

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
                             const VirtualCallSiteAnalysisResult& virtualCallsInfo,
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
    void finalizeGlobals(const GlobalVariableDependencyMap& globalsDeps) override;
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
    DepInfo getRefInfo(llvm::LoadInst* loadInst) override;
    void updateAliasesDependencies(llvm::Value* val, const DepInfo& info) override;
    void updateModAliasesDependencies(llvm::StoreInst* storeInst, const DepInfo& info) override;
    DepInfo getLoadInstrDependencies(llvm::LoadInst* instr) override;
    DepInfo determineInstructionDependenciesFromOperands(llvm::Instruction* instr) override;
    /// \}

    /// \name Implementation of DependencyAnalysisResult interface
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
    const ArgumentDependenciesMap& getOutParamsDependencies() const override;
    const FunctionCallsArgumentDependencies& getFunctionsCallInfo() const override;
    const FunctionCallDepInfo& getFunctionCallInfo(llvm::Function* F) const override;
    bool hasFunctionCallInfo(llvm::Function* F) const override;
    const FunctionSet& getCallSitesData() const override;
    const GlobalsSet& getReferencedGlobals() const override;
    const GlobalsSet& getModifiedGlobals() const override;
    /// \}

protected:
    llvm::BasicBlock* m_BB;
}; // class BasicBlockAnalysisResult

} // namespace input_dependency

