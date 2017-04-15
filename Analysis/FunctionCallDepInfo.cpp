#include "FunctionCallDepInfo.h"
#include "Utils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

namespace {

void finalizeDependencies(const FunctionCallDepInfo::ArgumentDependenciesMap& actualDeps,
                          FunctionCallDepInfo::ArgumentDependenciesMap& toFinalize)
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

void FunctionCallDepInfo::addDepInfo(const FunctionCallDepInfo& callsInfo)
{
    for (const auto& callItem : callsInfo.getCallsDependencies()) {
        addCall(callItem.first, callItem.second);
    }
    for (const auto& invokeItem : callsInfo.getInvokesDependencies()) {
        addInvoke(invokeItem.first, invokeItem.second);
    }
}

const FunctionCallDepInfo::CallInstrDepMap& FunctionCallDepInfo::getCallsDependencies() const
{
    return m_callsDeps;
}

const FunctionCallDepInfo::InvokeInstrDepMap& FunctionCallDepInfo::getInvokesDependencies() const
{
    return m_invokesDeps;
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

void FunctionCallDepInfo::finalize(const ArgumentDependenciesMap& actualDeps)
{
    for (auto& callItem : m_callsDeps) {
        finalizeDependencies(actualDeps, callItem.second);
    }
    for (auto& invokeItem : m_invokesDeps) {
        finalizeDependencies(actualDeps, invokeItem.second);
    }
}

}

