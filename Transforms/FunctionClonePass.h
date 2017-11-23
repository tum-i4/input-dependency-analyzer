#pragma once

#include "FunctionClone.h"
#include "Analysis/InputDependencyAnalysis.h"
#include "Analysis/Statistics.h"

#include "llvm/Pass.h"

namespace input_dependency {
class InputDependencyAnalysis;
class FunctionAnaliser;
}

namespace llvm {
class Function;
class Instruction;
class Module;
}

namespace oh {

class CloneStatistics : public input_dependency::Statistics
{
public:
    CloneStatistics() = default;
    CloneStatistics(const std::string& format,
                    const std::string& file_name)
        : Statistics(format, file_name)
        , m_numOfClonnedInst(0)
        , m_numOfInstAfterCloning(0)
        , m_numOfInDepInstAfterCloning(0)
    {
    }

public:
    void report() override;

    void set_module_name(const std::string& name)
    {
        m_module_name = name;
    }

    void add_numOfClonnedInst(unsigned num)
    {
        m_numOfClonnedInst += num;
    }

    void add_numOfInstAfterCloning(unsigned num)
    {
        m_numOfInstAfterCloning += num;
    }

    void add_numOfInDepInstAfterCloning(unsigned num)
    {
        m_numOfInDepInstAfterCloning += num;
    }

    void add_clonnedFunction(const std::string& name)
    {
        m_clonnedFuncs.push_back(name);
    }

private:
    std::string m_module_name;
    unsigned m_numOfClonnedInst;
    unsigned m_numOfInstAfterCloning;
    unsigned m_numOfInDepInstAfterCloning;
    std::vector<std::string> m_clonnedFuncs;
}; // class CloneStatistics

class FunctionClonePass : public llvm::ModulePass
{
public:
    static char ID;

    FunctionClonePass()
        : llvm::ModulePass(ID)
    {
    }

public:
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;

private:
    using FunctionSet = std::unordered_set<llvm::Function*>;
    using InputDepRes = input_dependency::InputDependencyAnalysis::InputDepResType;
    FunctionSet doClone(const InputDepRes& analiser,
                        llvm::Function* calledF);
    InputDepRes getFunctionInputDepInfo(llvm::Function* F) const;
    std::pair<llvm::Function*, bool> doCloneForArguments(
                                            llvm::Function* calledF,
                                            InputDepRes original_analiser,
                                            FunctionClone& clone,
                                            const input_dependency::FunctionCallDepInfo::ArgumentDependenciesMap& argDeps);

    void initialize_statistics();
    void dump() const;

private:
    input_dependency::InputDependencyAnalysis* IDA;
    using FunctionCloneInfo = std::unordered_map<llvm::Function*, FunctionClone>;
    FunctionCloneInfo m_functionCloneInfo;
    std::unordered_map<llvm::Function*, llvm::Function*> m_clone_to_original;
    CloneStatistics m_statistics;
};

}

