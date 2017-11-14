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

#include "json/writer.h"
#include "json/value.h"

#include <fstream>

namespace input_dependency {

class InputDependencyStatistics::ReportWriter
{
public:
    // both input dep and indep info
    struct inputdepindep_data
    {
        std::string name;
        std::string unit_name;
        unsigned input_deps_count;
        unsigned input_indeps_count;
        unsigned unknowns_count;
    };

    // input dep info
    struct inputdep_data
    {
        std::string name;
        std::string unit_name;
        unsigned all_instrs_count;
        unsigned input_dep_instrs_count;
        unsigned inputdep_functions_count;
        std::vector<std::string> inputdep_functions;
    };

    struct clone_data
    {
        std::string name;
        unsigned numOfClonnedInst;
        unsigned numOfInstAfterCloning;
        unsigned numOfInDepInstAfterCloning;
        std::vector<std::string> clonnedFuncs;
    };

    struct extraction_data
    {
        std::string name;
        unsigned numOfExtractedInst;
        unsigned numOfMediateInst;
        std::vector<std::string> extractedFuncs;
    };

    struct coverage_data
    {
        std::string name;
        std::string unit_name;
        unsigned input_indep_blocks;
        unsigned unreachable_blocks;
        unsigned all_blocks;
        unsigned input_indep_instrs;
        unsigned unreachable_instrs;
        unsigned all_instrs;
    };

public:
    virtual ~ReportWriter()
    {
    }

    virtual void write_inputdep_ratio_statistics(const inputdepindep_data& data) = 0;
    virtual void write_input_dep_info_statistics(const inputdep_data& data) = 0;
    virtual void write_clone_statistics(const clone_data& data) = 0;
    virtual void write_extraction_statistics(const extraction_data& data) = 0;
    virtual void write_coverage_statistics(const coverage_data& data) = 0;
};

class TextReportWriter : public InputDependencyStatistics::ReportWriter
{
public:
    TextReportWriter(const std::string& file_name);
    ~TextReportWriter();

    void write_inputdep_ratio_statistics(const inputdepindep_data& data) override;
    void write_input_dep_info_statistics(const inputdep_data& data) override;
    void write_clone_statistics(const clone_data& data) override;
    void write_extraction_statistics(const extraction_data& data) override;
    void write_coverage_statistics(const coverage_data& data) override;

private:
    std::ofstream m_strm;
};

class JsonReportWriter : public InputDependencyStatistics::ReportWriter
{
public:
    JsonReportWriter(const std::string& file_name);
    ~JsonReportWriter();

