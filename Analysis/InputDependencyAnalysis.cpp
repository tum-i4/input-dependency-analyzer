#include "InputDependencyAnalysis.h"

#include "IndirectCallSitesAnalysis.h"
#include "InputDepInstructionsRecorder.h"
#include "FunctionAnaliser.h"
#include "InputDepConfig.h"
#include "Utils.h"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/Analysis/PostDominators.h"
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

static llvm::cl::opt<bool> goto_unsafe(
    "goto-unsafe",
    llvm::cl::desc("Process irregular CFG in an unsafe way"),
    llvm::cl::value_desc("boolean flag"));

static llvm::cl::opt<std::string> libfunction_config(
    "lib-config",
    llvm::cl::desc("Configuration file for library functions"),
    llvm::cl::value_desc("file name"));

void configure_run()
{
    InputDepInstructionsRecorder::get().set_record();
    InputDepConfig().get().set_goto_unsafe(goto_unsafe);
    InputDepConfig().get().set_lib_config_file(libfunction_config);
}

char InputDependencyAnalysis::ID = 0;

bool InputDependencyAnalysis::runOnModule(llvm::Module& M)
{
    llvm::dbgs() << "Running input dependency analysis pass\n";
    configure_run();

    m_module = &M;
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
        FunctionAnaliser* f_analiser = pos->second->toFunctionAnalysisResult();
        return f_analiser;
    };

    const auto& indirectCallAnalysis = getAnalysis<IndirectCallSitesAnalysis>();
    const VirtualCallSiteAnalysisResult& virtualCallsInfo = indirectCallAnalysis.getVirtualsAnalysisResult();
    const IndirectCallSitesAnalysisResult& indirectCallsInfo = indirectCallAnalysis.getIndirectsAnalysisResult();
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
            if (F == nullptr || Utils::isLibraryFunction(F, &M)) {
                continue;
            }
            llvm::dbgs() << "Processing function " << F->getName() << "\n";
            m_moduleFunctions.insert(m_moduleFunctions.begin(), F);
            llvm::AAResults& AAR = AARGetter(*F);
            llvm::LoopInfo& LI = getAnalysis<llvm::LoopInfoWrapperPass>(*F).getLoopInfo();
            const llvm::PostDominatorTree& PDom = getAnalysis<llvm::PostDominatorTreeWrapperPass>(*F).getPostDomTree();
            const llvm::DominatorTree& dom = getAnalysis<llvm::DominatorTreeWrapperPass>(*F).getDomTree();
            InputDepResType analiser(new FunctionAnaliser(F, FAGetter));
            auto res = m_functionAnalisers.insert(std::make_pair(F, analiser));
            assert(res.second);
            auto analizer = res.first->second->toFunctionAnalysisResult();
            analizer->setAAResults(&AAR);
            analizer->setLoopInfo(&LI);
            analizer->setPostDomTree(&PDom);
            analizer->setDomTree(&dom);
            analizer->setVirtualCallSiteAnalysisResult(&virtualCallsInfo);
            analizer->setIndirectCallSiteAnalysisResult(&indirectCallsInfo);
            analizer->analize();
            const auto& calledFunctions = analizer->getCallSitesData();
            mergeCallSitesData(F, calledFunctions);
        }
        ++CGI;
    }
    doFinalization(CG);
    llvm::dbgs() << "Finished input dependency analysis\n\n";
    return false;
}

void InputDependencyAnalysis::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.setPreservesCFG();
    AU.addRequired<IndirectCallSitesAnalysis>();
    AU.addRequired<llvm::AssumptionCacheTracker>(); // otherwise run-time error
    llvm::getAAResultsAnalysisUsage(AU);
    AU.addRequired<llvm::CallGraphWrapperPass>();
    AU.addPreserved<llvm::CallGraphWrapperPass>();
    AU.addRequired<llvm::LoopInfoWrapperPass>();
    AU.addRequired<llvm::PostDominatorTreeWrapperPass>();
    AU.addRequired<llvm::DominatorTreeWrapperPass>();
    AU.setPreservesAll();
}

