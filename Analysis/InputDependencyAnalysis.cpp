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
                continue;
            }
            m_moduleFunctions.insert(m_moduleFunctions.begin(), F);
            llvm::AAResults& AAR = AARGetter(*F);
            llvm::LoopInfo& LI = getAnalysis<llvm::LoopInfoWrapperPass>(*F).getLoopInfo();
            auto res = m_functionAnalisers.insert(std::make_pair(F, FunctionAnaliser(F, AAR, LI, FAGetter)));
            assert(res.second);
            auto& analizer = res.first->second;
            analizer.analize();
            const auto& calledFunctions = analizer.getCallSitesData();
            mergeCallSitesData(F, calledFunctions);
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
    //for (const auto& item : m_functionAnalisers) {
    //    item.second.dump();
    //}

    for (auto F : m_moduleFunctions) {
        auto pos = m_functionAnalisers.find(F);
        if (pos != m_functionAnalisers.end()) {
            if (m_calleeCallersInfo.find(F) == m_calleeCallersInfo.end()) {
                continue;
            }
            const auto& callInfo = getFunctionCallInfo(F);
            pos->second.finalize(callInfo);
        }
    }

    //for (const auto& item : m_functionAnalisers) {
    //    item.second.dump();
    //}
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

void InputDependencyAnalysis::mergeCallSitesData(llvm::Function* caller, const FunctionSet& calledFunctions)
{
    for (const auto& F : calledFunctions) {
        m_calleeCallersInfo[F].insert(caller);
    }
}

void InputDependencyAnalysis::mergeArgumentDependencies(DependencyAnaliser::ArgumentDependenciesMap& mergeTo,
                                                        const DependencyAnaliser::ArgumentDependenciesMap& mergeFrom)
{
    for (const auto item : mergeFrom) {
        // only input dependent arguments were collected
        assert(item.second.isInputDep() || item.second.isInputIndep() || item.second.isInputArgumentDep());
        auto res = mergeTo.insert(item);
        if (res.second) {
            continue;
        }
        res.first->second.mergeDependencies(item.second);
        assert(res.first->second.isInputDep() || res.first->second.isInputArgumentDep());
    }
}

DependencyAnaliser::ArgumentDependenciesMap InputDependencyAnalysis::getFunctionCallInfo(llvm::Function* F)
{
    DependencyAnaliser::ArgumentDependenciesMap argDeps;
    auto pos = m_calleeCallersInfo.find(F);
    const auto& callers = pos->second;
    for (const auto& caller : callers) {
        auto fpos = m_functionAnalisers.find(caller);
        assert(fpos != m_functionAnalisers.end());
        auto callInfo = fpos->second.getCallArgumentInfo(F);
        mergeArgumentDependencies(argDeps, callInfo);
    }

    return argDeps;
}

static llvm::RegisterPass<InputDependencyAnalysis> X("input-dep","runs input dependency analysis");

}

