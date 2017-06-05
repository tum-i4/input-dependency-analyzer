#include "FunctionCallDepInfo.h"
#include "Utils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

namespace {

DepInfo getFinalizedDepInfo(const std::unordered_map<llvm::GlobalVariable*, DepInfo>& actualDeps,
                            const ValueSet& valueDeps)
{
    DepInfo resultInfo;
    for (auto& dep : valueDeps) {
        auto global = llvm::dyn_cast<llvm::GlobalVariable>(dep);
        assert(global != nullptr);
        auto pos = actualDeps.find(global);
        if (pos == actualDeps.end()) {
            continue;
        }
        assert(pos->second.isDefined());
        assert(!pos->second.isValueDep());
        resultInfo.mergeDependencies(pos->second);
    }
    return resultInfo;
}

template<class DepMapType>
void finalizeArgDeps(const FunctionCallDepInfo::ArgumentDependenciesMap& actualDeps,
                     DepMapType& toFinalize)
{
    auto it = toFinalize.begin();
    while (it != toFinalize.end()) {
        if (it->second.isInputArgumentDep() && !Utils::haveIntersection(actualDeps, it->second.getArgumentDependencies())) {
            auto old = it;
            ++it;
            toFinalize.erase(old);
            continue;
        }
        ++it;
    }
}

template<class DepMapType>
void finalizeGlobalsDeps(const FunctionCallDepInfo::GlobalVariableDependencyMap& actualDeps,
                         DepMapType& toFinalize)
{
        for (auto& item : toFinalize) {
        if (!item.second.isValueDep()) {
            continue;
        }
        const auto& finalDeps = getFinalizedDepInfo(actualDeps, item.second.getValueDependencies());
        assert(!finalDeps.isValueDep());
        if (item.second.getDependency() == DepInfo::VALUE_DEP) {
            item.second.setDependency(finalDeps.getDependency());
        }
        item.second.mergeDependencies(finalDeps);
    }
}

}

FunctionCallDepInfo::FunctionCallDepInfo(const llvm::Function& F)
                                : m_F(F)
{
}

void FunctionCallDepInfo::addCall(const llvm::CallInst* callInst, const ArgumentDependenciesMap& deps)
{
    if (auto F = callInst->getCalledFunction()) {
        // if callInst is virtual call, then CalledFunction is going to be null
        assert(F == &m_F);
    }
    addCallSiteArguments(callInst, deps);
}

void FunctionCallDepInfo::addInvoke(const llvm::InvokeInst* invokeInst, const ArgumentDependenciesMap& deps)
{
    if (auto F = invokeInst->getCalledFunction()) {
        assert(F == &m_F);
    }
    addCallSiteArguments(invokeInst, deps);
}

void FunctionCallDepInfo::addCall(const llvm::CallInst* callInst, const GlobalVariableDependencyMap& deps)
{
    if (auto F = callInst->getCalledFunction()) {
        // if callInst is virtual call, then CalledFunction is going to be null
        assert(F == &m_F);
    }
    addCallSiteGlobals(callInst, deps);
}

void FunctionCallDepInfo::addInvoke(const llvm::InvokeInst* invokeInst, const GlobalVariableDependencyMap& deps)
{
    if (auto F = invokeInst->getCalledFunction()) {
        // if callInst is virtual call, then CalledFunction is going to be null
        assert(F == &m_F);
    }
    addCallSiteGlobals(invokeInst, deps);
}

void FunctionCallDepInfo::addDepInfo(const FunctionCallDepInfo& callsInfo)
{
    for (const auto& callItem : callsInfo.getCallsArgumentDependencies()) {
        addCallSiteArguments(callItem.first, callItem.second);
    }
    for (const auto& callItem : callsInfo.getCallsGlobalsDependencies()) {
        addCallSiteGlobals(callItem.first, callItem.second);
    }
}

const FunctionCallDepInfo::CallSiteArgumentsDependenciesMap& FunctionCallDepInfo::getCallsArgumentDependencies() const
{
    return m_callsArgumentsDeps;
}

const FunctionCallDepInfo::CallSiteGloblasDependenciesMap& FunctionCallDepInfo::getCallsGlobalsDependencies() const
{
    return m_callsGlobalsDeps;
}

const FunctionCallDepInfo::ArgumentDependenciesMap&
FunctionCallDepInfo::getArgumentDependenciesForCall(const llvm::CallInst* callInst) const
{
    return const_cast<FunctionCallDepInfo*>(this)->getArgumentsDependencies(callInst);
}

const FunctionCallDepInfo::ArgumentDependenciesMap&
FunctionCallDepInfo::getArgumentDependenciesForInvoke(const llvm::InvokeInst* invokeInst) const
{
    return const_cast<FunctionCallDepInfo*>(this)->getArgumentsDependencies(invokeInst);
}

