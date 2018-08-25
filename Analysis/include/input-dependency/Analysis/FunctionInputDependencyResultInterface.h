#pragma once

#include "input-dependency/Analysis/FunctionCallDepInfo.h"

namespace llvm {

class Function;
class BasicBlock;
class Instruction;
class Value;

} // namespace llvm

namespace input_dependency {

class FunctionAnaliser;
class ClonedFunctionAnalysisResult;
class InputDependentFunctionAnalysisResult;
class CachedFunctionAnalysisResult;

/// Defines interface to request for input dependency information
class FunctionInputDependencyResultInterface
{
public:
    virtual ~FunctionInputDependencyResultInterface()
    {
    }

public:
    virtual void analyze() = 0;
    virtual llvm::Function* getFunction() = 0;
    virtual const llvm::Function* getFunction() const = 0;
    virtual bool isInputDepFunction() const = 0;
    virtual void setIsInputDepFunction(bool isInputDep) = 0;
    virtual bool isExtractedFunction() const = 0;
    virtual void setIsExtractedFunction(bool isExtracted) = 0;

    virtual bool isInputDependent(llvm::Instruction* instr) const = 0;
    virtual bool isInputDependent(const llvm::Instruction* instr) const = 0;
    virtual bool isInputIndependent(llvm::Instruction* instr) const = 0;
    virtual bool isInputIndependent(const llvm::Instruction* instr) const = 0;
    virtual bool isInputDependentBlock(llvm::BasicBlock* block) const = 0;
    virtual bool isControlDependent(llvm::Instruction* I) const = 0;
    virtual bool isDataDependent(llvm::Instruction* I) const = 0;
    virtual bool isArgumentDependent(llvm::Instruction* I) const = 0;
    virtual bool isArgumentDependent(llvm::BasicBlock* block) const = 0;
    virtual bool isGlobalDependent(llvm::Instruction* I) const = 0;
    // these functions may be moved to separate interface, as they are relevent only for some implementations of the this interface
    virtual FunctionSet getCallSitesData() const = 0;
    virtual FunctionCallDepInfo getFunctionCallDepInfo(llvm::Function* F) const = 0;
    virtual bool changeFunctionCall(const llvm::Instruction* callInstr, llvm::Function* oldF, llvm::Function* newF)
    {
        assert(false);
        return false;
    }

    // caching for statistics
    virtual long unsigned get_input_dep_blocks_count() const = 0;
    virtual long unsigned get_input_indep_blocks_count() const = 0;
    virtual long unsigned get_unreachable_blocks_count() const = 0;
    virtual long unsigned get_unreachable_instructions_count() const = 0;
    virtual long unsigned get_input_dep_count() const = 0;
    virtual long unsigned get_input_indep_count() const = 0;
    virtual long unsigned get_data_indep_count() const = 0;
    virtual long unsigned get_input_unknowns_count() const = 0;

    // cast interface
    virtual FunctionAnaliser* toFunctionAnalysisResult()
    {
        return nullptr;
    }

    virtual ClonedFunctionAnalysisResult* toClonedFunctionAnalysisResult()
    {
        return nullptr;
    }

    virtual InputDependentFunctionAnalysisResult* toInputDependentFunctionAnalysisResult()
    {
        return nullptr;
    }

    virtual CachedFunctionAnalysisResult* toCachedInputDependentFunctionAnalysisResult()
    {
        return nullptr;
    }

}; // class InputDependencyResult

} // namespace input_dependency

