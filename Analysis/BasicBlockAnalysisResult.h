#pragma once

#include "DependencyAnaliser.h"
#include "DependencyAnalysisResult.h"
#include "definitions.h"

namespace llvm {

class BasicBlock;

}

namespace input_dependency {

class VirtualCallSiteAnalysisResult;
class IndirectCallSitesAnalysisResult;

/**
* \class BasicBlockAnalysisResult
* \brief Implementation of the dependency analyser and results reporter for a basic block
**/
class BasicBlockAnalysisResult : public virtual DependencyAnalysisResult
                               , public DependencyAnaliser
{
public:
    using ArgumentDependenciesMap = DependencyAnaliser::ArgumentDependenciesMap;
    using GlobalVariableDependencyMap = DependencyAnaliser::GlobalVariableDependencyMap;
    using ValueDependencies = DependencyAnaliser::ValueDependencies;
    using FunctionCallsArgumentDependencies = DependencyAnaliser::FunctionCallsArgumentDependencies;

public:
    BasicBlockAnalysisResult(llvm::Function* F,
                             llvm::AAResults& AAR,
                             const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                             const IndirectCallSitesAnalysisResult& indirectCallsInfo,
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
    ValueDepInfo getValueDependencies(llvm::Value* value) override;
    DepInfo getCompositeValueDependencies(llvm::Value* value, llvm::Instruction* element_instr) override;
    void updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info) override;
    void updateValueDependencies(llvm::Value* value, const DepInfo& info) override;
    void updateValueDependencies(llvm::Value* value, const ValueDepInfo& info) override;
    void updateCompositeValueDependencies(llvm::Value* value,
                                          llvm::Instruction* elInstr,
                                          const DepInfo& info) override;
    void updateReturnValueDependencies(const ValueDepInfo& info) override;
    DepInfo getDependenciesFromAliases(llvm::Value* val) override;
    DepInfo getRefInfo(llvm::LoadInst* loadInst) override;
    void updateAliasesDependencies(llvm::Value* val, const ValueDepInfo& info) override;
    void updateModAliasesDependencies(llvm::StoreInst* storeInst, const DepInfo& info) override;
    void updateRefAliasesDependencies(llvm::Instruction* instr, const DepInfo& info);
    DepInfo getLoadInstrDependencies(llvm::LoadInst* instr) override;
    DepInfo determineInstructionDependenciesFromOperands(llvm::Instruction* instr) override;
    /// \}

    /// \name Implementation of DependencyAnalysisResult interface
    /// \{
public:
    void setInitialValueDependencies(const ValueDependencies& valueDependencies) override;
    void setOutArguments(const ArgumentDependenciesMap& outArgs) override;

    bool isInputDependent(llvm::BasicBlock* block) const override;
    bool isInputDependent(llvm::BasicBlock* block, const ArgumentDependenciesMap& depArgs) const override;
    bool isInputDependent(llvm::Instruction* instr) const override;
    bool isInputDependent(llvm::Instruction* instr, const ArgumentDependenciesMap& depArgs) const override;
    bool isInputIndependent(llvm::Instruction* instr) const override;
    bool isInputIndependent(llvm::Instruction* instr, const ArgumentDependenciesMap& depArgs) const override;

    bool hasValueDependencyInfo(llvm::Value* val) const override;
    const ValueDepInfo& getValueDependencyInfo(llvm::Value* val) override;
    DepInfo getInstructionDependencies(llvm::Instruction* instr) const override;
    const ValueDependencies& getValuesDependencies() const override;
    const ValueDepInfo& getReturnValueDependencies() const override;
    const ArgumentDependenciesMap& getOutParamsDependencies() const override;
    const FunctionCallsArgumentDependencies& getFunctionsCallInfo() const override;
    const FunctionCallDepInfo& getFunctionCallInfo(llvm::Function* F) const override;
    bool changeFunctionCall(llvm::Instruction* instr, llvm::Function* oldF, llvm::Function* newCallee) override;
    bool hasFunctionCallInfo(llvm::Function* F) const override;
    const FunctionSet& getCallSitesData() const override;
    const GlobalsSet& getReferencedGlobals() const override;
    const GlobalsSet& getModifiedGlobals() const override;
    void markAllInputDependent() override;

    long unsigned get_input_dep_count() const override;
    long unsigned get_input_indep_count() const override;
    long unsigned get_input_unknowns_count() const override;
    /// \}

protected:
    llvm::BasicBlock* m_BB;
    bool m_is_inputDep;
}; // class BasicBlockAnalysisResult

} // namespace input_dependency

