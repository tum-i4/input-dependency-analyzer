#pragma once

#include "definitions.h"
#include "DependencyAnaliser.h"
#include "FunctionCallDepInfo.h"

namespace input_dependency {

/**
* \class DependencyAnalysisResult
* Interface for providing dependency analysis information.
**/
class DependencyAnalysisResult
{
public:
    using ArgumentDependenciesMap = DependencyAnaliser::ArgumentDependenciesMap;
    using GlobalVariableDependencyMap = DependencyAnaliser::GlobalVariableDependencyMap;
    using ValueDependencies = DependencyAnaliser::ValueDependencies;
    using FunctionCallsArgumentDependencies = DependencyAnaliser::FunctionCallsArgumentDependencies;

public:
    DependencyAnalysisResult() = default;

    DependencyAnalysisResult(const DependencyAnalysisResult&) = delete;
    DependencyAnalysisResult(DependencyAnalysisResult&& ) = delete;
    DependencyAnalysisResult& operator =(const DependencyAnalysisResult&) = delete;
    DependencyAnalysisResult& operator =(DependencyAnalysisResult&&) = delete;

    virtual ~DependencyAnalysisResult() = default;

public:
    virtual void setInitialValueDependencies(const ValueDependencies& valueDependencies) = 0;
    virtual void setOutArguments(const ArgumentDependenciesMap& outArgs) = 0;

    /// \name Interface to start analysis
    /// \{
public:
    virtual void gatherResults() = 0;
    virtual void finalizeResults(const ArgumentDependenciesMap& dependentArgs) = 0;
    virtual void finalizeGlobals(const GlobalVariableDependencyMap& globalsDeps) = 0;
    virtual void dumpResults() const = 0;
    /// \}

    /// \name Abstract interface for getting analysis results
    /// \{
public:
    virtual bool isInputDependent(llvm::BasicBlock* bock) const = 0;
    virtual bool isInputDependent(llvm::BasicBlock* block, const ArgumentDependenciesMap& depArgs) const = 0;
    virtual bool isInputDependent(llvm::Instruction* instr) const = 0;
    virtual bool isInputDependent(llvm::Instruction* instr, const ArgumentDependenciesMap& depArgs) const = 0;
    virtual bool isInputIndependent(llvm::Instruction* instr) const = 0;
    virtual bool isInputIndependent(llvm::Instruction* instr, const ArgumentDependenciesMap& depArgs) const = 0;

    virtual bool hasValueDependencyInfo(llvm::Value* val) const = 0;
    virtual const ValueDepInfo& getValueDependencyInfo(llvm::Value* val) = 0;
    virtual DepInfo getInstructionDependencies(llvm::Instruction* instr) const = 0;
    virtual const ValueDependencies& getValuesDependencies() const = 0;
    virtual const DepInfo& getReturnValueDependencies() const = 0;
    virtual const ArgumentDependenciesMap& getOutParamsDependencies() const = 0;
    virtual const FunctionCallsArgumentDependencies& getFunctionsCallInfo() const = 0;
    virtual bool hasFunctionCallInfo(llvm::Function* F) const = 0;
    virtual const FunctionCallDepInfo& getFunctionCallInfo(llvm::Function* F) const = 0;
    virtual bool changeFunctionCall(llvm::Instruction* instr, llvm::Function* oldF, llvm::Function* newCallee) = 0;
    virtual const FunctionSet& getCallSitesData() const = 0;
    virtual const GlobalsSet& getReferencedGlobals() const = 0;
    virtual const GlobalsSet& getModifiedGlobals() const = 0;
    virtual void markAllInputDependent() = 0;

    // debug interface
    virtual long unsigned get_input_dep_count() const = 0;
    virtual long unsigned get_input_indep_count() const = 0;
    virtual long unsigned get_input_unknowns_count() const = 0;
    /// \}

protected:
}; // class DependencyAnalysisResult

} // namespace input_dependency

