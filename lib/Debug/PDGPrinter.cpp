#include "llvm/Pass.h"

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

#include "PDG/SVFGDefUseAnalysisResults.h"
#include "PDG/LLVMMemorySSADefUseAnalysisResults.h"
#include "PDG/LLVMDominanceTree.h"
#include "PDG/PDGBuilder.h"
#include "PDG/PDGGraphTraits.h"
#include "analysis/SVFGIndirectCallSiteResults.h"

#include "SVF/MSSA/SVFG.h"
#include "SVF/MSSA/SVFGBuilder.h"
#include "SVF/Util/SVFModule.h"
#include "SVF/MemoryModel/PointerAnalysis.h"
#include "SVF/WPA/Andersen.h"

#include <fstream>

class PDGPrinterPass : public llvm::ModulePass
{
public:
    static char ID;
    PDGPrinterPass()
        : llvm::ModulePass(ID)
    {
    }

    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override
    {
        AU.addRequired<llvm::AssumptionCacheTracker>(); // otherwise run-time error
        llvm::getAAResultsAnalysisUsage(AU);
        AU.addRequiredTransitive<llvm::MemorySSAWrapperPass>();
        AU.addRequired<llvm::PostDominatorTreeWrapperPass>();
        AU.addRequired<llvm::DominatorTreeWrapperPass>();
        AU.setPreservesAll();
    }

    bool runOnModule(llvm::Module& M) override
    {
        auto memSSAGetter = [this] (llvm::Function* F) -> llvm::MemorySSA* {
            return &this->getAnalysis<llvm::MemorySSAWrapperPass>(*F).getMSSA();
        };
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
        AndersenWaveDiff* ander = new AndersenWaveDiff();
        ander->disablePrintStat();
        ander->analyze(svfM);
        SVFGBuilder memSSA(true);
        SVFG *svfg = memSSA.buildSVFG((BVDataPTAImpl*)ander);

        using DefUseResultsTy = PDGBuilder::DefUseResultsTy;
        using IndCSResultsTy = PDGBuilder::IndCSResultsTy;
        using DominanceResultsTy = PDGBuilder::DominanceResultsTy;
        DefUseResultsTy pointerDefUse = DefUseResultsTy(new SVFGDefUseAnalysisResults(svfg));
        DefUseResultsTy scalarDefUse = DefUseResultsTy(
                new LLVMMemorySSADefUseAnalysisResults(memSSAGetter, aliasAnalysisResGetter));
        IndCSResultsTy indCSRes = IndCSResultsTy(new
                input_dependency::SVFGIndirectCallSiteResults(ander->getPTACallGraph()));
        DominanceResultsTy domResults = DominanceResultsTy(new LLVMDominanceTree(domTreeGetter,
                                                                                 postdomTreeGetter));

        pdg::PDGBuilder pdgBuilder(&M);
        pdgBuilder.setPointerDesUseResults(pointerDefUse);
        pdgBuilder.setScalarDesUseResults(scalarDefUse);
        pdgBuilder.setIndirectCallSitesResults(indCSRes);
        pdgBuilder.setDominanceResults(domResults);
        pdgBuilder.build();

        auto pdg = pdgBuilder.getPDG();
        for (auto& F : M) {
            if (!pdg->hasFunctionPDG(&F)) {
                llvm::dbgs() << "Function does not have pdg " << F.getName() << "\n";
                continue;
            }
            auto functionPDG = pdg->getFunctionPDG(&F);
            pdg::FunctionPDG* Graph = functionPDG.get();
            std::string Filename = "cfg." + F.getName().str() + ".dot";
            std::error_code EC;
            llvm::errs() << "Writing '" << Filename << "'...";
            llvm::raw_fd_ostream File(Filename, EC, llvm::sys::fs::F_Text);
            std::string GraphName = llvm::DOTGraphTraits<pdg::FunctionPDG*>::getGraphName(Graph);
            std::string Title = GraphName + " for '" + F.getName().str() + "' function";
            if (!EC) {
                llvm::WriteGraph(File, Graph, false, Title);
            } else {
                llvm::errs() << "  error opening file for writing!";
            }
            llvm::errs() << "\n";
        }
        return false;
    }

}; // class PDGPrinterPass

char PDGPrinterPass::ID = 0;
static llvm::RegisterPass<PDGPrinterPass> X("dump-pdg","Dump pdg in dot format");
