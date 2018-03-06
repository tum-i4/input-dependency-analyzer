#include "InputDependencyStatistics.h"
#include "InputDependencyAnalysisPass.h"
#include "FunctionInputDependencyResultInterface.h"
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
                                                     llvm::Module* M,
                                                     InputDependencyAnalysisInfo* IDA)
    : Statistics(format, file_name)
    , m_module(M)
    , m_IDA(IDA)
{
    setSectionName("input_dependency_stats");
}

void InputDependencyStatistics::report()
{
    reportInputDependencyInfo();
    reportInputDepCoverage();
    reportInputInDepCoverage();
    reportDataInpdependentCoverage();
}

void InputDependencyStatistics::reportInputDependencyInfo()
{
    llvm::dbgs() << "input_dependency_info\n";
    setStatsTypeName("input_dependency_info");
    unsigned module_instructions = 0;
    unsigned module_inputdep_instrs = 0;
    unsigned inputdep_functions_count = 0;
    std::vector<std::string> input_dep_functions;

    for (const auto& F_inputDep : *m_IDA) {
        module_instructions += get_function_instrs_count(*F_inputDep.first);
        module_inputdep_instrs += F_inputDep.second->get_input_indep_count();
        if (F_inputDep.second->isInputDepFunction() || F_inputDep.second->isExtractedFunction()) {
            ++inputdep_functions_count;
            input_dep_functions.push_back(F_inputDep.first->getName());
        }
    }
    report_inputdep_data(inputdep_data{m_module->getName(),
                                       module_instructions,
                                       module_inputdep_instrs,
                                       inputdep_functions_count,
                                       input_dep_functions});
}

void InputDependencyStatistics::reportInputInDepCoverage()
{
    setStatsTypeName("input_indep_coverage");
    input_indep_coverage_data module_coverage_data{m_module->getName(), 0, 0, 0, 0, 0, 0};

    for (auto& F : *m_module) {
        auto FA_pos = m_IDA->find(&F);
        if (FA_pos == m_IDA->end()) {
            continue;
        }
        if (F.isDeclaration()) {
            continue;
        }
        auto cached_input_indep_data = m_function_input_indep_function_coverage_data.find(&F);
        if (cached_input_indep_data != m_function_input_indep_function_coverage_data.end()) {
            report_input_indep_coverage_data(cached_input_indep_data->second);
            update_module_coverage_data(module_coverage_data, cached_input_indep_data->second);
            continue;
        }
        const auto& FA = FA_pos->second;
        unsigned indep_count = 0;
        unsigned indep_instrs_count = 0;
        if (!FA->isInputDepFunction()) {
            indep_count =  FA->get_input_indep_blocks_count();
            indep_instrs_count = FA->get_input_indep_count();
        }
        unsigned unreachable = FA->get_unreachable_blocks_count();
        unsigned blocks = F.getBasicBlockList().size();
        unsigned unreachable_instrs = FA->get_unreachable_instructions_count();
        unsigned instructions = get_function_instrs_count(F);
        auto input_indep_cov = input_indep_coverage_data{F.getName(), indep_count, unreachable, blocks,
                                           indep_instrs_count, unreachable_instrs, instructions};

        m_function_input_indep_function_coverage_data.insert(std::make_pair(&F, input_indep_cov));
        report_input_indep_coverage_data(input_indep_cov);
        update_module_coverage_data(module_coverage_data, input_indep_cov);
    }
    report_input_indep_coverage_data(module_coverage_data);
    unsetStatsTypeName();
}

