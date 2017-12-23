#include "InputDependencyAnalysisPass.h"

#include "InputDependencyAnalysis.h"
#include "CachedInputDependencyAnalysis.h"
#include "InputDependencyStatistics.h"
#include "IndirectCallSitesAnalysis.h"
#include "InputDepConfig.h"
#include "InputDepInstructionsRecorder.h"
#include "constants.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/PassRegistry.h"
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

static llvm::cl::opt<bool> stats(
    "dependency-stats",
    llvm::cl::desc("Dump statistics"),
    llvm::cl::value_desc("boolean flag"));

static llvm::cl::opt<std::string> stats_format(
    "dependency-stats-format",
    llvm::cl::desc("Statistics format"),
    llvm::cl::value_desc("format name"));

static llvm::cl::opt<std::string> stats_file(
    "dependency-stats-file",
    llvm::cl::desc("Statistics file"),
    llvm::cl::value_desc("file name"));

static llvm::cl::opt<bool> cache(
    "transparent-caching",
    llvm::cl::desc("Cache input dependency results"),
    llvm::cl::value_desc("boolean flag"));

void configure_run()
{
    InputDepInstructionsRecorder::get().set_record();
    InputDepConfig::get().set_goto_unsafe(goto_unsafe);
    InputDepConfig::get().set_lib_config_file(libfunction_config);
    InputDepConfig::get().set_cache_input_dependency(cache);
}

char InputDependencyAnalysisPass::ID = 0;

bool InputDependencyAnalysisPass::runOnModule(llvm::Module& M)
{
    llvm::dbgs() << "Running input dependency analysis pass\n";
    configure_run();
    m_module = &M;

    llvm::Optional<llvm::BasicAAResult> BAR;
    llvm::Optional<llvm::AAResults> AAR;
    auto AARGetter = [&](llvm::Function* F) -> llvm::AAResults* {
        BAR.emplace(llvm::createLegacyPMBasicAAResult(*this, *F));
        AAR.emplace(llvm::createLegacyPMAAResults(*this, *F, *BAR));
        return &*AAR;
    };

    if (cache && has_cached_input_dependency()) {
        create_cached_input_dependency_analysis();
    } else {
        create_input_dependency_analysis(AARGetter);
    }

    m_analysis->run();

    if (InputDepConfig::get().is_cache_input_dep()) {
        m_analysis->cache();
    }
    if (stats) {
        dump_statistics();
    }

    return false;
}

void InputDependencyAnalysisPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
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

bool InputDependencyAnalysisPass::has_cached_input_dependency() const
{
    bool is_cached = false;
    if (llvm::Metadata* flag = m_module->getModuleFlag(metadata_strings::cached_input_dep)) {
        if (auto* constAsMd = llvm::dyn_cast<llvm::ConstantAsMetadata>(flag)) {
            if (llvm::Value* val = constAsMd->getValue()) {
                if (auto* constInt = llvm::dyn_cast<llvm::ConstantInt>(val)) {
                    is_cached = constInt->getValue().getBoolValue();
                }
            }
        }
    }
    return is_cached;
}

void InputDependencyAnalysisPass::create_input_dependency_analysis(const InputDependencyAnalysisInterface::AliasAnalysisInfoGetter& AARGetter)
{
    llvm::CallGraph* CG = &getAnalysis<llvm::CallGraphWrapperPass>().getCallGraph();
    const auto& indirectCallAnalysis = getAnalysis<IndirectCallSitesAnalysis>();
    const VirtualCallSiteAnalysisResult* virtualCallsInfo = &indirectCallAnalysis.getVirtualsAnalysisResult();
    const IndirectCallSitesAnalysisResult* indirectCallsInfo = &indirectCallAnalysis.getIndirectsAnalysisResult();
    const auto& loopInfoGetter = [this] (llvm::Function* F)
    {
        return &this->getAnalysis<llvm::LoopInfoWrapperPass>(*F).getLoopInfo();
    };
    const auto& postDomTreeGetter = [this] (llvm::Function* F)
    {
        return &this->getAnalysis<llvm::PostDominatorTreeWrapperPass>(*F).getPostDomTree();
    };
    const auto& domTreeGetter = [this] (llvm::Function* F)
    {
        return &this->getAnalysis<llvm::DominatorTreeWrapperPass>(*F).getDomTree();
    };
    InputDependencyAnalysis* analysis = new InputDependencyAnalysis(m_module);
    analysis->setCallGraph(CG);
    analysis->setVirtualCallSiteAnalysisResult(virtualCallsInfo);
    analysis->setIndirectCallSiteAnalysisResult(indirectCallsInfo);
    analysis->setAliasAnalysisInfoGetter(AARGetter);
    analysis->setLoopInfoGetter(loopInfoGetter);
    analysis->setPostDominatorTreeGetter(postDomTreeGetter);
    analysis->setDominatorTreeGetter(domTreeGetter);
    m_analysis.reset(analysis);
}

void InputDependencyAnalysisPass::create_cached_input_dependency_analysis()
{
    m_analysis.reset(new CachedInputDependencyAnalysis(m_module));
}

void InputDependencyAnalysisPass::dump_statistics()
{
    std::string file_name = stats_file;
    if (file_name.empty()) {
        file_name = "stats";
    }
    InputDependencyStatistics stats(stats_format, file_name, m_module, &m_analysis->getAnalysisInfo());
    stats.setSectionName("inputdep_stats");
    stats.report();
    stats.flush();
}

static llvm::RegisterPass<InputDependencyAnalysisPass> X("input-dep","runs input dependency analysis");

}

