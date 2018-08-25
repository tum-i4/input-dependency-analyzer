#pragma once

#include "input-dependency/Analysis/InputDependencyAnalysisInterface.h"
#include "llvm/Pass.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace llvm {
class Module;
}

namespace input_dependency {

//class InputDependencyAnalysisInterface;

class InputDependencyAnalysisPass : public llvm::ModulePass
{
public:
    using InputDependencyAnalysisType = std::shared_ptr<InputDependencyAnalysisInterface>;

public:
    static char ID;

    InputDependencyAnalysisPass()
        : llvm::ModulePass(ID)
    {
    }

public:
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;

public:
    InputDependencyAnalysisType getInputDependencyAnalysis()
    {
        return m_analysis;
    }

    const InputDependencyAnalysisType& getInputDependencyAnalysis() const
    {
        return m_analysis;
    }
    
private:
    bool has_cached_input_dependency() const;
    void create_input_dependency_analysis(const InputDependencyAnalysisInterface::AliasAnalysisInfoGetter& AARGetter);
    void create_cached_input_dependency_analysis();
    std::unordered_set<llvm::Function*> get_main_non_reachable_functions();
    void mark_main_reachable_functions(const std::unordered_set<llvm::Function*>& functions);
    void dump_statistics(const std::unordered_set<llvm::Function*>& functions);

private:
    llvm::Module* m_module;
    InputDependencyAnalysisType m_analysis;
};

}

