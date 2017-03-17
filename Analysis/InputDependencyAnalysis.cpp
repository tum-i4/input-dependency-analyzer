#include "InputDependencyAnalysis.h"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/PassRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <cassert>

namespace input_dependency {

char InputDependencyAnalysis::ID = 0;

bool InputDependencyAnalysis::runOnModule(llvm::Module& M)
{
    llvm::Optional<llvm::BasicAAResult> BAR;
    llvm::Optional<llvm::AAResults> AAR;
    auto AARGetter = [&](llvm::Function &F) -> llvm::AAResults & {
        BAR.emplace(llvm::createLegacyPMBasicAAResult(*this, F));
        AAR.emplace(llvm::createLegacyPMAAResults(*this, F, *BAR));
        return *AAR;
    };
    auto FAGetter = [&] (llvm::Function* F) -> const FunctionAnaliser* {
        auto pos = m_functionAnalisers.find(F);
        if (pos == m_functionAnalisers.end()) {
            return nullptr;
        }
        return &pos->second;
    };


    llvm::CallGraph& CG = getAnalysis<llvm::CallGraphWrapperPass>().getCallGraph();

    llvm::scc_iterator<llvm::CallGraph*> CGI = llvm::scc_begin(&CG);
    llvm::CallGraphSCC CurSCC(CG, &CGI);
    while (!CGI.isAtEnd()) {
        // Copy the current SCC and increment past it so that the pass can hack
        // on the SCC if it wants to without invalidating our iterator.
        const std::vector<llvm::CallGraphNode *> &NodeVec = *CGI;
        CurSCC.initialize(NodeVec.data(), NodeVec.data() + NodeVec.size());

        for (llvm::CallGraphNode* node : CurSCC) {
            llvm::Function* F = node->getFunction();
            if (F == nullptr || F->isDeclaration() || F->getLinkage() == llvm::GlobalValue::LinkOnceODRLinkage) {
                if (F != nullptr) {
                    llvm::dbgs() << "skip function " << F->getName() << "\n";
                }
                continue;
            }
            llvm::dbgs() << "Processing function " << F->getName() << "\n";
            llvm::AAResults& AAR = AARGetter(*F);
            llvm::LoopInfo& LI = getAnalysis<llvm::LoopInfoWrapperPass>(*F).getLoopInfo();
            auto res = m_functionAnalisers.insert(std::make_pair(F, FunctionAnaliser(F, AAR, LI, FAGetter)));
            assert(res.second);
            auto& analizer = res.first->second;
            analizer.analize();
            const auto& callInfo = analizer.getCallSitesData();
            mergeFunctionsCallInfo(callInfo);
        }
        ++CGI;
    }
    doFinalization(CG);
    return false;
}

void InputDependencyAnalysis::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.setPreservesCFG();
    AU.setPreservesAll();
    AU.addRequired<llvm::AssumptionCacheTracker>(); // otherwise run-time error
    llvm::getAAResultsAnalysisUsage(AU);
    AU.addRequired<llvm::CallGraphWrapperPass>();
    AU.addPreserved<llvm::CallGraphWrapperPass>();
    AU.addRequired<llvm::LoopInfoWrapperPass>();
    AU.setPreservesAll();
}

// doFinalize is called once
bool InputDependencyAnalysis::doFinalization(llvm::CallGraph &CG)
{
    for (auto& item : m_functionsCallInfo) {
        llvm::dbgs() << "Finalizing " << item.first->getName() << "\n";
        auto pos = m_functionAnalisers.find(item.first);
        assert(pos != m_functionAnalisers.end());
        pos->second.finalize(item.second);
        //pos->second.dump();
    }
    for (auto& item : m_functionAnalisers) {
        item.second.dump();
        //if (item.first->getName() == "main") {
        //    item.second.dump();
        //}
    }
    return true;
}

bool InputDependencyAnalysis::isInputDependent(llvm::Function* F, llvm::Instruction* instr) const
{
    auto pos = m_functionAnalisers.find(F);
    if (pos == m_functionAnalisers.end()) {
        return false;
        // or even exception
    }
    return pos->second.isInputDependent(instr);
}

bool InputDependencyAnalysis::isInputDependent(llvm::Instruction* instr) const
{
    auto* F = instr->getParent()->getParent();
    assert(F != nullptr);
    return isInputDependent(F, instr);
}

void InputDependencyAnalysis::mergeFunctionsCallInfo(
        const DependencyAnaliser::FunctionArgumentsDependencies& newInfo)
{
    for (auto& newitem : newInfo) {
        //llvm::dbgs() << newitem.first->getName() << "\n";
        auto res = m_functionsCallInfo.insert(newitem);
        if (res.second) {
            continue;
        }
        for (auto& item : res.first->second) {
            auto pos = newitem.second.find(item.first);
            if (pos == newitem.second.end()) {
                continue;
            }
            const auto& newdeps = pos->second.getArgumentDependencies();
            auto& argumentDeps = item.second.getArgumentDependencies();
            argumentDeps.insert(newdeps.begin(), newdeps.end());
        }
    }
}

FunctionAnaliser* InputDependencyAnalysis::getAnalysisInfo(llvm::Function* F)
{
    auto pos = m_functionAnalisers.find(F);
    if (pos == m_functionAnalisers.end()) {
        return nullptr;
    }
    return &pos->second;
}

const FunctionAnaliser* InputDependencyAnalysis::getAnalysisInfo(llvm::Function* F) const
{
    auto pos = m_functionAnalisers.find(F);
    if (pos == m_functionAnalisers.end()) {
        return nullptr;
    }
    return &pos->second;
}


//static void registerInputSlicePasses(const llvm::PassManagerBuilder&,
//                                     llvm::legacy::PassManagerBase &PM)
//{
//    PM.add(new llvm::TargetLibraryInfoWrapperPass());
//    PM.add(new llvm::LoopInfoWrapperPass());
//    PM.add(new InputDependencyAnalysis());
//}
//
//static llvm::RegisterStandardPasses
//  RegisterMyPass(llvm::PassManagerBuilder::EP_ModuleOptimizerEarly,
//                 registerInputSlicePasses);
//
//static RegisterStandardPasses
//  RegisterMyPass1(PassManagerBuilder::EP_EnabledOnOptLevel0,
//                 registerInputSlicePasses);
//
//
static llvm::RegisterPass<InputDependencyAnalysis> X("input-dep","runs input dependency analysis");

}

