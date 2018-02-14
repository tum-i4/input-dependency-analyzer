#pragma once

#include <memory>
#include <unordered_map>

namespace llvm {
class AAResults;
class Function;
class Instruction;
class BasicBlock;
}

namespace input_dependency {

class FunctionInputDependencyResultInterface;

class InputDependencyAnalysisInterface
{
public:
    using InputDepResType = std::shared_ptr<FunctionInputDependencyResultInterface>;
    using InputDependencyAnalysisInfo = std::unordered_map<llvm::Function*, InputDepResType>;
    using AliasAnalysisInfoGetter = std::function<llvm::AAResults* (llvm::Function* F)>;

public:
    InputDependencyAnalysisInterface() = default;

    virtual ~InputDependencyAnalysisInterface()
    {
    }

public:
    virtual void run() = 0;
    virtual bool isInputDependent(llvm::Function* F, llvm::Instruction* instr) const = 0;
    virtual bool isInputDependent(llvm::Instruction* instr) const = 0;
    virtual bool isInputDependent(llvm::BasicBlock* block) const = 0;
    virtual bool isControlDependent(llvm::Instruction* I) const = 0;
    virtual bool isDataDependent(llvm::Instruction* I) const = 0;
    virtual const InputDependencyAnalysisInfo& getAnalysisInfo() const = 0;
    virtual InputDependencyAnalysisInfo& getAnalysisInfo() = 0;
    virtual InputDepResType getAnalysisInfo(llvm::Function* F) = 0;
    virtual const InputDepResType getAnalysisInfo(llvm::Function* F) const = 0;
    virtual bool insertAnalysisInfo(llvm::Function* F, InputDepResType analysis_info) = 0;
}; // class InputDependencyAnalysisInterface

} // namespace input_dependency

