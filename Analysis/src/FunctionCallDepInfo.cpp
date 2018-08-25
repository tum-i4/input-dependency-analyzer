#include "input-dependency/Analysis/Utils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

namespace {

// Note, only the dependency info of a global value is going to be finalized, the dependencies of elements are not
ValueDepInfo getFinalizedDepInfo(const std::unordered_map<llvm::GlobalVariable*, ValueDepInfo>& actualDeps,
                                 ValueSet& valueDeps)
{
    std::vector<llvm::Value*> values_to_erase;
    ValueDepInfo resultInfo;
    for (auto& dep : valueDeps) {
        auto* global = llvm::dyn_cast<llvm::GlobalVariable>(dep);
        if (global == nullptr) {
            values_to_erase.push_back(dep);
            continue;
        }
        auto pos = actualDeps.find(global);
        if (pos == actualDeps.end()) {
            llvm::dbgs() << "Function call info finalization.\n"
            << "    Function argument depends on global for which no input dep info is known. Global is: " << *global <<
            "\n";
            values_to_erase.push_back(global);
            continue;
        }
        values_to_erase.push_back(global);
        assert(pos->second.isDefined());
        ValueDepInfo global_depInfo = pos->second;
        ValueSet& globalDependencies = global_depInfo.getValueDependencies();
        if (global_depInfo.getDependency() == DepInfo::VALUE_DEP && !globalDependencies.empty()) {
            ValueSet seen;
            // assert(pos->second.isOnlyGlobalValueDependent());
            auto it = globalDependencies.begin();
            while (it != globalDependencies.end()) {
                auto d = *it;
                if (d == dep || seen.find(d) != seen.end()) {
                    ++it;
                    continue;
                }
                seen.insert(d);
                auto global = llvm::dyn_cast<llvm::GlobalVariable>(d);
                if (global == nullptr) {
                    ++it;
                    continue;
                }
                auto deps = actualDeps.find(global);
                if (deps != actualDeps.end()) {
                    global_depInfo.mergeDependencies(deps->second);
                }
                ++it;
            }
            for (auto s : seen) {
                if (globalDependencies.empty()) {
                    break;
                }
                globalDependencies.erase(s);
            }
            if (globalDependencies.empty() && global_depInfo.getDependency() == DepInfo::VALUE_DEP) {
                global_depInfo.setDependency(DepInfo::INPUT_INDEP);
            }
        } else {
            global_depInfo.getValueDependencies().clear();
            if (global_depInfo.isValueDep()) {
                global_depInfo.setDependency(DepInfo::INPUT_INDEP);
            }
        }
        //assert(!pos->second.isValueDep());
        //assert(!global_depInfo.isValueDep());
        resultInfo.mergeDependencies(global_depInfo);
    }
    std::for_each(values_to_erase.begin(), values_to_erase.end(), [&valueDeps] (llvm::Value* val)
    {valueDeps.erase(val);});
    return resultInfo;
}

template<class DepMapType>
void finalizeArgDeps(const FunctionCallDepInfo::ArgumentDependenciesMap& actualDeps,
                     DepMapType& toFinalize)
{
    auto it = toFinalize.begin();
    // TODO: what about don't erase, set to input indep?
    while (it != toFinalize.end()) {
        if (it->second.isInputIndep() ||
            (it->second.isInputArgumentDep() && !Utils::haveIntersection(actualDeps, it->second.getArgumentDependencies()))) {
            auto old = it;
            ++it;
            toFinalize.erase(old);
            continue;
        }
        it->second.setDependency(DepInfo::INPUT_DEP);
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
        //assert(!finalDeps.isValueDep());
        if (item.second.getDependency() == DepInfo::VALUE_DEP) {
            item.second.setDependency(finalDeps.getDependency());
        }
        if (item.second.getDependency() == DepInfo::VALUE_DEP && item.second.getValueDependencies().empty()) {
            item.second.setDependency(DepInfo::INPUT_INDEP);
        }
        item.second.mergeDependencies(finalDeps);
    }
}

}

FunctionCallDepInfo::FunctionCallDepInfo()
    : m_F(nullptr)
    , m_isCallback(false)
{
}

