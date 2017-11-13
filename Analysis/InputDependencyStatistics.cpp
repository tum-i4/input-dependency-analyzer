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

unsigned get_function_instrs_count(llvm::Function& F)
{
    unsigned count = 0;
    for (auto& B : F) {
        count += B.getInstList().size();
    }
    return count;
}

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

void print_coverage_stats(std::ofstream& strm, const std::string& name,
                          const std::string unit, unsigned indep_count,
                          unsigned unreachable, unsigned whole)
{
    strm << unit << " coverage for " << name << " ----- ";
    unsigned coverage = (indep_count * 100) / (whole - unreachable);
    strm << coverage << "\n";
}

InputDependencyStatisticsPass::Type string_to_stats_type(const std::string& type_str)
{
    if (type_str == "input-dep") {
        return InputDependencyStatisticsPass::INPUT_DEP;
    } else if (type_str == "coverage") {
        return InputDependencyStatisticsPass::COVERAGE;
    }
    return InputDependencyStatisticsPass::UNKNOWN;
}

} // unnamed namespace


void CoverageStatistics::report(llvm::Module& M, const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo) const
{
    unsigned module_indep_blocks_count = 0;
    unsigned module_unreachable_blocks_count = 0;
    unsigned module_blocks_count = 0;
    unsigned module_indep_instrs_count = 0;
    unsigned module_unreachable_instrs_count = 0;
    unsigned module_instrs_count = 0;

    std::ofstream stats_strm("coverage_stats.txt", std::ofstream::out);
    for (auto& F : M) {
        auto FA_pos = inputDepInfo.find(&F);
        if (FA_pos == inputDepInfo.end()) {
            continue;
        }
        const auto& FA = FA_pos->second;
        unsigned indep_count = FA->get_input_indep_blocks_count();
        unsigned unreachable = FA->get_unreachable_blocks_count();
        unsigned blocks = F.getBasicBlockList().size();
        unsigned indep_instrs_count = FA->get_input_indep_count();
        unsigned unreachable_instrs = FA->get_unreachable_instructions_count();
        unsigned instructions = get_function_instrs_count(F);
        print_coverage_stats(stats_strm, F.getName(), "basic block", indep_count, unreachable, blocks);
        print_coverage_stats(stats_strm, F.getName(), "instructions", indep_instrs_count, unreachable_instrs, instructions);
        module_indep_blocks_count += indep_count;
        module_unreachable_blocks_count += unreachable;
        module_blocks_count += blocks;
        module_indep_instrs_count += indep_instrs_count;
        module_unreachable_instrs_count += unreachable_instrs;
        module_instrs_count += instructions;
    }
    print_coverage_stats(stats_strm, M.getName(), "basic block",
                         module_indep_blocks_count,
                         module_unreachable_blocks_count,
                         module_blocks_count);
    print_coverage_stats(stats_strm, M.getName(), "instructions",
                         module_indep_instrs_count,
                         module_unreachable_instrs_count,
                         module_instrs_count);
}

void InputDependencyStatistics::report(llvm::Module& M, const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo) const
{
    unsigned module_dep_count = 0;
    unsigned module_indep_count = 0;
    unsigned module_unknown_count = 0;

    std::ofstream stats_strm("input_dep_stats.txt", std::ofstream::out);
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

static llvm::cl::opt<std::string> stats_type(
    "type",
    llvm::cl::desc("Statistics type"),
    llvm::cl::value_desc("type name"));


char InputDependencyStatisticsPass::ID = 0;

void InputDependencyStatisticsPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<InputDependencyAnalysis>();
    AU.setPreservesAll();
}

bool InputDependencyStatisticsPass::runOnModule(llvm::Module& M)
{
    const auto& IDA = getAnalysis<InputDependencyAnalysis>();
    Statistics* statistics;
    Type type = string_to_stats_type(stats_type);
    switch (type) {
    case INPUT_DEP:
        statistics = new InputDependencyStatistics();
        break;
    case COVERAGE:
        statistics = new CoverageStatistics();
    default:
        llvm::dbgs() << "Invalid statistics type\n";
    };
    if (statistics) {
        statistics->report(M, IDA.getAnalysisInfo());
    }
    return false;
}

static llvm::RegisterPass<InputDependencyStatisticsPass> X("stats-dependency","runs input dependency analysis");

}

