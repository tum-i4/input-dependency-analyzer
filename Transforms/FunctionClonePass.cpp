#include "FunctionClonePass.h"
#include "Utils.h"

#include "Analysis/FunctionAnaliser.h"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include "llvm/PassRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"


namespace oh {

namespace {

bool skip_function(llvm::Function* F, const std::unordered_set<llvm::Function*>& skip_set)
{
    return F->isDeclaration() || F->isIntrinsic() || (skip_set.find(F) != skip_set.end());
}

}

void CloneStatistics::report()
{
    write_entry(m_module_name, "NumOfClonnedInst", m_numOfClonnedInst);
    write_entry(m_module_name, "NumOfInstAfterCloning", m_numOfInstAfterCloning);
    write_entry(m_module_name, "NumOfInDepInstAfterCloning", m_numOfInDepInstAfterCloning);
    write_entry(m_module_name, "ClonnedFunctions", m_clonnedFuncs);
    flush();
}

static llvm::cl::opt<bool> stats(
    "clone-stats",
    llvm::cl::desc("Dump statistics"),
    llvm::cl::value_desc("boolean flag"));

static llvm::cl::opt<std::string> stats_format(
    "clone-stats-format",
    llvm::cl::desc("Statistics format"),
    llvm::cl::value_desc("format name"));

static llvm::cl::opt<std::string> stats_file(
    "clone-stats-file",
    llvm::cl::desc("Statistics file"),
    llvm::cl::value_desc("file name"));

char FunctionClonePass::ID = 0;

void FunctionClonePass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<input_dependency::InputDependencyAnalysis>();
    AU.addRequired<llvm::CallGraphWrapperPass>();
}

bool FunctionClonePass::runOnModule(llvm::Module& M)
{
    llvm::dbgs() << "Running function clonning transofrmation pass\n";
    bool isChanged = true;
    IDA = &getAnalysis<input_dependency::InputDependencyAnalysis>();

    createStatistics(M);
    m_coverageStatistics->setSectionName("input_indep_coverage_before_clonning");
    m_coverageStatistics->reportInputInDepCoverage();
    m_coverageStatistics->flush();

    auto it = M.begin();
    FunctionSet to_process;
    FunctionSet processed;
    while (it != M.end()) {
        auto F = &*it;
        ++it;
        if (skip_function(F, processed)) {
            continue;
        }
        to_process.insert(F);
        m_cloneStatistics->add_numOfInstAfterCloning(Utils::get_function_instrs_count(*F));
        while (!to_process.empty()) {
            auto pos = to_process.begin();
            auto& currentF = *pos;
            llvm::dbgs() << "Cloning functions called in " << currentF->getName() << "\n";
            processed.insert(currentF);

            auto f_analysisInfo = getFunctionInputDepInfo(currentF);
            if (f_analysisInfo == nullptr) {
                to_process.erase(currentF);
                continue;
            }
            m_cloneStatistics->add_numOfInDepInstAfterCloning(f_analysisInfo->get_input_indep_count());
            if (f_analysisInfo->isInputDepFunction()) {
                to_process.erase(currentF);
                continue;
            }
            const auto& callSites = f_analysisInfo->getCallSitesData();
            for (const auto& callSite : callSites) {
                if (callSite->isDeclaration() || callSite->isIntrinsic()) {
                    continue;
                }
                if (callSite->getArgumentList().empty()) {
                    // don't clone function with no argument
                    continue;
                }
                const auto& clonedFunctions = doClone(f_analysisInfo, callSite);
                to_process.insert(clonedFunctions.begin(), clonedFunctions.end());
            }
            to_process.erase(currentF);
        }
    }

    llvm::dbgs() << "Finished function clonning transofrmation\n\n";
    dump();
    m_coverageStatistics->setSectionName("input_indep_coverage_after_clonning");
    m_coverageStatistics->reportInputInDepCoverage();
    m_coverageStatistics->flush();
    m_cloneStatistics->report();
    return isChanged;
}

