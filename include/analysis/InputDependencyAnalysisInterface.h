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

    // TODO: think about adding argument dependencies and global dependencies
}; // class InputDependencyAnalysisInterface

} // namespace input_dependency