FunctionCallDepInfo::FunctionCallDepInfo(const llvm::Function& F)
    : m_F(&F)
    , m_isCallback(false)
{
}

bool FunctionCallDepInfo::empty() const
{
    return m_callsArgumentsDeps.empty() && m_callsGlobalsDeps.empty();
}

void FunctionCallDepInfo::addCall(const llvm::CallInst* callInst, const ArgumentDependenciesMap& deps)
{
    if (auto F = callInst->getCalledFunction()) {
        // if callInst is virtual call, then CalledFunction is going to be null
        assert(F == m_F);
    }
    addCallSiteArguments(callInst, deps);
}

void FunctionCallDepInfo::addInvoke(const llvm::InvokeInst* invokeInst, const ArgumentDependenciesMap& deps)
{
    if (auto F = invokeInst->getCalledFunction()) {
        assert(F == m_F);
    }
    addCallSiteArguments(invokeInst, deps);
}

void FunctionCallDepInfo::addCall(const llvm::CallInst* callInst, const GlobalVariableDependencyMap& deps)
{
    if (auto F = callInst->getCalledFunction()) {
        // if callInst is virtual call, then CalledFunction is going to be null
        assert(F == m_F);
    }
    addCallSiteGlobals(callInst, deps);
}

void FunctionCallDepInfo::addInvoke(const llvm::InvokeInst* invokeInst, const GlobalVariableDependencyMap& deps)
{
    if (auto F = invokeInst->getCalledFunction()) {
        // if callInst is virtual call, then CalledFunction is going to be null
        assert(F == m_F);
    }
    addCallSiteGlobals(invokeInst, deps);
}

void FunctionCallDepInfo::addCall(const llvm::Instruction* instr, const ArgumentDependenciesMap& deps)
{
    assert(isValidInstruction(instr));
    addCallSiteArguments(instr, deps);
}

void FunctionCallDepInfo::addCall(const llvm::Instruction* instr, const GlobalVariableDependencyMap& deps)
{
    assert(isValidInstruction(instr));
    addCallSiteGlobals(instr, deps);
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

void FunctionCallDepInfo::removeCall(const llvm::Instruction* callInst)
{
    m_callsArgumentsDeps.erase(callInst);
    m_callsGlobalsDeps.erase(callInst);
}

const InstrSet& FunctionCallDepInfo::getCallSites() const
{
    return m_callSites;
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
    if (m_isCallback) {
        auto arg_it = m_F->arg_begin();
        while (arg_it != m_F->arg_end()) {
            mergedDeps[const_cast<llvm::Argument*>(&*arg_it)] = ValueDepInfo(DepInfo(DepInfo::INPUT_DEP));
            ++arg_it;
        }
        return mergedDeps;
    }
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

bool FunctionCallDepInfo::isValidInstruction(const llvm::Instruction* instr) const
{
    llvm::Function* calledF;
    if (const auto call = llvm::dyn_cast<llvm::CallInst>(instr)) {
        calledF = call->getCalledFunction();
    } else if (const auto invoke = llvm::dyn_cast<llvm::InvokeInst>(instr)){
        calledF = invoke->getCalledFunction();
    } else {
        return false;
    }
    return !calledF || (calledF == m_F);
}

void FunctionCallDepInfo::addCallSiteArguments(const llvm::Instruction* instr, const ArgumentDependenciesMap& argDeps)
{
    auto res = m_callsArgumentsDeps.insert(std::make_pair(instr, argDeps));
    m_callSites.insert(const_cast<llvm::Instruction*>(instr));
    if (!res.second) {
        //llvm::dbgs() << "FunctionCallDepInfo: arguments information for call site " << *instr << " is already collected\n";
    }
}

void FunctionCallDepInfo::addCallSiteGlobals(const llvm::Instruction* instr, const GlobalVariableDependencyMap& globalDeps)
{
    auto res = m_callsGlobalsDeps.insert(std::make_pair(instr, globalDeps));
    m_callSites.insert(const_cast<llvm::Instruction*>(instr));
    if (!res.second) {
        //llvm::dbgs() << "FunctionCallDepInfo: globals information for call site " << *instr << " is already collected\n";
    }
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

}