const FunctionCallDepInfo::GlobalVariableDependencyMap&
FunctionCallDepInfo::getGlobalsDependenciesForCall(const llvm::CallInst* callInst) const
{
    return const_cast<FunctionCallDepInfo*>(this)->getGlobalsDependencies(callInst);
}

const FunctionCallDepInfo::GlobalVariableDependencyMap&
FunctionCallDepInfo::getGlobalsDependenciesForInvoke(const llvm::InvokeInst* invokeInst) const
{
    return const_cast<FunctionCallDepInfo*>(this)->getGlobalsDependencies(invokeInst);
}

FunctionCallDepInfo::ArgumentDependenciesMap&
FunctionCallDepInfo::getArgumentDependenciesForCall(const llvm::CallInst* callInst)
{
    return getArgumentsDependencies(callInst);
}

FunctionCallDepInfo::ArgumentDependenciesMap&
FunctionCallDepInfo::getArgumentDependenciesForInvoke(const llvm::InvokeInst* invokeInst)
{
    return getArgumentsDependencies(invokeInst);
}

FunctionCallDepInfo::GlobalVariableDependencyMap&
FunctionCallDepInfo::getGlobalsDependenciesForCall(const llvm::CallInst* callInst)
{
    return getGlobalsDependencies(callInst);
}

FunctionCallDepInfo::GlobalVariableDependencyMap&
FunctionCallDepInfo::getGlobalsDependenciesForInvoke(const llvm::InvokeInst* invokeInst)
{
    return getGlobalsDependencies(invokeInst);
}

FunctionCallDepInfo::ArgumentDependenciesMap FunctionCallDepInfo::getMergedArgumentDependencies() const
{
    ArgumentDependenciesMap mergedDeps;
    for (const auto& item : m_callsArgumentsDeps) {
        for (const auto& argItem : item.second) {
            mergedDeps[argItem.first].mergeDependencies(argItem.second);
        }
    }
    return mergedDeps;
}


FunctionCallDepInfo::GlobalVariableDependencyMap FunctionCallDepInfo::getMergedGlobalsDependencies() const
{
    GlobalVariableDependencyMap mergedDeps;
    for (const auto& item : m_callsGlobalsDeps) {
        for (const auto& globalItem : item.second) {
            mergedDeps[globalItem.first].mergeDependencies(globalItem.second);
        }
    }
    return mergedDeps;
}

void FunctionCallDepInfo::finalizeArgumentDependencies(const ArgumentDependenciesMap& actualDeps)
{
    for (auto& callItem : m_callsArgumentsDeps) {
        finalizeArgDeps(actualDeps, callItem.second);
    }
    for (auto& callItem : m_callsGlobalsDeps) {
        finalizeArgDeps(actualDeps, callItem.second);
    }
}

void FunctionCallDepInfo::finalizeGlobalsDependencies(const GlobalVariableDependencyMap& actualDeps)
{
    for (auto& callItem : m_callsArgumentsDeps) {
        finalizeGlobalsDeps(actualDeps, callItem.second);
    }
    for (auto& callItem : m_callsGlobalsDeps) {
        finalizeGlobalsDeps(actualDeps, callItem.second);
    }
}

void FunctionCallDepInfo::markAllInputDependent()
{
    for (auto& callItem : m_callsArgumentsDeps) {
        markAllInputDependent(callItem.second);
    }
    for (auto& globalItem : m_callsGlobalsDeps) {
        markAllInputDependent(globalItem.second);
    }
}

void FunctionCallDepInfo::addCallSiteArguments(const llvm::Instruction* instr, const ArgumentDependenciesMap& argDeps)
{
    auto res = m_callsArgumentsDeps.insert(std::make_pair(instr, argDeps));
    assert(res.second);
}

void FunctionCallDepInfo::addCallSiteGlobals(const llvm::Instruction* instr, const GlobalVariableDependencyMap& globalDeps)
{
    auto res = m_callsGlobalsDeps.insert(std::make_pair(instr, globalDeps));
    assert(res.second);
}

FunctionCallDepInfo::ArgumentDependenciesMap& FunctionCallDepInfo::getArgumentsDependencies(const llvm::Instruction* instr)
{
    auto pos = m_callsArgumentsDeps.find(instr);
    assert(pos != m_callsArgumentsDeps.end());
    return pos->second;
}

FunctionCallDepInfo::GlobalVariableDependencyMap& FunctionCallDepInfo::getGlobalsDependencies(const llvm::Instruction* instr)
{
    auto pos = m_callsGlobalsDeps.find(instr);
    assert(pos != m_callsGlobalsDeps.end());
    return pos->second;
}

template <class Key>
void FunctionCallDepInfo::markAllInputDependent(std::unordered_map<Key, DepInfo>& argDeps)
{
    DepInfo info(DepInfo::INPUT_DEP);
    for (auto& item : argDeps) {
        item.second = info;
    }
}

}

