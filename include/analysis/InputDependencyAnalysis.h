#pragma once

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
}

namespace input_dependency {

class InputDepInfo;

class InputDependencyAnalysis
{
public:
    using PDGType = std::shared_ptr<pdg::PDG>;
    using NodeType = std::shared_ptr<pdg::PDGNode>;
    using FunctionSet = std::unordered_set<llvm::Function*>;
    using ArgInputDependencies = std::vector<InputDepInfo>;
    using FunctionArgumentDependencies = std::unordered_map<llvm::Function*, ArgInputDependencies>;

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
    void analyze();

private:
    void runArgumentReachabilityAnalysis();
    void runInputReachabilityAnalysis();
    void collectFunctionsInBottomUp();
    void setArgumentDependencies();
    void updateFunctionArgDeps(NodeType node);
    FunctionSet getCalledFunction(const llvm::CallSite& callSite);

private:
    PDGType m_pdg;
    llvm::Module* m_module;
    llvm::CallGraph* m_callGraph;
    FunctionArgumentDependencies m_functionArgDeps;
    std::vector<llvm::Function*> m_functions;
}; // class InputDependencyAnalysis

} // namespace input_dependency

