#include "analysis/InputDependencyStatistics.h"
#include "analysis/InputDependencyAnalysis.h"
#include "utils/Utils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/PassRegistry.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

namespace input_dependency {

namespace {
unsigned get_argument_or_data_dependent_loop_instr_count(llvm::Function& F,
                                                         InputDependencyStatistics::InputDepAnalysisRes IDA,
                                                         llvm::LoopInfo* LI)
{
    unsigned argument_dep_instr_count = 0;
    for (auto& B : F) {
        auto* loop = LI->getLoopFor(&B);
        if (!loop) {
            continue;
        }
        if (!IDA->isInputDependent(&B) && !IDA->isArgumentDependent(&B)) {
            continue;
        }
        for (auto& I : B) {
            if (!IDA->isDataDependent(&I)) {
                ++argument_dep_instr_count;
            }
        }
    }
    return argument_dep_instr_count;
}


}

InputDependencyStatistics::InputDependencyStatistics(const std::string& format,
                                                     const std::string& file_name,
                                                     llvm::Module* M,
                                                     InputDepAnalysisRes IDA)
    : Statistics(format, file_name)
    , m_module(M)
    , m_IDA(IDA)
{
    setSectionName("input_dependency_stats");
}

void InputDependencyStatistics::setLoopInfoGetter(const LoopInfoGetter& loop_info_getter)
{
    m_loopInfoGetter = loop_info_getter;
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

    for (auto& F : *m_module) {
        module_instructions += Utils::getFunctionInstrsCount(F);
        module_inputdep_instrs += m_IDA->getInputIndepInstrCount(&F);
        if (m_IDA->isInputDependent(&F) /*|| isExtractedFunction*/) {
            ++inputdep_functions_count;
            input_dep_functions.push_back(F.getName().str());
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
        if (F.isDeclaration()) {
            continue;
        }
        auto cached_input_indep_data = m_function_input_indep_function_coverage_data.find(&F);
        if (cached_input_indep_data != m_function_input_indep_function_coverage_data.end()) {
            report_input_indep_coverage_data(cached_input_indep_data->second);
            update_module_coverage_data(module_coverage_data, cached_input_indep_data->second);
            continue;
        }
        unsigned indep_count = 0;
        unsigned indep_instrs_count = 0;
        if (!m_IDA->isInputDependent(&F)) {
            indep_count =  m_IDA->getInputIndepBlocksCount(&F);
            indep_instrs_count = m_IDA->getInputIndepInstrCount(&F);
        }
        unsigned unreachable = m_IDA->getUnreachableBlocksCount(&F);
        unsigned blocks = F.getBasicBlockList().size();
        unsigned unreachable_instrs = m_IDA->getUnreachableInstructionsCount(&F);
        unsigned instructions = Utils::getFunctionInstrsCount(F);
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
        if (F.isDeclaration()) {
            continue;
        }
        auto cached_input_dep_data = m_function_input_dep_function_coverage_data.find(&F);
        if (cached_input_dep_data != m_function_input_dep_function_coverage_data.end()) {
            report_input_dep_coverage_data(cached_input_dep_data->second);
            update_module_coverage_data(module_coverage_data, cached_input_dep_data->second);
            continue;
        }
        unsigned dep_count = m_IDA->isInputDependent(&F) ? F.getBasicBlockList().size() : m_IDA->getInputDepBlocksCount(&F);
        unsigned unreachable = m_IDA->getUnreachableBlocksCount(&F);
        unsigned blocks = F.getBasicBlockList().size();
        unsigned dep_instrs_count = m_IDA->isInputDependent(&F) ? Utils::getFunctionInstrsCount(F) : m_IDA->getInputDepInstrCount(&F);
        unsigned unreachable_instrs = m_IDA->getUnreachableInstructionsCount(&F);
        unsigned instructions = Utils::getFunctionInstrsCount(F);
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
    data_independent_coverage_data module_coverage_data{m_module->getName(), 0, 0, 0, 0};

    for (auto& F : *m_module) {
        if (F.isDeclaration()) {
            continue;
        }
        unsigned data_indep_count = m_IDA->getDataIndepInstrCount(&F);
        unsigned instructions = Utils::getFunctionInstrsCount(F);
        unsigned argument_dep_data_indeps = m_IDA->getArgumentDepInstrCount(&F);
        unsigned argument_or_data_dep_data_dep_loop_instrs = get_argument_or_data_dependent_loop_instr_count(F,
                                                                                                             m_IDA,
                                                                                                             m_loopInfoGetter(&F));
        auto data_indep_cov = data_independent_coverage_data{F.getName(), instructions, data_indep_count,
                                                             argument_dep_data_indeps,
                                                             argument_or_data_dep_data_dep_loop_instrs};
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
    write_entry(data.name, "ArgumentDepInstrs", data.argument_dependent_instrs);
    write_entry(data.name, "ArgumentOrDataDepLoopInstrs", data.dep_loop_instrs);
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
    module_coverage_data.argument_dependent_instrs += function_coverage_data.argument_dependent_instrs;
    module_coverage_data.dep_loop_instrs += function_coverage_data.dep_loop_instrs;
}

}