void InputDependencyStatistics::reportInputDepCoverage()
{
    setStatsTypeName("input_dep_coverage");
    input_dep_coverage_data module_coverage_data{m_module->getName(), 0, 0, 0, 0, 0, 0};

    for (auto& F : *m_module) {
        auto FA_pos = m_IDA->find(&F);
        if (FA_pos == m_IDA->end()) {
            continue;
        }
        if (F.isDeclaration()) {
            continue;
        }
        auto cached_input_dep_data = m_function_input_dep_function_coverage_data.find(&F);
        if (cached_input_dep_data != m_function_input_dep_function_coverage_data.end()) {
            report_input_dep_coverage_data(cached_input_dep_data->second);
            update_module_coverage_data(module_coverage_data, cached_input_dep_data->second);
            continue;
        }
        const auto& FA = FA_pos->second;
        unsigned dep_count = FA->isInputDepFunction() ? F.getBasicBlockList().size() : FA->get_input_dep_blocks_count();
        unsigned unreachable = FA->get_unreachable_blocks_count();
        unsigned blocks = F.getBasicBlockList().size();
        unsigned dep_instrs_count = FA->isInputDepFunction() ? get_function_instrs_count(F) : FA->get_input_dep_count();
        unsigned unreachable_instrs = FA->get_unreachable_instructions_count();
        unsigned instructions = get_function_instrs_count(F);
        auto input_dep_cov = input_dep_coverage_data{F.getName(), dep_count, unreachable, blocks,
                                           dep_instrs_count, unreachable_instrs, instructions};
        m_function_input_dep_function_coverage_data.insert(std::make_pair(&F, input_dep_cov));
        report_input_dep_coverage_data(input_dep_cov);
        update_module_coverage_data(module_coverage_data, input_dep_cov);
    }
    report_input_dep_coverage_data(module_coverage_data);
    unsetStatsTypeName();
}

void InputDependencyStatistics::reportDataInpdependentCoverage()
{
    setStatsTypeName("data_indep_coverage");
    data_independent_coverage_data module_coverage_data{m_module->getName(), 0, 0};

    for (auto& F : *m_module) {
        auto FA_pos = m_IDA->find(&F);
        if (FA_pos == m_IDA->end()) {
            continue;
        }
        if (F.isDeclaration()) {
            continue;
        }
        const auto& FA = FA_pos->second;
        unsigned data_indep_count = FA->get_data_indep_count();
        unsigned instructions = get_function_instrs_count(F);
        auto data_indep_cov = data_independent_coverage_data{F.getName(), instructions, data_indep_count};
        report_data_indep_coverage_data(data_indep_cov);
        update_module_coverage_data(module_coverage_data, data_indep_cov);
    }
    report_data_indep_coverage_data(module_coverage_data);
    unsetStatsTypeName();
}

void InputDependencyStatistics::invalidate_stats_data()
{
    m_function_input_dep_function_coverage_data.clear();
    m_function_input_indep_function_coverage_data.clear();
}

void InputDependencyStatistics::report_inputdep_data(const inputdep_data& data)
{
    write_entry(data.name, "NumOfInst", data.all_instrs_count);
    write_entry(data.name, "NumOfInDepInst", data.input_dep_instrs_count);
    write_entry(data.name, "NumOfInDepFuncs", data.inputdep_functions_count);
    write_entry(data.name, "InputDepFuncs", data.inputdep_functions);
}

void InputDependencyStatistics::report_input_indep_coverage_data(const input_indep_coverage_data& data)
{
    write_entry(data.name, "NumBlocks", data.all_blocks);
    write_entry(data.name, "NumInputIndepBlocks", data.input_indep_blocks);
    write_entry(data.name, "NumUnreachableBlocks", data.unreachable_blocks);
    double block_coverage = (data.input_indep_blocks * 100.0) / (data.all_blocks - data.unreachable_blocks);
    write_entry(data.name, "BlockCoverage", block_coverage);

    write_entry(data.name, "NumInstrs", data.all_instrs);
    write_entry(data.name, "NumInputIndepInstr", data.input_indep_instrs);
    write_entry(data.name, "NumUnreachableInstr", data.unreachable_instrs);
    double instr_coverage = (data.input_indep_instrs * 100.0) / (data.all_instrs - data.unreachable_instrs);
    write_entry(data.name, "InstrCoverage", instr_coverage);
}