bool InputDependencyAnalysis::isInputDependent(llvm::Function* F, llvm::Instruction* instr) const
{
    auto pos = m_functionAnalisers.find(F);
    if (pos == m_functionAnalisers.end()) {
        return false;
        // or even exception
    }
    return pos->second->isInputDependent(instr);
}

bool InputDependencyAnalysis::isInputDependent(llvm::Instruction* instr) const
{
    auto* F = instr->getParent()->getParent();
    assert(F != nullptr);
    return isInputDependent(F, instr);
}

bool InputDependencyAnalysis::isInputDependent(llvm::BasicBlock* block) const
{
    auto F = block->getParent();
    auto pos = m_functionAnalisers.find(F);
    if (pos == m_functionAnalisers.end()) {
        return false;
    }
    return pos->second->isInputDependentBlock(block);
}

InputDependencyAnalysis::InputDepResType InputDependencyAnalysis::getAnalysisInfo(llvm::Function* F)
{
    auto pos = m_functionAnalisers.find(F);
    if (pos == m_functionAnalisers.end()) {
        return nullptr;
    }
    return pos->second;
}

const InputDependencyAnalysis::InputDepResType InputDependencyAnalysis::getAnalysisInfo(llvm::Function* F) const
{
    auto pos = m_functionAnalisers.find(F);
    if (pos == m_functionAnalisers.end()) {
        return nullptr;
    }
    return pos->second;
}

bool InputDependencyAnalysis::insertAnalysisInfo(llvm::Function* F, InputDepResType analysis_info)
{
    auto pos = m_functionAnalisers.find(F);
    if (pos != m_functionAnalisers.end()) {
        return false;
    }
    m_functionAnalisers.insert(std::make_pair(F, analysis_info));
    return true;
}

// doFinalize is called once
bool InputDependencyAnalysis::doFinalization(llvm::CallGraph &CG)
{
    for (auto F : m_moduleFunctions) {
        auto pos = m_functionAnalisers.find(F);
        if (pos == m_functionAnalisers.end()) {
            // log message
            continue;
        }
        llvm::dbgs() << "Finalizing " << F->getName() << "\n";
        finalizeForGlobals(F, pos->second);
        finalizeForArguments(F, pos->second);
    }
    return true;
}

void InputDependencyAnalysis::finalizeForArguments(llvm::Function* F, InputDepResType& FA)
{
    auto f_analiser = FA->toFunctionAnalysisResult();
    if (!f_analiser) {
        return;
    }

    if (m_calleeCallersInfo.find(F) == m_calleeCallersInfo.end()) {
        auto& arguments = F->getArgumentList();
        DependencyAnaliser::ArgumentDependenciesMap arg_deps;
        for (auto& arg : arguments) {
            arg_deps.insert(std::make_pair(&arg, ValueDepInfo(arg.getType(), DepInfo(DepInfo::INPUT_DEP))));
        }
        f_analiser->finalizeArguments(arg_deps);
        return;
    }
    const auto& callInfo = getFunctionCallInfo(F);
    f_analiser->finalizeArguments(callInfo);
}

void InputDependencyAnalysis::finalizeForGlobals(llvm::Function* F, InputDepResType& FA)
{
    auto f_analiser = FA->toFunctionAnalysisResult();
    if (!f_analiser) {
        return;
    }
    const auto& globalsInfo = getFunctionCallGlobalsInfo(F);
    f_analiser->finalizeGlobals(globalsInfo);
}

void InputDependencyAnalysis::mergeCallSitesData(llvm::Function* caller, const FunctionSet& calledFunctions)
{
    for (const auto& F : calledFunctions) {
        m_calleeCallersInfo[F].insert(caller);
    }
}

