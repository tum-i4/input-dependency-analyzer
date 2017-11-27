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

    struct input_indep_coverage_data
    {
        std::string name;
        unsigned input_indep_blocks;
        unsigned unreachable_blocks;
        unsigned all_blocks;
        unsigned input_indep_instrs;
        unsigned unreachable_instrs;
        unsigned all_instrs;
    };

    struct input_dep_coverage_data
    {
        std::string name;
        unsigned input_dep_blocks;
        unsigned unreachable_blocks;
        unsigned all_blocks;
        unsigned input_dep_instrs;
        unsigned unreachable_instrs;
        unsigned all_instrs;
    };
public:
    InputDependencyStatistics() = default;
    InputDependencyStatistics(const std::string& format,
                              const std::string& file_name,
                              llvm::Module* M,
                              InputDependencyAnalysisInfo* IDA);

public:
    void report() override;

    /// Reports number of input dependent and independent instructions, as well as corresponding percentages.
    /// This function collects all necessary information from given inputDepAnalysisInfo
    virtual void reportInputDepInputIndepRatio();

    /// Reports number of input dep/input indep instructions and input dep functions.
    /// This function collects all necessary information from given inputDepAnalysisInfo
    virtual void reportInputDependencyInfo();

    /// Reports ratio of input indep instructions over all instructions, as well as ratio of input indep basic blocks
    /// over all basic blocks.
    /// This function collects all necessary information from given inputDepAnalysisInfo and caches for further uses.
    /// Cached data can be invalidated by a call of \a invalidate_stats_data
    virtual void reportInputInDepCoverage();

    /// Reports ratio of input dependent instructions from input dependent functions over all instructions, as well as
    /// ration of input dependent basic blocks in input dependent functions over all basic blocks.
    /// Note that this function will not add input dependent instructions/blocks that are in input independent
    /// functions. The information collected by this function will be cached for further uses and can be invalidated
    /// by a call to \a invalidate_stats_data function.
    virtual void reportInputDepCoverage();

    /// Invalidates stat data cached so far. Note cached data will persist, unless this function is called.
    virtual void invalidate_stats_data();

private:
    void report_inputdepindep_data(const inputdepindep_data& data);
    void report_inputdep_data(const inputdep_data& data);
    void report_input_indep_coverage_data(const input_indep_coverage_data& data);
    void report_input_dep_coverage_data(const input_dep_coverage_data& data);
    void update_module_coverage_data(input_dep_coverage_data& module_coverage_data,
                                     const input_dep_coverage_data& function_coverage_data) const;
    void update_module_coverage_data(input_indep_coverage_data& module_coverage_data,
                                     const input_indep_coverage_data& function_coverage_data) const;

private:
    llvm::Module* m_module;
    InputDependencyAnalysisInfo* m_IDA; 

    // caching stats
    std::unordered_map<llvm::Function*, input_indep_coverage_data> m_function_input_indep_coverage_data;
    std::unordered_map<llvm::Function*, input_dep_coverage_data> m_function_input_dep_coverage_data;
};

class DummyInputDependencyStatistics : public InputDependencyStatistics
{
public:
    DummyInputDependencyStatistics() = default;

    void reportInputDepInputIndepRatio() override {}
    void reportInputDependencyInfo() override {}
    void reportInputInDepCoverage() override {}
    void reportInputDepCoverage() override {}
    void invalidate_stats_data() override {}

    void flush() override {}
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

