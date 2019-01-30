#include "passes/GraphBuilderPass.h"

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

#include "PDG/PDG/PDG.h"
#include "PDG/PDG/FunctionPDG.h"
#include "PDG/PDG/SVFGDefUseAnalysisResults.h"
#include "PDG/PDG/LLVMMemorySSADefUseAnalysisResults.h"
#include "PDG/PDG/DGDefUseAnalysisResults.h"
#include "PDG/PDG/LLVMDominanceTree.h"
#include "PDG/PDG/PDGGraphTraits.h"
#include "PDG/PDG/SVFGIndirectCallSiteResults.h"

#include "PDG/GraphBuilder.h"

#include "SVF/MSSA/SVFG.h"
#include "SVF/MSSA/SVFGBuilder.h"
#include "SVF/Util/SVFModule.h"
#include "SVF/MemoryModel/PointerAnalysis.h"
#include "SVF/WPA/Andersen.h"
#include "SVF/PDG/PDGPointerAnalysis.h"


namespace input_dependency {

void GraphBuilderPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<llvm::AssumptionCacheTracker>(); // otherwise run-time error
    llvm::getAAResultsAnalysisUsage(AU);
    AU.addRequired<llvm::PostDominatorTreeWrapperPass>();
    AU.addRequired<llvm::DominatorTreeWrapperPass>();
    AU.setPreservesAll();
}

bool GraphBuilderPass::runOnModule(llvm::Module& M)
{
    llvm::Optional<llvm::BasicAAResult> BAR;
    llvm::Optional<llvm::AAResults> AAR;
    auto AARGetter = [&](llvm::Function* F) -> llvm::AAResults* {
        BAR.emplace(llvm::createLegacyPMBasicAAResult(*this, *F));
        AAR.emplace(llvm::createLegacyPMAAResults(*this, *F, *BAR));
        return &*AAR;
    };
    std::unordered_map<llvm::Function*, llvm::AAResults*> functionAAResults;
    for (auto& F : M) {
        if (!F.isDeclaration()) {
            functionAAResults.insert(std::make_pair(&F, AARGetter(&F)));
        }
    }

    auto domTreeGetter = [&] (llvm::Function* F) {
        return &this->getAnalysis<llvm::DominatorTreeWrapperPass>(*F).getDomTree();
    };
    auto postdomTreeGetter = [&] (llvm::Function* F) {
        return &this->getAnalysis<llvm::PostDominatorTreeWrapperPass>(*F).getPostDomTree();
    };

    // Passing AARGetter to LLVMMemorySSADefUseAnalysisResults causes segmentation fault when requesting AAR
    // use following functional as a workaround before the problem with AARGetter is found
    auto aliasAnalysisResGetter = [&functionAAResults] (llvm::Function* F) {
        return functionAAResults[F];
    };

    SVFModule svfM(M);
    AndersenWaveDiff* ander = new svfg::PDGAndersenWaveDiff();
    ander->disablePrintStat();
    ander->analyze(svfM);
    SVFGBuilder memSSA(true);
    SVFG *svfg = memSSA.buildSVFG((BVDataPTAImpl*)ander);

    using DefUseResultsTy = pdg::PDGBuilder::DefUseResultsTy;
    using IndCSResultsTy = pdg::PDGBuilder::IndCSResultsTy;
    using DominanceResultsTy = pdg::PDGBuilder::DominanceResultsTy;
    DefUseResultsTy defUse = DefUseResultsTy(new pdg::SVFGDefUseAnalysisResults(svfg));
    IndCSResultsTy indCSRes = IndCSResultsTy(new
            pdg::SVFGIndirectCallSiteResults(ander->getPTACallGraph()));
    DominanceResultsTy domResults = DominanceResultsTy(new pdg::LLVMDominanceTree(domTreeGetter,
                postdomTreeGetter));

    input_dependency::GraphBuilder pdgBuilder(&M);
    pdgBuilder.setDesUseResults(defUse);
    pdgBuilder.setIndirectCallSitesResults(indCSRes);
    pdgBuilder.setDominanceResults(domResults);
    pdgBuilder.build();

    m_pdg = pdgBuilder.getPDG();
    return false;
}

char GraphBuilderPass::ID = 0;
static llvm::RegisterPass<GraphBuilderPass> X("pdg-build","Build PDG graph");
} // namespace input_dependency

