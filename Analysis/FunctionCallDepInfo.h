#pragma once

#include "DependencyInfo.h"

namespace llvm {

class Argument;
class CallInst;
class Function;
class InvokeInst;
}

namespace input_dependency {

class FunctionCallDepInfo
{
public:
    using ArgumentDependenciesMap = std::unordered_map<llvm::Argument*, DepInfo>;
    using CallInstrDepMap = std::unordered_map<const llvm::CallInst*, ArgumentDependenciesMap>;
    using InvokeInstrDepMap = std::unordered_map<const llvm::InvokeInst*, ArgumentDependenciesMap>;

public:
    FunctionCallDepInfo(const llvm::Function& F);

public:
    void addCall(const llvm::CallInst* callInst, const ArgumentDependenciesMap& deps);
    void addInvoke(const llvm::InvokeInst* invokeInst, const ArgumentDependenciesMap& deps);
    void addDepInfo(const FunctionCallDepInfo& depInfo);

    const CallInstrDepMap& getCallsDependencies() const;
    const InvokeInstrDepMap& getInvokesDependencies() const;

    const ArgumentDependenciesMap& getDependenciesForCall(const llvm::CallInst* callInst) const;
    const ArgumentDependenciesMap& getDependenciesForInvoke(const llvm::InvokeInst* invokeInst) const;

    // Used in reflection algorithm.
    ArgumentDependenciesMap& getDependenciesForCall(const llvm::CallInst* callInst);
    ArgumentDependenciesMap& getDependenciesForInvoke(const llvm::InvokeInst* invokeInst);

    ArgumentDependenciesMap getMergedDependencies() const;

    /**
    * \brief Finalize call instructions dependencies based on actual dependencies of caller.
    * \param actualDeps Argument dependencies map of caller function.
    */
    void finalize(const ArgumentDependenciesMap& actualDeps);

/*
public:
    using iterator = CallInstrDepMap::iterator;
    using const_iterator = CallInstrDepMap::const_iterator;

    iterator begin()
    {
        return m_callsDeps.begin();
    }

    iterator end()
    {
        return m_callsDeps.end();
    }

    const_iterator begin() const
    {
        return m_callsDeps.begin();
    }

    const_iterator end() const
    {
        return m_callsDeps.end();
    }
    */
private:
    const llvm::Function& m_F;
    CallInstrDepMap m_callsDeps;
    InvokeInstrDepMap m_invokesDeps;
};

} // namespace input_dependency