// Do clonning for the given called functions.
// Will clone for all sets of input dependent arguments.
// Returns set of clonned functions.
FunctionClonePass::FunctionSet FunctionClonePass::doClone(const InputDepRes& caller_analiser,
                                                          llvm::Function* calledF)
{
    llvm::dbgs() << "   Clone " << calledF->getName() << "\n";
    llvm::dbgs() << "---------------------------\n";
    FunctionSet clonedFunctions;

    // keep the value of actual calledF
    llvm::Function* cloned_calledF = calledF;
    // if calledF is a clone itself, get its original function information
    auto pos = m_clone_to_original.find(calledF);
    if (pos != m_clone_to_original.end()) {
        calledF = pos->second;
    }
    auto calledFunctionAnaliser = getFunctionInputDepInfo(calledF);
    if (!calledFunctionAnaliser) {
        return clonedFunctions;
    }
    auto emplace_res = m_functionCloneInfo.emplace(calledF, FunctionClone(calledF));
    auto& clone = emplace_res.first->second;

    auto functionCallDepInfo = caller_analiser->getFunctionCallDepInfo(cloned_calledF);
    auto& callArgDeps = functionCallDepInfo.getCallsArgumentDependencies();

    for (auto& argDepItem : callArgDeps) {
        const llvm::BasicBlock* callsite_block = argDepItem.first->getParent();
        if (caller_analiser->isInputDependentBlock(const_cast<llvm::BasicBlock*>(callsite_block))) {
            continue;
        }
        //llvm::dbgs() << "   Clone for call site " << *argDepItem.first << "\n";
        auto clone_res = doCloneForArguments(calledF, calledFunctionAnaliser, clone, argDepItem.second);
        if (!clone_res.first && !clone_res.second) {
            continue;
        }
        auto F = clone_res.first;
        if (clone_res.second) {
            auto new_clone = m_clone_to_original.insert(std::make_pair(F, calledF));
            assert(new_clone.second);
            clonedFunctions.insert(F);
            // add to analysis info
        }
        if (cloned_calledF != F) {
            //llvm::dbgs() << "   Change call site to call cloned function\n";
            caller_analiser->changeFunctionCall(argDepItem.first, cloned_calledF, F);
        }
    }
    //llvm::dbgs() << "\n";
    return clonedFunctions;
}

input_dependency::InputDependencyAnalysis::InputDepResType FunctionClonePass::getFunctionInputDepInfo(llvm::Function* F) const
{
    input_dependency::InputDependencyAnalysis::InputDepResType analysisInfo = IDA->getAnalysisInfo(F);
    if (!analysisInfo) {
        //llvm::dbgs() << "No input dep info for " << F->getName() << "\n";
        return nullptr;
    }
    return analysisInfo;
}

std::pair<llvm::Function*, bool> FunctionClonePass::doCloneForArguments(
                                                       llvm::Function* calledF,
                                                       InputDepRes original_analiser,
                                                       FunctionClone& clone,
                                                       const input_dependency::FunctionCallDepInfo::ArgumentDependenciesMap& argDeps)
{
    const FunctionClone::mask& mask = FunctionClone::createMaskForCall(argDeps,
                                                                       calledF->getArgumentList().size(),
                                                                       calledF->isVarArg());
    // no need to clone for all input dep arguments
    if (std::all_of(mask.begin(), mask.end(), [] (bool b) {return b;})) {
        return std::make_pair(nullptr, false);
    }
    //llvm::dbgs() << "   Argument dependency mask is: " << FunctionClone::mask_to_string(mask) << "\n";
    llvm::Function* F = nullptr;
    if (clone.hasCloneForMask(mask)) {
        F = clone.getClonedFunction(mask);
        //llvm::dbgs() << "   Has clone for mask " << F->getName() << ". reuse..\n";
        return std::make_pair(F, false);
    }
    auto original_f_analiser = original_analiser->toFunctionAnalysisResult();
    if (!original_f_analiser) {
        // no cloning for already cloned function or for extracted function.
        return std::make_pair(nullptr, false);
    }
    InputDepRes cloned_analiser(original_f_analiser->cloneForArguments(argDeps));
    // call sites at input dep blocks are filtered out, thus if we got to this point, means call site is input indep
    cloned_analiser->setIsInputDepFunction(false);
    F = cloned_analiser->getFunction();
    std::string newName = calledF->getName();
    newName += FunctionClone::mask_to_string(mask);
    F->setName(newName);
    clone.addClone(mask, F);
    bool add_to_input_dep = IDA->insertAnalysisInfo(F, cloned_analiser);
    m_cloneStatistics->add_numOfInDepInstAfterCloning(cloned_analiser->get_input_indep_count());
    m_cloneStatistics->add_numOfClonnedInst(Utils::get_function_instrs_count(*F));
    m_cloneStatistics->add_numOfInstAfterCloning(Utils::get_function_instrs_count(*F));
    m_cloneStatistics->add_clonnedFunction(F->getName());
    return std::make_pair(F, true);
}

void FunctionClonePass::createStatistics(llvm::Module& M)
{
    if (!stats) {
        m_cloneStatistics = CloneStatisticsType(new DummyCloneStatistics());
        m_coverageStatistics = CoverageStatisticsType(new input_dependency::DummyInputDependencyStatistics());
        return;
    }
    std::string file_name = stats_file;
    if (file_name.empty()) {
        file_name = "stats";
    }
    m_cloneStatistics = CloneStatisticsType(new CloneStatistics(M.getName(), stats_format, file_name));
    m_cloneStatistics->setSectionName("clone_stats");
    m_coverageStatistics = CoverageStatisticsType(new input_dependency::InputDependencyStatistics(stats_format, file_name, &M,
                                                                          &IDA->getAnalysisInfo()));
}

void FunctionClonePass::dump() const
{
    llvm::dbgs() << "Clonning transformation results\n";
    for (const auto& clone : m_functionCloneInfo) {
        clone.second.dump();
    }
}

static llvm::RegisterPass<FunctionClonePass> X(
                                "clone-functions",
                                "Transformation pass to duplicate functions with different set ot input dependent arguments");
}

