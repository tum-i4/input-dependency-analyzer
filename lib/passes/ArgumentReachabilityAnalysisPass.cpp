#include "passes/ArgumentReachabilityAnalysisPass.h"

#include "PDG/PDG/PDG.h"
#include "PDG/PDG/FunctionPDG.h"

#include "PDG/GraphBuilder.h"
#include "PDG/LLVMNode.h"
#include "passes/GraphBuilderPass.h"
#include "analysis/ArgumentReachabilityAnalysis.h"

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

void dumpArgumentReachability(const pdg::FunctionPDG& fPDG)
{
    llvm::dbgs() << "Function " << fPDG.getFunction()->getName() << "\n";
    for (auto it = fPDG.nodesBegin(); it != fPDG.nodesEnd(); ++it) {
        auto node = llvm::dyn_cast<LLVMNode>(*it);
        if (node->getInputDepInfo().isArgumentDep()) {
            llvm::dbgs() << "   " << *node->getNodeValue() << "\n";
        }
    }
}

void ArgumentReachabilityAnalysisPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<GraphBuilderPass>();
    AU.setPreservesAll();
}

bool ArgumentReachabilityAnalysisPass::runOnModule(llvm::Module& M)
{
    auto pdg = getAnalysis<GraphBuilderPass>().getPDG();
    for (auto& F : M) {
        assert(pdg->hasFunctionPDG(&F));
        auto fPDG = pdg->getFunctionPDG(&F);
        ArgumentReachabilityAnalysis argReachability(fPDG);
        argReachability.analyze();
        dumpArgumentReachability(*fPDG.get());
    }
    return false;
}

char ArgumentReachabilityAnalysisPass::ID = 0;
static llvm::RegisterPass<ArgumentReachabilityAnalysisPass> X("arg-reachability","Perform argument reachability analysis");

} // namespace input_dependency

