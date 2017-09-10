#pragma once

namespace llvm {

class BasicBlock;
class Instruction;
class Value;

} // namespace llvm

namespace input_dependency {

class FunctionAnaliser;

/// Defines interface to request for input dependency information
class InputDependencyResult
{
public:
    virtual bool isInputDependent(llvm::Instruction* instr) const = 0;
    virtual bool isInputDependent(const llvm::Instruction* instr) const = 0;
    virtual bool isInputIndependent(llvm::Instruction* instr) const = 0;
    virtual bool isInputIndependent(const llvm::Instruction* instr) const = 0;
    virtual bool isInputDependent(llvm::Value* val) const = 0;
    virtual bool isInputIndependent(llvm::Value* val) const = 0;
    virtual bool isInputDependentBlock(llvm::BasicBlock* block) const = 0;

    // for debug only
    virtual long unsigned get_input_dep_count() const = 0;
    virtual long unsigned get_input_indep_count() const = 0;
    virtual long unsigned get_input_unknowns_count() const = 0;

    // cast interface
    virtual FunctionAnaliser* toFunctionAnalysisResult()
    {
        return nullptr;
    }

}; // class InputDependencyResult

} // namespace input_dependency

