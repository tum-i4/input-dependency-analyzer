#pragma once

#include "llvm/Pass.h"

#include <unordered_map>
#include <unordered_set>

namespace llvm {
class CallGraph;
class CallGraphNode;
class Function;
class Module;
}

namespace input_dependency {

class InputDependencyAnalysisInterface;

class ReachableFunctions
{
public:
    using FunctionSet = std::unordered_set<llvm::Function*>;

public:
    ReachableFunctions(llvm::Module* M,
                       llvm::CallGraph* cfg);

    ReachableFunctions(const ReachableFunctions& ) = delete;
    ReachableFunctions(ReachableFunctions&& ) = delete;
    ReachableFunctions& operator =(const ReachableFunctions& ) = delete;
    ReachableFunctions& operator =(ReachableFunctions&& ) = delete;

public:
    void setInputDependencyAnalysisResult(InputDependencyAnalysisInterface* inputDepAnalysis);

    FunctionSet getReachableFunctions(llvm::Function* F);
    
private:
    void collect_reachable_functions(llvm::CallGraphNode* callNode,
                                     FunctionSet& reachable_functions);
    void collect_indirectly_reachable_functions(FunctionSet& reachable_functions);

private:
    llvm::Module* m_module;
    llvm::CallGraph* m_callGraph;
    InputDependencyAnalysisInterface* m_inputDepAnalysis;
};

class ReachableFunctionsPass : public llvm::ModulePass
{
public:
  static char ID;

  ReachableFunctionsPass()
      : llvm::ModulePass(ID)
  {}

  bool runOnModule(llvm::Module &M) override;
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

};

}

