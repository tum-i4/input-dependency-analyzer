#include "passes/InputDependencyAnalysisPass.h"

#include "PDG/GraphBuilder.h"
#include "PDG/LLVMNode.h"
#include "analysis/InputDependencyAnalysis.h"
#include "analysis/InputDependencyStatistics.h"
#include "analysis/InputDepConfig.h"
#include "passes/GraphBuilderPass.h"

#include "PDG/PDG/PDG.h"
#include "PDG/PDG/FunctionPDG.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Module.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"


namespace input_dependency {

static llvm::cl::opt<std::string> libfunction_config(
    "lib-config",
    llvm::cl::desc("Configuration file for library functions"),
    llvm::cl::value_desc("file name"));

// TODO: add other cmd line options too

static llvm::cl::opt<bool> stats(
    "input-dep-stats",
    llvm::cl::desc("Dump statistics"),
    llvm::cl::value_desc("boolean flag"));

static llvm::cl::opt<std::string> stats_format(
    "input-dep-stats-format",
    llvm::cl::desc("Statistics format"),
    llvm::cl::value_desc("format name"));

static llvm::cl::opt<std::string> stats_file(
    "iput-dep-stats-file",
    llvm::cl::desc("Statistics file"),
    llvm::cl::value_desc("file name"));

void configure_run()
{
    InputDepConfig::get().set_lib_config_file(libfunction_config);
}

void InputDependencyAnalysisPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.setPreservesCFG();
    AU.addRequired<GraphBuilderPass>();
    AU.addRequired<llvm::CallGraphWrapperPass>();
    AU.addPreserved<llvm::CallGraphWrapperPass>();
    AU.addRequired<llvm::LoopInfoWrapperPass>();
    AU.setPreservesAll();
}

bool InputDependencyAnalysisPass::runOnModule(llvm::Module& M)
{
    configure_run();
    auto pdg = getAnalysis<GraphBuilderPass>().getPDG();
    llvm::CallGraph* CG = &getAnalysis<llvm::CallGraphWrapperPass>().getCallGraph();
    InputDependencyAnalysis* inputDepAnalysis = new InputDependencyAnalysis(&M);
    inputDepAnalysis->setPDG(pdg);
    inputDepAnalysis->setCallGraph(CG);
    m_inputDepAnalysisRes.reset(inputDepAnalysis);
    m_inputDepAnalysisRes->analyze();

    if (stats) {
        dump_statistics(&M);
    }
    return false;
}

void InputDependencyAnalysisPass::dump_statistics(llvm::Module* M)
{
    std::string file_name = stats_file;
    if (file_name.empty()) {
        file_name = "stats.json";
    }
    const auto& loopInfoGetter = [this] (llvm::Function* F)
    {
        return &this->getAnalysis<llvm::LoopInfoWrapperPass>(*F).getLoopInfo();
    };

    InputDependencyStatistics stats(stats_format, file_name,
                                    M,
                                    m_inputDepAnalysisRes.get());
    stats.setLoopInfoGetter(loopInfoGetter);
    stats.setSectionName("inputdep_stats");
    stats.report();
    stats.flush();
}

char InputDependencyAnalysisPass::ID = 0;
static llvm::RegisterPass<InputDependencyAnalysisPass> X("input-dependency","Perform input dependency reachability analysis");


} // namespace input_dependency