    void write_inputdep_ratio_statistics(const inputdepindep_data& data) override;
    void write_input_dep_info_statistics(const inputdep_data& data) override;
    void write_clone_statistics(const clone_data& data) override;
    void write_extraction_statistics(const extraction_data& data) override;
    void write_coverage_statistics(const coverage_data& data) override;

private:
    std::ofstream m_strm;
};

JsonReportWriter::JsonReportWriter(const std::string& file_name)
{
    m_strm.open(file_name, std::ofstream::out);
}

JsonReportWriter::~JsonReportWriter()
{
    m_strm.close();
}

void JsonReportWriter::write_inputdep_ratio_statistics(const inputdepindep_data& data)
{
    Json::Value info;   
    info[data.name]["Instructions"] = data.input_deps_count + data.input_indeps_count + data.unknowns_count;
    info[data.name]["NumInputDep"] = data.input_deps_count;
    info[data.name]["NumInputInDep"] = data.input_indeps_count;
    info[data.name]["NumUnknowns"] = data.unknowns_count;
    unsigned percent = (data.input_deps_count * 100) / (data.input_deps_count + data.input_indeps_count + data.unknowns_count);
    info[data.name]["Ratio"] = percent;
    m_strm << info << "\n";
}

void JsonReportWriter::write_input_dep_info_statistics(const inputdep_data& data)
{
    Json::Value info;
    info[data.name]["NumOfInst"] = data.all_instrs_count;
    info[data.name]["NumOfInDepInst"] = data.input_dep_instrs_count;
    info[data.name]["NumOfInDepFuncs"] = data.inputdep_functions_count;
    Json::Value jsonArray;
    std::for_each(data.inputdep_functions.begin(), data.inputdep_functions.end(),
                 [&jsonArray] (const std::string& f_name) {jsonArray.append(f_name);});
    info[data.name]["InputDepFuncs"] = jsonArray;
    m_strm << info;
}

void JsonReportWriter::write_clone_statistics(const clone_data& data)
{
    Json::Value info;
    info[data.name]["NumOfClonnedInst"] = data.numOfClonnedInst;
    info[data.name]["NumOfInstAfterCloning"] = data.numOfInstAfterCloning;
    info[data.name]["NumOfInDepInstAfterCloning"] = data.numOfInDepInstAfterCloning;
    Json::Value jsonArray;
    std::for_each(data.clonnedFuncs.begin(), data.clonnedFuncs.end(),
                 [&jsonArray] (const std::string& f_name) {jsonArray.append(f_name);});
    info[data.name]["InputDepFuncs"] = jsonArray;
    m_strm << info;
}

void JsonReportWriter::write_extraction_statistics(const extraction_data& data)
{
    Json::Value info;
    info[data.name]["NumOfExtractedInst"] = data.numOfExtractedInst;
    info[data.name]["NumOfMediateInst"] = data.numOfMediateInst;
    Json::Value jsonArray;
    std::for_each(data.extractedFuncs.begin(), data.extractedFuncs.end(),
                 [&jsonArray] (const std::string& f_name) {jsonArray.append(f_name);});
    info[data.name]["ExtractedFuncs"] = jsonArray;
    m_strm << info;
}

void JsonReportWriter::write_coverage_statistics(const coverage_data& data)
{
    Json::Value info;   
    info[data.name]["Blocks"]["NumBlocks"] = data.all_blocks;
    info[data.name]["Blocks"]["NumInputIndep"] = data.input_indep_blocks;
    info[data.name]["Blocks"]["NumUnreachable"] = data.unreachable_blocks;
    unsigned block_coverage = (data.input_indep_blocks * 100) / (data.all_blocks - data.unreachable_blocks);
    info[data.name]["Blocks"]["Coverage"] = block_coverage;

    info[data.name]["Instructions"]["NumInstrs"] = data.all_instrs;
    info[data.name]["Instructions"]["NumInputIndep"] = data.input_indep_instrs;
    info[data.name]["Instructions"]["NumUnreachable"] = data.unreachable_instrs;
    unsigned instr_coverage = (data.input_indep_instrs * 100) / (data.all_instrs - data.unreachable_instrs);
    info[data.name]["Instructions"]["Coverage"] = instr_coverage;

    m_strm << info << "\n";
}


TextReportWriter::TextReportWriter(const std::string& file_name)
{
    m_strm.open(file_name, std::ofstream::out);
}

TextReportWriter::~TextReportWriter()
{
    m_strm.close();
}

void TextReportWriter::write_inputdep_ratio_statistics(const inputdepindep_data& data)
{
    m_strm << "Input Dep statistics for " << data.unit_name << " " << data.name << "\n";
    m_strm << "Input Dependent instructions: " << data.input_deps_count << "\n";
    m_strm << "Input Independent instructions: " << data.input_indeps_count << "\n";
    m_strm << "Unknown instructions: " << data.unknowns_count << "\n";
    unsigned percent = (data.input_deps_count * 100) / (data.input_deps_count + data.input_indeps_count + data.unknowns_count);
    m_strm << "Input dependent instructions' percent: " << percent << "%\n";
}

void TextReportWriter::write_input_dep_info_statistics(const inputdep_data& data)
{
    m_strm << "Input dep statistics for " << data.unit_name << " " << data.name << "\n";
    m_strm << "Number of instructions: " << data.all_instrs_count << "\n";
    m_strm << "Number of input dep instructions: " << data.input_dep_instrs_count << "\n";
    m_strm << "Number of input dep functions: " << data.inputdep_functions_count << "\n";
    m_strm << "Input dep functions:\n";
    std::for_each(data.inputdep_functions.begin(), data.inputdep_functions.end(),
                  [this] (const std::string& f_name) {this->m_strm << f_name << "\n";});
}

void TextReportWriter::write_clone_statistics(const clone_data& data)
{
    m_strm << "Clone statistics for module " << data.name << "\n";
    m_strm << "Number of clonned instructions: " << data.numOfClonnedInst << "\n";
    m_strm << "Number of instructions after cloning: " << data.numOfInstAfterCloning << "\n";
    m_strm << "Number of input dep instructions after clonning: " << data.numOfInDepInstAfterCloning << "\n";
    m_strm << "Clonned functions:\n";
    std::for_each(data.clonnedFuncs.begin(), data.clonnedFuncs.end(),
                  [this] (const std::string& f_name) {this->m_strm << f_name << "\n";});
}

void TextReportWriter::write_extraction_statistics(const extraction_data& data)
{
    m_strm << "Extraction statistics for module " << data.name << "\n";
    m_strm << "Number of extracted instructions: " << data.numOfExtractedInst << "\n";
    m_strm << "Number of mediate instructions: " << data.numOfMediateInst << "\n";
    m_strm << "Extracted functions:\n";
    std::for_each(data.extractedFuncs.begin(), data.extractedFuncs.end(),
                  [this] (const std::string& f_name) {this->m_strm << f_name << "\n";});
}

void TextReportWriter::write_coverage_statistics(const coverage_data& data)
{
    m_strm << data.unit_name << " coverage for " << data.name << " ----- ";
    m_strm << "Blocks coverage\n";
    unsigned block_coverage = (data.input_indep_blocks * 100) / (data.all_blocks - data.unreachable_blocks);
    m_strm << block_coverage << "\n";
    m_strm << "Instructions coverage\n";
    unsigned instrs_coverage = (data.input_indep_instrs * 100) / (data.all_instrs - data.unreachable_instrs);
    m_strm << instrs_coverage << "\n";
}

unsigned get_function_instrs_count(llvm::Function& F)
{
    unsigned count = 0;
    for (auto& B : F) {
        count += B.getInstList().size();
    }
    return count;
}

InputDependencyStatistics::Format string_to_stats_format(const std::string& stats_format)
{
    if (stats_format == "json") {
        return InputDependencyStatistics::JSON;
    }
    //return InputDependencyStatistics::JSON;
    return InputDependencyStatistics::TEXT;
}

InputDependencyStatistics::InputDependencyStatistics(const std::string& format_str,
                                                     const std::string& file_name)
{
    Format format = string_to_stats_format(format_str);
    switch (format) {
    case TEXT:
        m_writer.reset(new TextReportWriter(file_name));
        break;
    case JSON:
        m_writer.reset(new JsonReportWriter(file_name));
        break;
    }
}

void InputDependencyStatistics::report(llvm::Module& M,
                                       const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo)
{
    reportInputDepInputIndepRatio(M, inputDepInfo);
    reportInputDependencyInfo(M, inputDepInfo);
    reportCloningInformation(M, inputDepInfo);
    reportExtractionInformation(M, inputDepInfo);
    // add the others here
    reportInputDepCoverage(M, inputDepInfo);
}

void InputDependencyStatistics::reportInputDepInputIndepRatio(llvm::Module& M,
                                     const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo)
{
    unsigned module_dep_count = 0;
    unsigned module_indep_count = 0;
    unsigned module_unknown_count = 0;

    for (auto& F : M) {
        auto FA_pos = inputDepInfo.find(&F);
        if (FA_pos == inputDepInfo.end()) {
            continue;
        }
        const auto& FA = FA_pos->second;
        unsigned dep_count = FA->get_input_dep_count();
        unsigned indep_count = FA->get_input_indep_count();
        unsigned unknown_count = FA->get_input_unknowns_count();
        m_writer->write_inputdep_ratio_statistics(ReportWriter::inputdepindep_data{F.getName(), "function",
                dep_count, indep_count,
                unknown_count});
        module_dep_count += dep_count;
        module_indep_count += indep_count;
        module_unknown_count += unknown_count;
    }
    m_writer->write_inputdep_ratio_statistics(
            ReportWriter::inputdepindep_data{M.getName(), "module",
            module_dep_count,
            module_indep_count,
            module_unknown_count});
}

void InputDependencyStatistics::reportInputDependencyInfo(llvm::Module& M,
                             const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo)
{
    unsigned module_instructions = 0;
    unsigned module_inputdep_instrs = 0;
    unsigned inputdep_functions_count = 0;
    std::vector<std::string> input_dep_functions;

    for (const auto& F_inputDep : inputDepInfo) {
        module_instructions += get_function_instrs_count(*F_inputDep.first);
        module_inputdep_instrs += F_inputDep.second->get_input_indep_count();
        if (F_inputDep.second->isInputDepFunction()) {
            ++inputdep_functions_count;
            input_dep_functions.push_back(F_inputDep.first->getName());
        }
    }
    m_writer->write_input_dep_info_statistics(ReportWriter::inputdep_data{M.getName(), "module",
                                                               module_instructions,
                                                               module_inputdep_instrs,
                                                               inputdep_functions_count,
                                                               input_dep_functions});
}

void InputDependencyStatistics::reportCloningInformation(llvm::Module& M,
                                  const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo)
{
    unsigned numOfClonnedInst = 0;
    unsigned numOfInstAfterCloning = 0;
    unsigned numOfInDepInstAfterCloning = 0;
    std::vector<std::string> clonnedFuncs;

    for (const auto& F_inputDep : inputDepInfo) {
        numOfInstAfterCloning += get_function_instrs_count(*F_inputDep.first);
        numOfInDepInstAfterCloning += F_inputDep.second->get_input_dep_count();
        auto clonedFunctionDepInfo = F_inputDep.second->toClonedFunctionAnalysisResult();
        if (!clonedFunctionDepInfo) {
            continue;
        }
        numOfClonnedInst += get_function_instrs_count(*F_inputDep.first);
        clonnedFuncs.push_back(F_inputDep.first->getName());
    }
    m_writer->write_clone_statistics(ReportWriter::clone_data{M.getName(), numOfClonnedInst, numOfInstAfterCloning,
                                                              numOfInDepInstAfterCloning, clonnedFuncs});
}

void InputDependencyStatistics::reportExtractionInformation(llvm::Module& M,
                                const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo)
{
    unsigned numOfExtractedInst = 0;
    // TODO: see what on earth this means
    unsigned numOfMediateInst = 0;
    std::vector<std::string> extractedFuncs;

    for (const auto& F_inputDep : inputDepInfo) {
        auto extractedFunctionDepInfo = F_inputDep.second->toInputDependentFunctionAnalysisResult();
        if (!extractedFunctionDepInfo) {
            continue;
        }
        numOfExtractedInst += get_function_instrs_count(*F_inputDep.first);
        extractedFuncs.push_back(F_inputDep.first->getName());
    }
    m_writer->write_extraction_statistics(ReportWriter::extraction_data{M.getName(), numOfExtractedInst,
                                                                        numOfMediateInst, extractedFuncs});
}

void InputDependencyStatistics::reportInputDepCoverage(llvm::Module& M,
                           const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo)
{
    unsigned module_indep_blocks_count = 0;
    unsigned module_unreachable_blocks_count = 0;
    unsigned module_blocks_count = 0;
    unsigned module_indep_instrs_count = 0;
    unsigned module_unreachable_instrs_count = 0;
    unsigned module_instrs_count = 0;

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
        m_writer->write_coverage_statistics(ReportWriter::coverage_data{F.getName(), "function", indep_count, unreachable, blocks,
                indep_instrs_count, unreachable_instrs, instructions});
        module_indep_blocks_count += indep_count;
        module_unreachable_blocks_count += unreachable;
        module_blocks_count += blocks;
        module_indep_instrs_count += indep_instrs_count;
        module_unreachable_instrs_count += unreachable_instrs;
        module_instrs_count += instructions;
    }
    m_writer->write_coverage_statistics(
            ReportWriter::coverage_data{M.getName(), "module", module_indep_blocks_count,
            module_unreachable_blocks_count, module_blocks_count,
            module_indep_instrs_count, module_unreachable_instrs_count,
            module_instrs_count});
}

static llvm::cl::opt<std::string> stats_format(
    "format",
    llvm::cl::desc("Statistics format"),
    llvm::cl::value_desc("format name"));


char InputDependencyStatisticsPass::ID = 0;

void InputDependencyStatisticsPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<InputDependencyAnalysis>();
    AU.setPreservesAll();
}

bool InputDependencyStatisticsPass::runOnModule(llvm::Module& M)
{
    const auto& IDA = getAnalysis<InputDependencyAnalysis>();
    InputDependencyStatistics statistics(stats_format, "stats");
    statistics.report(M, IDA.getAnalysisInfo());
    return false;
}

static llvm::RegisterPass<InputDependencyStatisticsPass> X("stats-dependency","runs input dependency analysis");

}

