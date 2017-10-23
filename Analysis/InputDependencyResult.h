#pragma once

#include "definitions.h"
#include "FunctionCallDepInfo.h"

namespace llvm {

class Function;
class BasicBlock;
class Instruction;
class Value;

} // namespace llvm

namespace input_dependency {

class FunctionAnaliser;
class InputDependentFunctionAnalysisResult;

/// Defines interface to request for input dependency information
class InputDependencyResult
{
public:
    virtual llvm::Function* getFunction() = 0;
    virtual const llvm::Function* getFunction() const = 0;

    virtual bool isInputDependent(llvm::Instruction* instr) const = 0;
    virtual bool isInputDependent(const llvm::Instruction* instr) const = 0;
    virtual bool isInputIndependent(llvm::Instruction* instr) const = 0;
    virtual bool isInputIndependent(const llvm::Instruction* instr) const = 0;
    //TODO: implement later if needed
    //virtual bool isInputDependent(llvm::Value* val) const = 0;
    virtual bool isInputDependentBlock(llvm::BasicBlock* block) const = 0;

    // these functions may be moved to separate interface, as they are relevent only for some implementations of the this interface
    virtual FunctionSet getCallSitesData() const = 0;
    virtual FunctionCallDepInfo getFunctionCallDepInfo(llvm::Function* F) const = 0;
    virtual bool changeFunctionCall(const llvm::Instruction* callInstr, llvm::Function* oldF, llvm::Function* newF)
    {
        assert(false);
        return false;
    }

    // for debug only
    virtual long unsigned get_input_dep_count() const = 0;
    virtual long unsigned get_input_indep_count() const = 0;
    virtual long unsigned get_input_unknowns_count() const = 0;

    // cast interface
    virtual FunctionAnaliser* toFunctionAnalysisResult()
    {
        return nullptr;
    }

    virtual InputDependentFunctionAnalysisResult* toInputDependentFunctionAnalysisResult()
    {
        return nullptr;
    }

}; // class InputDependencyResult

} // namespace input_dependency

