#pragma once

namespace llvm {
class Function;
class Instruction;
class BasicBlock;
}

namespace input_dependency {

class InputDependencyAnalysisInterface
{
public:
    virtual ~InputDependencyAnalysisInterface()
    {
    }

    virtual void analyze() = 0;
    virtual bool isInputDependent(llvm::Function* F, llvm::Instruction* instr) const = 0;
    virtual bool isInputDependent(llvm::Instruction* instr) const = 0;
    virtual bool isInputDependent(llvm::BasicBlock* block) const = 0;
    virtual bool isInputDependent(llvm::Function* F) const = 0;
    virtual bool isControlDependent(llvm::Instruction* I) const = 0;
    virtual bool isDataDependent(llvm::Instruction* I) const = 0;
    virtual bool isArgumentDependent(llvm::Instruction* I) const = 0;
    virtual bool isArgumentDependent(llvm::BasicBlock* B) const = 0;

    virtual unsigned getInputIndepInstrCount(llvm::Function* F) const = 0;
    virtual unsigned getInputIndepBlocksCount(llvm::Function* F) const = 0;
    virtual unsigned getInputDepInstrCount(llvm::Function* F) const = 0;
    virtual unsigned getInputDepBlocksCount(llvm::Function* F) const = 0;
    virtual unsigned getDataIndepInstrCount(llvm::Function* F) const = 0;
    virtual unsigned getArgumentDepInstrCount(llvm::Function* F) const = 0;
    virtual unsigned getUnreachableBlocksCount(llvm::Function* F) const = 0;
    virtual unsigned getUnreachableInstructionsCount(llvm::Function* F) const = 0;

    // TODO: think about adding argument dependencies and global dependencies
}; // class InputDependencyAnalysisInterface

} // namespace input_dependency

