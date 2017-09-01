#pragma once

#include "llvm/Pass.h"

#include <unordered_set>

namespace llvm {
class Function;
class BasicBlock;
class CallGraph;
}

namespace input_dependency {
    class IndirectCallSitesAnalysisResult;
    class InputDependencyAnalysis;

class FunctionDominanceTree;

class InputDependentFunctionsPass : public llvm::ModulePass
{
public:
   static char ID;
   
   InputDependentFunctionsPass()
        : llvm::ModulePass(ID)
   {
   }

public:
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;

public:
    bool is_function_input_dependent(llvm::Function* F) const;
    bool is_function_input_independent(llvm::Function* F) const;

private:
    using FunctionSet = std::unordered_set<llvm::Function*>;

    std::vector<llvm::Function*> collect_functons(llvm::Module& M);
    void erase_from_deterministic_functions(const FunctionSet& targets);
    void process_non_det_block(llvm::BasicBlock& block,
                               const IndirectCallSitesAnalysisResult& indirectCallSitesInfo);
    void process_function(llvm::Function* F,
                          const IndirectCallSitesAnalysisResult& indirectCallSitesInfo,
                          const InputDependencyAnalysis& inputDepAnalysis,
                          const FunctionDominanceTree& domTree,
                          FunctionSet& processed_function);
    void process_call(llvm::Function* parentF,
                      const FunctionSet& targets,
                      const IndirectCallSitesAnalysisResult& indirectCallSitesInfo,
                      const InputDependencyAnalysis& inputDepAnalysis,
                      const FunctionDominanceTree& domTree,
                      FunctionSet& processed_functions);

private:
    FunctionSet functions_called_in_loop;
    FunctionSet functions_called_in_non_det_blocks;
    FunctionSet functions_called_in_det_blocks;
};

}