DependencyAnaliser::ArgumentDependenciesMap InputDependencyAnalysis::getFunctionCallInfo(llvm::Function* F)
{
    DependencyAnaliser::ArgumentDependenciesMap argDeps;
    auto pos = m_calleeCallersInfo.find(F);
    assert(pos != m_calleeCallersInfo.end());
    const auto& callers = pos->second;
    for (const auto& caller : callers) {
        auto fpos = m_functionAnalisers.find(caller);
        assert(fpos != m_functionAnalisers.end());
        auto f_analiser = fpos->second->toFunctionAnalysisResult();
        if (!f_analiser) {
            // assert?
            continue;
        }
        auto callInfo = f_analiser->getCallArgumentInfo(F);
        if (!f_analiser->areArgumentsFinalized()) {
            // if callee is finalized before caller, means caller was analized before callee.
            // this on its turn means callee callArgumentDeps should be input dep.
            // proper fix would be making sure caller is finalized before callee
            for (auto& item : callInfo) {
                if (item.second.isValueDep() || item.second.isInputArgumentDep()) {
                    item.second = ValueDepInfo(DepInfo(DepInfo::INPUT_DEP));
                }
            }
        }
        mergeDependencyMaps(argDeps, callInfo);
    }
    return argDeps;
}

DependencyAnaliser::GlobalVariableDependencyMap InputDependencyAnalysis::getFunctionCallGlobalsInfo(llvm::Function* F)
{
    DependencyAnaliser::GlobalVariableDependencyMap globalDeps;
    auto pos = m_calleeCallersInfo.find(F);
    if (pos == m_calleeCallersInfo.end()) {
        addMissingGlobalsInfo(F, globalDeps);
        return globalDeps;

    }
    assert(pos != m_calleeCallersInfo.end());
    const auto& callers = pos->second;
    for (const auto& caller : callers) {
        auto fpos = m_functionAnalisers.find(caller);
        assert(fpos != m_functionAnalisers.end());
        auto f_analiser = fpos->second->toFunctionAnalysisResult();
        if (!f_analiser) {
            continue;
        }
        auto globalsInfo = f_analiser->getCallGlobalsInfo(F);
        if (!f_analiser->areGlobalsFinalized()) {
            // See comment in getFunctionCallInfo
            for (auto& item : globalsInfo) {
                item.second = ValueDepInfo(DepInfo(DepInfo::INPUT_DEP));
            }
        }
        mergeDependencyMaps(globalDeps, globalsInfo);
    }
    addMissingGlobalsInfo(F, globalDeps);
    return globalDeps;
}

template <class DependencyMapType>
void InputDependencyAnalysis::mergeDependencyMaps(DependencyMapType& mergeTo, const DependencyMapType& mergeFrom)
{
    for (const auto item : mergeFrom) {
        // only input dependent arguments were collected
        assert(item.second.isDefined());
        //assert(item.second.isInputDep() || item.second.isInputIndep() || item.second.isInputArgumentDep());
        auto res = mergeTo.insert(item);
        if (!res.second) {
            res.first->second.mergeDependencies(item.second);
        }
        assert(!res.first->second.isValueDep());
    }
}

void InputDependencyAnalysis::addMissingGlobalsInfo(llvm::Function* F, DependencyAnaliser::GlobalVariableDependencyMap& globalDeps)
{
    const std::string globalInitF("__cxx_global_var_init");
    llvm::Function* initF = m_module->getFunction(globalInitF);
    InputDependencyAnalysisInfo::iterator initFpos = m_functionAnalisers.end();
    if (initF) {
        initFpos = m_functionAnalisers.find(initF);
    }

    auto pos = m_functionAnalisers.find(F);
    assert(pos != m_functionAnalisers.end());
    auto f_analiser = pos->second->toFunctionAnalysisResult();
    if (!f_analiser) {
        return;
    }
    const auto& referencedGlobals = f_analiser->getReferencedGlobals();
    for (const auto& global : referencedGlobals) {
        if (globalDeps.find(global) != globalDeps.end()) {
            continue;
        }
        if (initFpos != m_functionAnalisers.end()) {
            auto initF_analiser = initFpos->second->toFunctionAnalysisResult();
            if (initF_analiser->hasGlobalVariableDepInfo(global)) {
                globalDeps[global] = initF_analiser->getGlobalVariableDependencies(global);
                continue;
            }
        }
        globalDeps[global] = ValueDepInfo(global->getType(), DepInfo(DepInfo::INPUT_INDEP));
    }
}

static llvm::RegisterPass<InputDependencyAnalysis> X("input-dep","runs input dependency analysis");

}

