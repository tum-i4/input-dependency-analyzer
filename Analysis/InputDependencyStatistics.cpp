#include "InputDependencyStatistics.h"
#include "InputDependencyAnalysis.h"
#include "Utils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/PassRegistry.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <fstream>

namespace input_dependency {

namespace {

void print_stats(std::ofstream& strm, const std::string& function_name, unsigned deps, unsigned indeps, unsigned unknowns)
{
    strm << function_name << "\n";
    strm << "----------------------\n";
    strm << "Input Dependent instructions: " << deps << "\n";
    strm << "Input Independent instructions: " << indeps << "\n";
    strm << "Unknown instructions: " << unknowns << "\n";
    unsigned percent = (deps * 100) / (deps + indeps + unknowns);
    strm << "Input dependent instructions' percent: " << percent << "%\n";
}

}

void InputDependencyStatistics::report(llvm::Module& M, const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo) const
{
    unsigned module_dep_count = 0;
    unsigned module_indep_count = 0;
    unsigned module_unknown_count = 0;

    std::ofstream stats_strm("stats.txt", std::ofstream::out);
    for (auto& F : M) {
        auto FA_pos = inputDepInfo.find(&F);
        if (FA_pos == inputDepInfo.end()) {
            continue;
        }
        const auto& FA = FA_pos->second;
        unsigned dep_count = FA->get_input_dep_count();
        unsigned indep_count = FA->get_input_indep_count();
        unsigned unknown_count = FA->get_input_unknowns_count();
        print_stats(stats_strm, F.getName(), dep_count, indep_count, unknown_count);
        module_dep_count += dep_count;
        module_indep_count += indep_count;
        module_unknown_count += unknown_count;
    }
    print_stats(stats_strm, M.getName(), module_dep_count, module_indep_count, module_unknown_count);
}

char InputDependencyStatisticsPass::ID = 0;

void InputDependencyStatisticsPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<InputDependencyAnalysis>();
    AU.setPreservesAll();
}

bool InputDependencyStatisticsPass::runOnModule(llvm::Module& M)
{
    const auto& IDA = getAnalysis<InputDependencyAnalysis>();
    InputDependencyStatistics statistics;
    statistics.report(M, IDA.getAnalysisInfo());
    return false;
}

static llvm::RegisterPass<InputDependencyStatisticsPass> X("inputdep-statistics","runs input dependency analysis");

}

