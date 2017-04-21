#pragma once

#include "DependencyInfo.h"

namespace llvm {

class Argument;
class CallInst;
class Function;
class GlobalVariable;
class InvokeInst;
}

namespace input_dependency {

class FunctionCallDepInfo
{
public:
    using ArgumentDependenciesMap = std::unordered_map<llvm::Argument*, DepInfo>;
    using GlobalVariableDependencyMap = std::unordered_map<llvm::GlobalVariable*, DepInfo>;
    using CallInstrArgumentDepMap = std::unordered_map<const llvm::CallInst*, ArgumentDependenciesMap>;
    using InvokeInstrArgumentDepMap = std::unordered_map<const llvm::InvokeInst*, ArgumentDependenciesMap>;
    using CallInstrGlobalsDepMap = std::unordered_map<const llvm::CallInst*, GlobalVariableDependencyMap>;
    using InvokeInstrGlobalsDepMap = std::unordered_map<const llvm::InvokeInst*, GlobalVariableDependencyMap>;

public:
    FunctionCallDepInfo(const llvm::Function& F);

public:
    void addCall(const llvm::CallInst* callInst, const ArgumentDependenciesMap& deps);
    void addInvoke(const llvm::InvokeInst* invokeInst, const ArgumentDependenciesMap& deps);
    void addCall(const llvm::CallInst* callInst, const GlobalVariableDependencyMap& deps);
    void addInvoke(const llvm::InvokeInst* invokeInst, const GlobalVariableDependencyMap& deps);
    void addDepInfo(const FunctionCallDepInfo& depInfo);

    const CallInstrArgumentDepMap& getCallsDependencies() const;
    const InvokeInstrArgumentDepMap& getInvokesDependencies() const;
    const CallInstrGlobalsDepMap& getCallsGlobalsDependencies() const;
    const InvokeInstrGlobalsDepMap& getInvokesGlobalsDependencies() const;

    const ArgumentDependenciesMap& getDependenciesForCall(const llvm::CallInst* callInst) const;
    const ArgumentDependenciesMap& getDependenciesForInvoke(const llvm::InvokeInst* invokeInst) const;
    const GlobalVariableDependencyMap& getGlobalsDependenciesForCall(const llvm::CallInst* callInst) const;
    const GlobalVariableDependencyMap& getGlobalsDependenciesForInvoke(const llvm::InvokeInst* invokeInst) const;

    // Used in reflection algorithm.
    ArgumentDependenciesMap& getDependenciesForCall(const llvm::CallInst* callInst);
    ArgumentDependenciesMap& getDependenciesForInvoke(const llvm::InvokeInst* invokeInst);
    GlobalVariableDependencyMap& getGlobalsDependenciesForCall(const llvm::CallInst* callInst);
    GlobalVariableDependencyMap& getGlobalsDependenciesForInvoke(const llvm::InvokeInst* invokeInst);

    ArgumentDependenciesMap getMergedDependencies() const;
    GlobalVariableDependencyMap getGlobalsMergedDependencies() const;

    /**
    * \brief Finalize call instructions dependencies based on actual dependencies of caller.
    * \param actualDeps Argument dependencies map of caller function.
    */
    void finalizeArgumentDependencies(const ArgumentDependenciesMap& actualDeps);
    void finalizeGlobalsDependencies(const GlobalVariableDependencyMap& actualDeps);

private:
    const llvm::Function& m_F;
    CallInstrArgumentDepMap m_callsDeps;
    InvokeInstrArgumentDepMap m_invokesDeps;
    CallInstrGlobalsDepMap m_callsGlobalDeps;
    InvokeInstrGlobalsDepMap m_invokesGlobalDeps;
};

} // namespace input_dependency

