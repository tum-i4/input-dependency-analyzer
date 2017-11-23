#include "InputDependencyStatistics.h"
#include "Utils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/PassRegistry.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

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
}

InputDependencyStatistics::InputDependencyStatistics(const std::string& format,
                                                     const std::string& file_name,
                                                     llvm::Module& M,
                                                     const InputDependencyAnalysisInfo& IDA)
    : Statistics(format, file_name)
    , m_module(M)
    , m_IDA(IDA)
{
}

void InputDependencyStatistics::report()
{
    reportInputDepInputIndepRatio(false);
    reportInputDependencyInfo(false);
    reportInputDepCoverage(false);
    flush();
}

void InputDependencyStatistics::reportInputDepInputIndepRatio(bool do_flush)
{
    unsigned module_dep_count = 0;
    unsigned module_indep_count = 0;
    unsigned module_unknown_count = 0;

    for (auto& F : m_module) {
        auto FA_pos = m_IDA.find(&F);
        if (FA_pos == m_IDA.end()) {
            continue;
        }
        const auto& FA = FA_pos->second;
        unsigned dep_count = FA->get_input_dep_count();
        unsigned indep_count = FA->get_input_indep_count();
        unsigned unknown_count = FA->get_input_unknowns_count();
        report_inputdepindep_data(inputdepindep_data{F.getName(),
                                  dep_count, indep_count,
                                  unknown_count});
        module_dep_count += dep_count;
        module_indep_count += indep_count;
        module_unknown_count += unknown_count;
    }
    report_inputdepindep_data(
            inputdepindep_data{m_module.getName(),
            module_dep_count,
            module_indep_count,
            module_unknown_count});
    if (do_flush) {
        flush();
    }
}

void InputDependencyStatistics::reportInputDependencyInfo(bool do_flush)
{
    unsigned module_instructions = 0;
    unsigned module_inputdep_instrs = 0;
    unsigned inputdep_functions_count = 0;
    std::vector<std::string> input_dep_functions;

    for (const auto& F_inputDep : m_IDA) {
        module_instructions += get_function_instrs_count(*F_inputDep.first);
        module_inputdep_instrs += F_inputDep.second->get_input_indep_count();
        if (F_inputDep.second->isInputDepFunction()) {
            ++inputdep_functions_count;
            input_dep_functions.push_back(F_inputDep.first->getName());
        }
    }
    report_inputdep_data(inputdep_data{m_module.getName(),
                                       module_instructions,
                                       module_inputdep_instrs,
                                       inputdep_functions_count,
                                       input_dep_functions});
    if (do_flush) {
        flush();
    }
}

void InputDependencyStatistics::reportInputDepCoverage(bool do_flush)
{
    unsigned module_indep_blocks_count = 0;
    unsigned module_unreachable_blocks_count = 0;
    unsigned module_blocks_count = 0;
    unsigned module_indep_instrs_count = 0;
    unsigned module_unreachable_instrs_count = 0;
    unsigned module_instrs_count = 0;

    for (auto& F : m_module) {
        auto FA_pos = m_IDA.find(&F);
        if (FA_pos == m_IDA.end()) {
            continue;
        }
        const auto& FA = FA_pos->second;
        unsigned indep_count = FA->get_input_indep_blocks_count();
        unsigned unreachable = FA->get_unreachable_blocks_count();
        unsigned blocks = F.getBasicBlockList().size();
        unsigned indep_instrs_count = FA->get_input_indep_count();
        unsigned unreachable_instrs = FA->get_unreachable_instructions_count();
        unsigned instructions = get_function_instrs_count(F);
        report_coverage_data(coverage_data{F.getName(), indep_count, unreachable, blocks,
                                           indep_instrs_count, unreachable_instrs, instructions});
        module_indep_blocks_count += indep_count;
        module_unreachable_blocks_count += unreachable;
        module_blocks_count += blocks;
        module_indep_instrs_count += indep_instrs_count;
        module_unreachable_instrs_count += unreachable_instrs;
        module_instrs_count += instructions;
    }
    report_coverage_data(
            coverage_data{m_module.getName(), module_indep_blocks_count,
                          module_unreachable_blocks_count, module_blocks_count,
                          module_indep_instrs_count, module_unreachable_instrs_count,
                          module_instrs_count});
    if (do_flush) {
        flush();
    }
}

void InputDependencyStatistics::report_inputdepindep_data(const inputdepindep_data& data)
{
    write_entry(data.name, "Instructions", data.input_deps_count + data.input_indeps_count + data.unknowns_count);
    write_entry(data.name, "NumInputDep", data.input_deps_count);
    write_entry(data.name, "NumInputInDep", data.input_indeps_count);
    write_entry(data.name, "NumUnknowns", data.unknowns_count);
    unsigned percent = (data.input_deps_count * 100) / (data.input_deps_count + data.input_indeps_count + data.unknowns_count);
    write_entry(data.name, "Ratio", percent);
}

void InputDependencyStatistics::report_inputdep_data(const inputdep_data& data)
{
    write_entry(data.name, "NumOfInst", data.all_instrs_count);
    write_entry(data.name, "NumOfInDepInst", data.input_dep_instrs_count);
    write_entry(data.name, "NumOfInDepFuncs", data.inputdep_functions_count);
    write_entry(data.name, "InputDepFuncs", data.inputdep_functions);
}

void InputDependencyStatistics::report_coverage_data(const coverage_data& data)
{
    write_entry(data.name, "NumBlocks", data.all_blocks);
    write_entry(data.name, "NumInputIndepBlocks", data.input_indep_blocks);
    write_entry(data.name, "NumUnreachableBlocks", data.unreachable_blocks);
    unsigned block_coverage = (data.input_indep_blocks * 100) / (data.all_blocks - data.unreachable_blocks);
    write_entry(data.name, "BlockCoverage", block_coverage);

    write_entry(data.name, "NumInstrs", data.all_instrs);
    write_entry(data.name, "NumInputIndepInstr", data.input_indep_instrs);
    write_entry(data.name, "NumUnreachableInstr", data.unreachable_instrs);
    unsigned instr_coverage = (data.input_indep_instrs * 100) / (data.all_instrs - data.unreachable_instrs);
    write_entry(data.name, "InstrCoverage", instr_coverage);
}

static llvm::cl::opt<std::string> stats_format(
    "stats-format",
    llvm::cl::desc("Statistics format"),
    llvm::cl::value_desc("format name"));

static llvm::cl::opt<std::string> stats_file(
    "stats-file",
    llvm::cl::desc("Statistics file"),
    llvm::cl::value_desc("file name"));

char InputDependencyStatisticsPass::ID = 0;

void InputDependencyStatisticsPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<InputDependencyAnalysis>();
    AU.setPreservesAll();
}

bool InputDependencyStatisticsPass::runOnModule(llvm::Module& M)
{
    const auto& IDA = getAnalysis<InputDependencyAnalysis>();
    std::string file_name = stats_file;
    if (stats_file.empty()) {
        file_name = "stats";
    }
    InputDependencyStatistics statistics(stats_format, stats_file, M, IDA.getAnalysisInfo());
    statistics.report();
    return false;
}

static llvm::RegisterPass<InputDependencyStatisticsPass> X("stats-dependency","runs input dependency analysis");

}

