#pragma once

#include "InputDependencyAnalysis.h"

#include "llvm/Pass.h"

#include <memory>

namespace llvm {
class Module;
}


namespace input_dependency {

class InputDependencyStatistics
{
public:
    class ReportWriter;
    // default format is JSON
    enum Format {
        TEXT,
        JSON
    };

public:
    InputDependencyStatistics(const std::string& format, const std::string& file_name);

public:
    void report(llvm::Module& M,
                const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo);
    void reportInputDepInputIndepRatio(llvm::Module& M,
                                       const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo);
    void reportInputDependencyInfo(llvm::Module& M,
                                   const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo);
    void reportCloningInformation(llvm::Module& M,
                                  const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo);
    void reportExtractionInformation(llvm::Module& M,
                                     const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo);
    void reportInputDepCoverage(llvm::Module& M,
                                const InputDependencyAnalysis::InputDependencyAnalysisInfo& inputDepInfo);

private:
    std::shared_ptr<ReportWriter> m_writer;
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

