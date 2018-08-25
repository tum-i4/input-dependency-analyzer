#pragma once

#include "input-dependency/Transforms/FunctionClone.h"
#include "input-dependency/Analysis/InputDependencyAnalysisPass.h"
#include "input-dependency/Analysis/Statistics.h"
#include "input-dependency/Analysis/InputDependencyStatistics.h"

#include "llvm/Pass.h"
#include <memory>

#include <iostream>

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
    CloneStatistics(const std::string& module_name,
                    const std::string& format,
                    const std::string& file_name)
        : Statistics(format, file_name)
        , m_module_name(module_name)
        , m_numOfClonnedInst(0)
        , m_numOfInstAfterCloning(0)
        , m_numOfInDepInstAfterCloning(0)
    {
    }

    CloneStatistics(Statistics::ReportWriterType writer)
        : Statistics(writer)
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

    virtual void add_numOfClonnedInst(unsigned num)
    {
        m_numOfClonnedInst += num;
    }

    virtual void add_numOfInstAfterCloning(unsigned num)
    {
        m_numOfInstAfterCloning += num;
    }

    virtual void remove_numOfInstAfterCloning(unsigned num)
    {
        if (num > m_numOfInstAfterCloning) {
            m_numOfInstAfterCloning = 0;
        } else {
            m_numOfInstAfterCloning -= num;
        }
    }

    virtual void add_numOfInDepInstAfterCloning(unsigned num)
    {
        m_numOfInDepInstAfterCloning += num;
    }

    virtual void add_clonnedFunction(const std::string& name)
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

class DummyCloneStatistics : public CloneStatistics
{
public:
    DummyCloneStatistics() = default;

    void report() override {}
    void flush() override {}

    void add_numOfClonnedInst(unsigned num) override
    {}

    void add_numOfInstAfterCloning(unsigned num) override
    {}

    void remove_numOfInstAfterCloning(unsigned num) override
    {}

    void add_numOfInDepInstAfterCloning(unsigned num) override
    {}

    void add_clonnedFunction(const std::string& name) override
    {}
};

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
                        llvm::Function* calledF,
                        bool& uses_original);
    InputDepRes getFunctionInputDepInfo(llvm::Function* F) const;
    std::pair<llvm::Function*, bool> doCloneForArguments(
                                            llvm::Function* calledF,
                                            InputDepRes original_analiser,
                                            FunctionClone& clone,
                                            const input_dependency::FunctionCallDepInfo::ArgumentDependenciesMap& argDeps);

    void remove_unused_originals(const std::unordered_map<llvm::Function*, bool>& original_uses);
    void createStatistics(llvm::Module& M);
    void dump() const;

private:
    input_dependency::InputDependencyAnalysisPass::InputDependencyAnalysisType IDA;
    using FunctionCloneInfo = std::unordered_map<llvm::Function*, FunctionClone>;
    FunctionCloneInfo m_functionCloneInfo;
    std::unordered_map<llvm::Function*, llvm::Function*> m_clone_to_original;
    using CloneStatisticsType = std::shared_ptr<CloneStatistics>;
    CloneStatisticsType m_cloneStatistics;
    using CoverageStatisticsType = std::shared_ptr<input_dependency::InputDependencyStatistics>;
    CoverageStatisticsType m_coverageStatistics;
};

}

