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
    DependencyAnalysisResult() = default;

    DependencyAnalysisResult(const DependencyAnalysisResult&) = delete;
    DependencyAnalysisResult(DependencyAnalysisResult&& ) = delete;
    DependencyAnalysisResult& operator =(const DependencyAnalysisResult&) = delete;
    DependencyAnalysisResult& operator =(DependencyAnalysisResult&&) = delete;


    virtual ~DependencyAnalysisResult() = default;

public:
    using InitialValueDpendencies = std::unordered_map<llvm::Value*, std::vector<DepInfo>>;
    virtual void setInitialValueDependencies(const InitialValueDpendencies& valueDependencies) = 0;
    using InitialArgumentDependencies =  std::unordered_map<llvm::Argument*, std::vector<DepInfo>>;
    virtual void setOutArguments(const InitialArgumentDependencies& outArgs) = 0;

    /// \name Interface to start analysis
    /// \{
public:
    virtual void gatherResults() = 0;
    virtual void finalizeResults(const DependencyAnaliser::ArgumentDependenciesMap& dependentArgs) = 0;
    virtual void finalizeGlobals(const DependencyAnaliser::GlobalVariableDependencyMap& globalsDeps) = 0;
    virtual void dumpResults() const = 0;
    /// \}

    /// \name Abstract interface for getting analysis results
    /// \{
public:
    virtual bool isInputDependent(llvm::Instruction* instr) const = 0;
    virtual bool isInputIndependent(llvm::Instruction* instr) const = 0;
    virtual bool hasValueDependencyInfo(llvm::Value* val) const = 0;
    virtual const DepInfo& getValueDependencyInfo(llvm::Value* val) const = 0;
    virtual DepInfo getInstructionDependencies(llvm::Instruction* instr) const = 0;
    virtual const DependencyAnaliser::ValueDependencies& getValuesDependencies() const = 0;
    virtual const DepInfo& getReturnValueDependencies() const = 0;
    virtual const DependencyAnaliser::ArgumentDependenciesMap& getOutParamsDependencies() const = 0;
    virtual const DependencyAnaliser::FunctionCallsArgumentDependencies& getFunctionsCallInfo() const = 0;
    virtual const FunctionCallDepInfo& getFunctionCallInfo(llvm::Function* F) const = 0;
    virtual bool hasFunctionCallInfo(llvm::Function* F) const = 0;
    virtual const FunctionSet& getCallSitesData() const = 0;
    virtual const GlobalsSet& getReferencedGlobals() const = 0;
    virtual const GlobalsSet& getModifiedGlobals() const = 0;
    /// \}

protected:
}; // class DependencyAnalysisResult

} // namespace input_dependency

