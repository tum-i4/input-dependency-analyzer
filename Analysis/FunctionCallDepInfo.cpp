#include "FunctionCallDepInfo.h"
#include "Utils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

namespace {

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
    assert(callInst->getCalledFunction() == &m_F);
    auto res = m_callsDeps.insert(std::make_pair(callInst, deps));
    assert(res.second);
}

void FunctionCallDepInfo::addInvoke(const llvm::InvokeInst* invokeInst, const ArgumentDependenciesMap& deps)
{
    assert(invokeInst->getCalledFunction() == &m_F);
    auto res = m_invokesDeps.insert(std::make_pair(invokeInst, deps));
    assert(res.second);
}

void FunctionCallDepInfo::addCall(const llvm::CallInst* callInst, const GlobalVariableDependencyMap& deps)
{
    assert(callInst->getCalledFunction() == &m_F);
    auto res = m_callsGlobalDeps.insert(std::make_pair(callInst, deps));
    assert(res.second);
}

void FunctionCallDepInfo::addInvoke(const llvm::InvokeInst* invokeInst, const GlobalVariableDependencyMap& deps)
{
    assert(invokeInst->getCalledFunction() == &m_F);
    auto res = m_invokesGlobalDeps.insert(std::make_pair(invokeInst, deps));
    assert(res.second);
}

void FunctionCallDepInfo::addDepInfo(const FunctionCallDepInfo& callsInfo)
{
    for (const auto& callItem : callsInfo.getCallsDependencies()) {
        addCall(callItem.first, callItem.second);
    }
    for (const auto& invokeItem : callsInfo.getInvokesDependencies()) {
        addInvoke(invokeItem.first, invokeItem.second);
    }
    for (const auto& callItem : callsInfo.getCallsGlobalsDependencies()) {
        addCall(callItem.first, callItem.second);
    }
    for (const auto& invokeItem : callsInfo.getInvokesGlobalsDependencies()) {
        addInvoke(invokeItem.first, invokeItem.second);
    }
}

const FunctionCallDepInfo::CallInstrArgumentDepMap& FunctionCallDepInfo::getCallsDependencies() const
{
    return m_callsDeps;
}

const FunctionCallDepInfo::InvokeInstrArgumentDepMap& FunctionCallDepInfo::getInvokesDependencies() const
{
    return m_invokesDeps;
}

const FunctionCallDepInfo::CallInstrGlobalsDepMap& FunctionCallDepInfo::getCallsGlobalsDependencies() const
{
    return m_callsGlobalDeps;
}

const FunctionCallDepInfo::InvokeInstrGlobalsDepMap& FunctionCallDepInfo::getInvokesGlobalsDependencies() const
{
    return m_invokesGlobalDeps;
}

const FunctionCallDepInfo::ArgumentDependenciesMap&
FunctionCallDepInfo::getDependenciesForCall(const llvm::CallInst* callInst) const
{
    return const_cast<FunctionCallDepInfo*>(this)->getDependenciesForCall(callInst);
}

const FunctionCallDepInfo::ArgumentDependenciesMap&
FunctionCallDepInfo::getDependenciesForInvoke(const llvm::InvokeInst* invokeInst) const
{
    return const_cast<FunctionCallDepInfo*>(this)->getDependenciesForInvoke(invokeInst);
}

const FunctionCallDepInfo::GlobalVariableDependencyMap&
FunctionCallDepInfo::getGlobalsDependenciesForCall(const llvm::CallInst* callInst) const
{
    return const_cast<FunctionCallDepInfo*>(this)->getGlobalsDependenciesForCall(callInst);
}

const FunctionCallDepInfo::GlobalVariableDependencyMap&
FunctionCallDepInfo::getGlobalsDependenciesForInvoke(const llvm::InvokeInst* invokeInst) const
{
    return const_cast<FunctionCallDepInfo*>(this)->getGlobalsDependenciesForInvoke(invokeInst);
}

FunctionCallDepInfo::ArgumentDependenciesMap&
FunctionCallDepInfo::getDependenciesForCall(const llvm::CallInst* callInst)
{
    auto pos = m_callsDeps.find(callInst);
    assert(pos != m_callsDeps.end());
    return pos->second;
}

FunctionCallDepInfo::ArgumentDependenciesMap&
FunctionCallDepInfo::getDependenciesForInvoke(const llvm::InvokeInst* invokeInst)
{
    auto pos = m_invokesDeps.find(invokeInst);
    assert(pos != m_invokesDeps.end());
    return pos->second;
}

FunctionCallDepInfo::GlobalVariableDependencyMap&
FunctionCallDepInfo::getGlobalsDependenciesForCall(const llvm::CallInst* callInst)
{
    auto pos = m_callsGlobalDeps.find(callInst);
    assert(pos != m_callsGlobalDeps.end());
    return pos->second;
}

FunctionCallDepInfo::GlobalVariableDependencyMap&
FunctionCallDepInfo::getGlobalsDependenciesForInvoke(const llvm::InvokeInst* invokeInst)
{
    auto pos = m_invokesGlobalDeps.find(invokeInst);
    assert(pos != m_invokesGlobalDeps.end());
    return pos->second;
}

FunctionCallDepInfo::ArgumentDependenciesMap FunctionCallDepInfo::getMergedDependencies() const
{
    ArgumentDependenciesMap mergedDeps;
    for (const auto& item : m_callsDeps) {
        for (const auto& argItem : item.second) {
            mergedDeps[argItem.first].mergeDependencies(argItem.second);
        }
    }
    for (const auto& item : m_invokesDeps) {
        for (const auto& argItem : item.second) {
            mergedDeps[argItem.first].mergeDependencies(argItem.second);
        }
    }
    return mergedDeps;
}


FunctionCallDepInfo::GlobalVariableDependencyMap FunctionCallDepInfo::getGlobalsMergedDependencies() const
{
    GlobalVariableDependencyMap mergedDeps;
    for (const auto& item : m_callsGlobalDeps) {
        for (const auto& globalItem : item.second) {
            mergedDeps[globalItem.first].mergeDependencies(globalItem.second);
        }
    }
    for (const auto& item : m_invokesGlobalDeps) {
        for (const auto& globalItem : item.second) {
            mergedDeps[globalItem.first].mergeDependencies(globalItem.second);
        }
    }
    return mergedDeps;
}

void FunctionCallDepInfo::finalizeArgumentDependencies(const ArgumentDependenciesMap& actualDeps)
{
    for (auto& callItem : m_callsDeps) {
        finalizeArgDeps(actualDeps, callItem.second);
    }
    for (auto& invokeItem : m_invokesDeps) {
        finalizeArgDeps(actualDeps, invokeItem.second);
    }
    for (auto& callItem : m_callsGlobalDeps) {
        finalizeArgDeps(actualDeps, callItem.second);
    }
    for (auto& invokeItem : m_invokesGlobalDeps) {
        finalizeArgDeps(actualDeps, invokeItem.second);
    }
}

void FunctionCallDepInfo::finalizeGlobalsDependencies(const GlobalVariableDependencyMap& actualDeps)
{
    for (auto& callItem : m_callsDeps) {
        finalizeGlobalsDeps(actualDeps, callItem.second);
    }
    for (auto& invokeItem : m_invokesDeps) {
        finalizeGlobalsDeps(actualDeps, invokeItem.second);
    }
    for (auto& callItem : m_callsGlobalDeps) {
        finalizeGlobalsDeps(actualDeps, callItem.second);
    }
    for (auto& invokeItem : m_invokesGlobalDeps) {
        finalizeGlobalsDeps(actualDeps, invokeItem.second);
    }
}

}