void InputDependencyStatistics::report_input_dep_coverage_data(const input_dep_coverage_data& data)
{
    write_entry(data.name, "NumBlocks", data.all_blocks);
    write_entry(data.name, "NumInputDepBlocks", data.input_dep_blocks);
    write_entry(data.name, "NumUnreachableBlocks", data.unreachable_blocks);
    double block_coverage = (data.input_dep_blocks * 100.0) / (data.all_blocks - data.unreachable_blocks);
    write_entry(data.name, "BlockCoverage", block_coverage);

    write_entry(data.name, "NumInstrs", data.all_instrs);
    write_entry(data.name, "NumInputDepInstr", data.input_dep_instrs);
    write_entry(data.name, "NumUnreachableInstr", data.unreachable_instrs);
    double instr_coverage = (data.input_dep_instrs * 100.0) / (data.all_instrs - data.unreachable_instrs);
    write_entry(data.name, "InstrCoverage", instr_coverage);
}

void InputDependencyStatistics::report_data_indep_coverage_data(const data_independent_coverage_data& data)
{
    write_entry(data.name, "NumInstrs", data.all_instrs);
    write_entry(data.name, "DataIndepInstrs", data.data_independent_instrs);
    double data_indep_cov = (data.data_independent_instrs * 100.0) / data.all_instrs;
    write_entry(data.name, "DataIndepCoverage", data_indep_cov);
}

void InputDependencyStatistics::update_module_coverage_data(
                                     input_dep_coverage_data& module_coverage_data,
                                     const input_dep_coverage_data& function_coverage_data) const
{
    module_coverage_data.input_dep_blocks += function_coverage_data.input_dep_blocks;
    module_coverage_data.unreachable_blocks += function_coverage_data.unreachable_blocks;
    module_coverage_data.all_blocks += function_coverage_data.all_blocks;
    module_coverage_data.input_dep_instrs += function_coverage_data.input_dep_instrs;
    module_coverage_data.unreachable_instrs += function_coverage_data.unreachable_instrs;
    module_coverage_data.all_instrs += function_coverage_data.all_instrs;
}

void InputDependencyStatistics::update_module_coverage_data(
                                     input_indep_coverage_data& module_coverage_data,
                                     const input_indep_coverage_data& function_coverage_data) const
{
    module_coverage_data.input_indep_blocks += function_coverage_data.input_indep_blocks;
    module_coverage_data.unreachable_blocks += function_coverage_data.unreachable_blocks;
    module_coverage_data.all_blocks += function_coverage_data.all_blocks;
    module_coverage_data.input_indep_instrs += function_coverage_data.input_indep_instrs;
    module_coverage_data.unreachable_instrs += function_coverage_data.unreachable_instrs;
    module_coverage_data.all_instrs += function_coverage_data.all_instrs;
}

void InputDependencyStatistics::update_module_coverage_data(
                                data_independent_coverage_data& module_coverage_data,
                                const data_independent_coverage_data& function_coverage_data) const
{
    module_coverage_data.all_instrs += function_coverage_data.all_instrs;
    module_coverage_data.data_independent_instrs += function_coverage_data.data_independent_instrs;
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
    AU.addRequired<InputDependencyAnalysisPass>();
    AU.setPreservesAll();
}

bool InputDependencyStatisticsPass::runOnModule(llvm::Module& M)
{
    auto IDA = getAnalysis<InputDependencyAnalysisPass>().getInputDependencyAnalysis();
    std::string file_name = stats_file;
    if (stats_file.empty()) {
        file_name = "stats";
    }
    InputDependencyStatistics statistics(stats_format, stats_file, &M, &IDA->getAnalysisInfo());
    statistics.report();
    statistics.flush();
    return false;
}

static llvm::RegisterPass<InputDependencyStatisticsPass> X("stats-dependency","runs input dependency analysis");

}

