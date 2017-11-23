#pragma once

#include "Analysis/Statistics.h"

#include "llvm/Pass.h"

#include <unordered_set>

namespace oh {
class ExtractionStatistics : public input_dependency::Statistics
{
public:
    ExtractionStatistics() = default;
    ExtractionStatistics(const std::string& format,
                         const std::string& file_name)
        : Statistics(format, file_name)
        , m_numOfExtractedInst(0)
        , m_numOfMediateInst(0)
    {
    }

public:
    void report() override;

    void set_module_name(const std::string& name)
    {
        m_module_name = name;
    }

    void add_numOfExtractedInst(unsigned num)
    {
        m_numOfExtractedInst += num;
    }

    void add_numOfMediateInst(unsigned num)
    {
        m_numOfMediateInst += num;
    }

    void add_extractedFunction(const std::string& name)
    {
        m_extractedFuncs.push_back(name);
    }

private:
    std::string m_module_name;
    unsigned m_numOfExtractedInst;
    unsigned m_numOfMediateInst;
    std::vector<std::string> m_extractedFuncs;
}; // class CloneStatistics


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
    void initialize_statistics();

private:
    std::unordered_set<llvm::Function*> m_extracted_functions;
    ExtractionStatistics m_statistics;
};

} // namespace oh

