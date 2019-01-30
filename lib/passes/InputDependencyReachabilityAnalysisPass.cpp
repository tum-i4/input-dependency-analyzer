#include "passes/InputDependencyReachabilityAnalysisPass.h"

#include "PDG/PDG/PDG.h"
#include "PDG/PDG/FunctionPDG.h"

#include "PDG/GraphBuilder.h"
#include "PDG/LLVMNode.h"
#include "passes/GraphBuilderPass.h"
#include "analysis/InputDependencyReachabilityAnalysis.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

namespace input_dependency {

void dumpInputDepReachability(const pdg::PDG& pdg)
{
    const auto& functionPdgs = pdg.getFunctionPDGs();
    for (const auto& fPDG : functionPdgs) {
        for (auto it = fPDG.second->nodesBegin(); it != fPDG.second->nodesEnd(); ++it) {
            auto node = llvm::dyn_cast<LLVMNode>(*it);
            if (node->getInputDepInfo().isInputDep()) {
                llvm::dbgs() << "   " << *node->getNodeValue() << "\n";
            }
        }
    }
    //llvm::dbgs() << "Function " << fPDG.getFunction()->getName() << "\n";
    //for (auto it = fPDG.nodesBegin(); it != fPDG.nodesEnd(); ++it) {
    //    auto node = llvm::dyn_cast<LLVMNode>(*it);
    //}
}

void InputDependencyReachabilityAnalysisPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<GraphBuilderPass>();
    AU.setPreservesAll();
}

bool InputDependencyReachabilityAnalysisPass::runOnModule(llvm::Module& M)
{
    auto pdg = getAnalysis<GraphBuilderPass>().getPDG();
    InputDependencyReachabilityAnalysis inputdepReachability(pdg);
    inputdepReachability.setNodeProcessor([] (ReachabilityAnalysis::NodeType ) {});
    inputdepReachability.analyze();
    dumpInputDepReachability(*pdg);
    return false;
}

char InputDependencyReachabilityAnalysisPass::ID = 0;
static llvm::RegisterPass<InputDependencyReachabilityAnalysisPass> X("inputdep-reachability","Perform input dependency reachability analysis");

} // namespace input_dependency

