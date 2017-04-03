#pragma once

#include "DependencyInfo.h"

namespace llvm {

class Argument;
class CallInst;
class Function;
}

namespace input_dependency {

class FunctionCallDepInfo
{
public:
    using ArgumentDependenciesMap = std::unordered_map<llvm::Argument*, DepInfo>;
    using CallInstrDepMap = std::unordered_map<const llvm::CallInst*, ArgumentDependenciesMap>;

public:
    FunctionCallDepInfo(const llvm::Function& F);

public:
    void addCall(const llvm::CallInst* callInst, const ArgumentDependenciesMap& deps);
    void addCalls(const FunctionCallDepInfo& callsInfo);

    const ArgumentDependenciesMap& getDependenciesForCall(const llvm::CallInst* callInst) const;

    // Used in reflection algorithm.
    ArgumentDependenciesMap& getDependenciesForCall(const llvm::CallInst* callInst);

    ArgumentDependenciesMap getMergedDependencies() const;

    /**
    * \brief Finalize call instructions dependencies based on actual dependencies of caller.
    * \param actualDeps Argument dependencies map of caller function.
    */
    void finalize(const ArgumentDependenciesMap& actualDeps);

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
private:
    const llvm::Function& m_F;
    CallInstrDepMap m_callsDeps;
};

} // namespace input_dependency

