#pragma once

#include "input-dependency/Analysis/Statistics.h"
#include "input-dependency/Analysis/InputDependencyStatistics.h"
#include "input-dependency/Analysis/InputDependencyAnalysisPass.h"

#include "llvm/Pass.h"

#include <memory>
#include <unordered_set>

namespace oh {
class ExtractionStatistics : public input_dependency::Statistics
{
public:
    ExtractionStatistics() = default;
    ExtractionStatistics(const std::string& module_name,
                         const std::string& format,
                         const std::string& file_name)
        : Statistics(format, file_name)
        , m_module_name(module_name)
        , m_numOfExtractedInst(0)
        , m_numOfMediateInst(0)
    {
    }

    ExtractionStatistics(Statistics::ReportWriterType writer)
        : Statistics(writer)
        , m_numOfExtractedInst(0)
        , m_numOfMediateInst(0)
    {
    }

public:
    void set_module_name(const std::string& name)
    {
        m_module_name = name;
    }

    void report() override;

    virtual void add_numOfExtractedInst(unsigned num)
    {
        m_numOfExtractedInst += num;
    }

    virtual void add_numOfMediateInst(unsigned num)
    {
        m_numOfMediateInst += num;
    }

    virtual void add_extractedFunction(const std::string& name)
    {
        m_extractedFuncs.push_back(name);
    }

private:
    std::string m_module_name;
    unsigned m_numOfExtractedInst;
    unsigned m_numOfMediateInst;
    std::vector<std::string> m_extractedFuncs;
}; // class CloneStatistics

class DummyExtractionStatistics :  public ExtractionStatistics
{
public:
    DummyExtractionStatistics() = default;

    void report() override {}
    void flush() override {}
    void add_numOfExtractedInst(unsigned num) override {}
    void add_numOfMediateInst(unsigned num) override {}
    void add_extractedFunction(const std::string& name) override {}
};

/**
* \class FunctionExtractionPass
* \brief Transformation pass to extract input dependent snippets of a function into separate function.
* Runs only for functions that are not input dependent, saying all call sites are from deterministic locations.
* Collects all functions that have been extracted as a result of the pass
*/
class FunctionExtractionPass : public llvm::ModulePass
{
public:
    static char ID;

    FunctionExtractionPass()
        : llvm::ModulePass(ID)
    {
    }

public:
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;

    const std::unordered_set<llvm::Function*>& get_extracted_functions() const;

private:
    void createStatistics(llvm::Module& M, input_dependency::InputDependencyAnalysisInterface& IDA);

private:
    std::unordered_set<llvm::Function*> m_extracted_functions;
    using ExtractionStatisticsType = std::shared_ptr<ExtractionStatistics>;
    ExtractionStatisticsType m_extractionStatistics;
    using CoverageStatisticsType = std::shared_ptr<input_dependency::InputDependencyStatistics>;
    CoverageStatisticsType m_coverageStatistics;
};

} // namespace oh

