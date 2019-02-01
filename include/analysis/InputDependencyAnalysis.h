#pragma once

#include "analysis/InputDependencyAnalysisInterface.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace pdg {
class PDG;
class PDGNode;
}

namespace llvm {
class Function;
class CallGraph;
class CallSite;
class Module;
class Instruction;
class BasicBlock;
}

namespace input_dependency {

class InputDepInfo;

class InputDependencyAnalysis : public InputDependencyAnalysisInterface
{
public:
    using PDGType = std::shared_ptr<pdg::PDG>;
    using NodeType = std::shared_ptr<pdg::PDGNode>;
    using FunctionSet = std::unordered_set<llvm::Function*>;
    using ArgInputDependencies = std::vector<InputDepInfo>;
    using FunctionArgumentDependencies = std::unordered_map<llvm::Function*, ArgInputDependencies>;

private:
    struct DebugInfo
    {
        unsigned InputIndepInstrCount = 0;
        unsigned InputIndepBlocksCount = 0;
        unsigned InputDepInstrCount = 0;
        unsigned InputDepBlocksCount = 0;
        unsigned DataIndepInstrCount = 0;
        unsigned ArgumentDepInstrCount = 0;
        unsigned UnreachableBlocksCount = 0;
        unsigned UnreachableInstructionsCount = 0;
    };

public:
    InputDependencyAnalysis(llvm::Module* module);

    InputDependencyAnalysis(const InputDependencyAnalysis& ) = delete;
    InputDependencyAnalysis(InputDependencyAnalysis&& ) = delete;
    InputDependencyAnalysis& operator =(const InputDependencyAnalysis& ) = delete;
    InputDependencyAnalysis& operator =(InputDependencyAnalysis&& ) = delete;

public:
    void setPDG(PDGType pdg);
    void setCallGraph(llvm::CallGraph* callGraph);

public:
    void analyze() override;

    bool isInputDependent(llvm::Function* F, llvm::Instruction* instr) const override;
    bool isInputDependent(llvm::Instruction* instr) const override;
    bool isInputDependent(llvm::BasicBlock* block) const override;
    bool isInputDependent(llvm::Function* F) const override;
    bool isControlDependent(llvm::Instruction* I) const override;
    bool isDataDependent(llvm::Instruction* I) const override;
    bool isArgumentDependent(llvm::Instruction* I) const override;
    bool isArgumentDependent(llvm::BasicBlock* B) const override;

    unsigned getInputIndepInstrCount(llvm::Function* F) const override;
    unsigned getInputIndepBlocksCount(llvm::Function* F) const override;
    unsigned getInputDepInstrCount(llvm::Function* F) const override;
    unsigned getInputDepBlocksCount(llvm::Function* F) const override;
    unsigned getDataIndepInstrCount(llvm::Function* F) const override;
    unsigned getArgumentDepInstrCount(llvm::Function* F) const override;
    unsigned getUnreachableBlocksCount(llvm::Function* F) const override;
    unsigned getUnreachableInstructionsCount(llvm::Function* F) const override;

private:
    void runArgumentReachabilityAnalysis();
    void runInputReachabilityAnalysis();
    void collectFunctionsInBottomUp();
    void setArgumentDependencies();
    void updateFunctionArgDeps(NodeType node);
    FunctionSet getCalledFunction(const llvm::CallSite& callSite);
    void collectFunctionDebugInfo(llvm::Function* F);

private:
    PDGType m_pdg;
    llvm::Module* m_module;
    llvm::CallGraph* m_callGraph;
    FunctionArgumentDependencies m_functionArgDeps;
    std::vector<llvm::Function*> m_functions;
    std::unordered_map<llvm::Function*, DebugInfo> m_functionDbgInfo;
}; // class InputDependencyAnalysis

} // namespace input_dependency

