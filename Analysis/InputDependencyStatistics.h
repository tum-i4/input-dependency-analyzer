#pragma once

#include "Statistics.h"
#include "InputDependencyAnalysis.h"

#include "llvm/Pass.h"
#include <memory>

namespace llvm {
class Module;
}

namespace input_dependency {

class InputDependencyStatistics : public Statistics
{
private:
    using InputDependencyAnalysisInfo = InputDependencyAnalysis::InputDependencyAnalysisInfo;

    // both input dep and indep info
    struct inputdepindep_data
    {
        std::string name;
        unsigned input_deps_count;
        unsigned input_indeps_count;
        unsigned unknowns_count;
    };

    // input dep info
    struct inputdep_data
    {
        std::string name;
        unsigned all_instrs_count;
        unsigned input_dep_instrs_count;
        unsigned inputdep_functions_count;
        std::vector<std::string> inputdep_functions;
    };

    struct coverage_data
    {
        std::string name;
        unsigned input_indep_blocks;
        unsigned unreachable_blocks;
        unsigned all_blocks;
        unsigned input_indep_instrs;
        unsigned unreachable_instrs;
        unsigned all_instrs;
    };

public:
    InputDependencyStatistics(const std::string& format,
                              const std::string& file_name,
                              llvm::Module& M,
                              const InputDependencyAnalysisInfo& IDA);

public:
    void report() override;

    /// Reports number of input dependent and independent instructions, as well as corresponding percentages.
    /// This function collects all necessary information from given inputDepAnalysisInfo
    void reportInputDepInputIndepRatio(bool do_flush = true);

    /// Reports number of input dep/input indep instructions and input dep functions.
    /// This function collects all necessary information from given inputDepAnalysisInfo
    void reportInputDependencyInfo(bool do_flush = true);

    /// Reports ratio of input indep instructions over all instructions, as well as ratio of input indep basic blocks
    /// over all basic blocks.
    /// This function collects all necessary information from given inputDepAnalysisInfo
    void reportInputDepCoverage(bool do_flush = true);

private:
    void report_inputdepindep_data(const inputdepindep_data& data);
    void report_inputdep_data(const inputdep_data& data);
    void report_coverage_data(const coverage_data& data);

private:
    llvm::Module& m_module;
    const InputDependencyAnalysisInfo& m_IDA; 
};

/// Collects and calculates statistics on input dependent instructions
class InputDependencyStatisticsPass : public llvm::ModulePass
{
public:
    static char ID;

    InputDependencyStatisticsPass()
        : llvm::ModulePass(ID)
    {
    }

public:
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;
};

} // namespace input_dependency

