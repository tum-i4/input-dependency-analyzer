#include "ReflectingBasicBlockAnaliser.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/OperandTraits.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

namespace {

DependencyAnaliser::ValueDependencies mergeValueDependencies(const std::vector<DependencyAnaliser::ValueDependencies>& dependencies)
{
    DependencyAnaliser::ValueDependencies mergedDependencies;
    for (const auto& deps : dependencies) {
        for (const auto& valDeps : deps) {
            assert(!valDeps.second.isValueDep());
            auto res = mergedDependencies.insert(valDeps);
            if (res.second) {
                continue;
            }
            res.first->second.mergeDependencies(valDeps.second);
        }
    }
    return mergedDependencies;
}

DependencyAnaliser::ValueDependencies mergeSuccessorDependenciesWithInitialDependencies(
                                                    const DependencyAnaliser::ValueDependencies& initialDependencies,
                                                    const DependencyAnaliser::ValueDependencies& mergeInto)
{
    auto finalMerged = mergeInto;
    for (const auto& initialDep : initialDependencies) {
        finalMerged.insert(initialDep);
    }
    return finalMerged;
}

} // unnamed namespace

ReflectingBasicBlockAnaliser::ReflectingBasicBlockAnaliser(
                        llvm::Function* F,
                        llvm::AAResults& AAR,
                        const Arguments& inputs,
                        const FunctionAnalysisGetter& Fgetter,
                        llvm::BasicBlock* BB)
                    : BasicBlockAnalysisResult(F, AAR, inputs, Fgetter, BB)
                    , m_isReflected(false)
{
}

// DUBUG
void ReflectingBasicBlockAnaliser::dumpResults() const
{
    BasicBlockAnalysisResult::dumpResults();

    llvm::dbgs() << "Value dependent instructions\n";
    for (const auto& item : m_instructionValueDependencies) {
        llvm::dbgs() << *item.first << "\n";
        for (const auto& dep : item.second.getValueDependencies()) {
            llvm::dbgs() << "       " << *dep << "\n";
        }
    }

    llvm::dbgs() << "Value dependent values\n";
    for (const auto& item : m_valueDependencies) {
        if (!item.second.isValueDep()) {
            continue;
        }
        llvm::dbgs() << *item.first << "\n";
        for (const auto& dep : item.second.getValueDependencies()) {
            llvm::dbgs() << "       " << *dep << "\n";
        }
    }

    llvm::dbgs() << "Value dependent out arguments\n";
    for (const auto& item : m_valueDependentOutArguments) {
        for (const auto& arg : item.second) {
            llvm::dbgs() << *arg << " depends on " << *item.first << "\n"; 
        }
    }
    
    llvm::dbgs() << "Value dependent called functions arguments\n";
    for (const auto& item : m_valueDependentFunctionArguments) {
        for (const auto& fdep : item.second) {
            llvm::dbgs() << "Function " << fdep.first->getName() << " argument \n";
            for (const auto& dep : fdep.second) {
                llvm::dbgs() << *dep << " depends on value " << *item.first << "\n";
            }
        }
    }

    llvm::dbgs() << "Value dependencies\n";
    for (const auto& item : m_valueDependentValues) {
        llvm::dbgs() << "Following values depend on " << *item.first << "\n";
        for (const auto& val : item.second) {
            llvm::dbgs() << "    " << *val << "\n";
        }
    }
    llvm::dbgs() << "\n";
}

void ReflectingBasicBlockAnaliser::reflect(const DependencyAnaliser::ValueDependencies& initialDependencies)
{
    addOnValueDependencies(initialDependencies);
    resolveValueDependencies(initialDependencies);
    for (auto& item : m_valueDependencies) {
        //llvm::dbgs() << *item.first << "\n";
        reflect(item.first, item.second);
    }
    assert(m_valueDependentValues.empty());
    assert(m_valueDependentInstrs.empty());
    assert(m_valueDependentOutArguments.empty());
    assert(m_valueDependentFunctionArguments.empty());
    m_isReflected = true;
}

void ReflectingBasicBlockAnaliser::reflect(const std::vector<DependencyAnaliser::ValueDependencies>& dependencies,
                                           const DependencyAnaliser::ValueDependencies& initialDependencies)
{
    const auto& mergedDeps = mergeValueDependencies(dependencies);
    resolveValueDependencies(mergedDeps, initialDependencies);
    addOnValueDependencies(mergedDeps, initialDependencies);
    for (auto& item : m_valueDependencies) {
        //llvm::dbgs() << *item.first << "\n";
        reflect(item.first, item.second);
    }
    assert(m_valueDependentValues.empty());
    const auto& finalMerged = mergeSuccessorDependenciesWithInitialDependencies(initialDependencies, mergedDeps);
    assert(m_valueDependentValues.empty());
    assert(m_valueDependentInstrs.empty());
    assert(m_valueDependentOutArguments.empty());
    assert(m_valueDependentFunctionArguments.empty());
    m_isReflected = true;
}

void ReflectingBasicBlockAnaliser::setInitialValueDependencies(
                    const DependencyAnalysisResult::InitialValueDpendencies& valueDependencies)
{
    BasicBlockAnalysisResult::setInitialValueDependencies(valueDependencies);
    for (const auto& valDep : m_valueDependencies) {
        if (valDep.second.isValueDep()) {
            for (const auto& val : valDep.second.getValueDependencies()) {
                m_valueDependentValues[val].insert(valDep.first);
            }
        }
    }
}

DepInfo ReflectingBasicBlockAnaliser::getInstructionDependencies(llvm::Instruction* instr) const
{
    auto indeppos = m_inputIndependentInstrs.find(instr);
    if (indeppos != m_inputIndependentInstrs.end()) {
        return DepInfo(DepInfo::INPUT_INDEP);
    }
    auto valpos = m_instructionValueDependencies.find(instr);
    if (valpos != m_instructionValueDependencies.end()) {
        return valpos->second;
    }
    auto deppos = m_inputDependentInstrs.find(instr);
    assert(deppos != m_inputDependentInstrs.end());
    return DepInfo(DepInfo::INPUT_DEP, deppos->second);
}

void ReflectingBasicBlockAnaliser::processInstrForOutputArgs(llvm::Instruction* I)
{
    if (m_outArgDependencies.empty()) {
        return;
    }
    const auto& DL = I->getModule()->getDataLayout();
    auto item = m_outArgDependencies.begin();
    while (item != m_outArgDependencies.end()) {
        llvm::Value* val = llvm::dyn_cast<llvm::Value>(item->first);
        const auto& info = m_AAR.getModRefInfo(I, val, DL.getTypeStoreSize(val->getType()));
        if (info != llvm::ModRefInfo::MRI_Mod) {
            ++item;
            continue;
        }
        auto valueDepPos = m_instructionValueDependencies.find(I);
        if (valueDepPos == m_instructionValueDependencies.end()) {
            assert(valueDepPos->second.isValueDep());
            const auto& dependencies = valueDepPos->second;
            item->second.setDependency(valueDepPos->second.getDependency());
            item->second.mergeDependencies(valueDepPos->second.getValueDependencies());
            for (const auto& val : item->second.getValueDependencies()) {
                m_valueDependentOutArguments[val].insert(item->first);
            }
            continue;
        }
        auto depInstrPos = m_inputDependentInstrs.find(I);
        if (depInstrPos != m_inputDependentInstrs.end()) {
            const auto& dependencies = depInstrPos->second;
            item->second.setDependency(DepInfo::INPUT_DEP);
            item->second.mergeDependencies(dependencies);
        } else {
            // making output input independent
            item->second = DepInfo(DepInfo::INPUT_INDEP);
        }
        ++item;
    }
}

DepInfo ReflectingBasicBlockAnaliser::getInstructionDependencies(llvm::Instruction* instr)
{
    auto deppos = m_inputDependentInstrs.find(instr);
    if (deppos != m_inputDependentInstrs.end()) {
        return DepInfo(DepInfo::INPUT_DEP, deppos->second);
    }
    auto indeppos = m_inputIndependentInstrs.find(instr);
    if (indeppos != m_inputIndependentInstrs.end()) {
        return DepInfo(DepInfo::INPUT_INDEP);
    }
    auto valdeppos = m_instructionValueDependencies.find(instr);
    if (valdeppos != m_instructionValueDependencies.end()) {
        return valdeppos->second;
    }
    if (auto* loadInst = llvm::dyn_cast<llvm::LoadInst>(instr)) {
        return getLoadInstrDependencies(loadInst);
    }

    return determineInstructionDependenciesFromOperands(instr);

}

void ReflectingBasicBlockAnaliser::updateInstructionDependencies(llvm::Instruction* instr,
                                                                 const DepInfo& info)
{
    switch (info.getDependency()) {
    case DepInfo::INPUT_INDEP:
        assert(info.getArgumentDependencies().empty());
        assert(info.getValueDependencies().empty());
        m_inputIndependentInstrs.insert(instr);
        break;
    case DepInfo::INPUT_DEP:
        assert(info.getValueDependencies().empty());
        m_inputDependentInstrs[instr].insert(info.getArgumentDependencies().begin(), info.getArgumentDependencies().end());
        break;
    case DepInfo::VALUE_DEP:
        m_instructionValueDependencies[instr].mergeDependencies(info);
        updateValueDependentInstructions(info, instr);
        break;
    default:
        assert(false);
    }
}

void ReflectingBasicBlockAnaliser::updateValueDependencies(llvm::Value* value,
                                                           const DepInfo& info)
{
    auto valPos = m_valueDependencies.find(value);
    if (valPos != m_valueDependencies.end()) {
        if (valPos->second.isValueDep()) {
            for (auto& val : valPos->second.getValueDependencies()) {
                m_valueDependentValues[val].erase(value);
                if (m_valueDependentValues[val].empty()) {
                    m_valueDependentValues.erase(val);
                }
            }
        }
    }
    if (!info.isValueDep()) {
        m_valueDependencies[value].setDependency(info.getDependency());
        m_valueDependencies[value] = info;
        return;
    }
    DepInfo newInfo(info.getDependency(), info.getArgumentDependencies());
    for (const auto& val : info.getValueDependencies()) {
        auto pos = m_valueDependencies.find(val);
        if (pos != m_valueDependencies.end()) {
            newInfo.mergeDependencies(pos->second);
        } else {
            newInfo.getValueDependencies().insert(val);
        }
    }
    m_valueDependencies[value] = newInfo;
    for (const auto& val : newInfo.getValueDependencies()) {
        m_valueDependentValues[val].insert(value);
    }
}

// basic block analysis result could do this same way
void ReflectingBasicBlockAnaliser::updateReturnValueDependencies(const DepInfo& info)
{
    m_returnValueDependencies.mergeDependencies(info);
}

DepInfo ReflectingBasicBlockAnaliser::getLoadInstrDependencies(llvm::LoadInst* instr)
{
    auto* loadOp = instr->getPointerOperand();
    llvm::Value* loadedValue = getMemoryValue(loadOp);
    if (loadedValue == nullptr) {
        return getInstructionDependencies(llvm::dyn_cast<llvm::Instruction>(loadOp));
    }
    auto pos = m_valueDependencies.find(loadedValue);
    if (pos != m_valueDependencies.end()) {
        return pos->second;
    }
    return DepInfo(DepInfo::VALUE_DEP, ValueSet{loadedValue});

}

DepInfo ReflectingBasicBlockAnaliser::determineInstructionDependenciesFromOperands(llvm::Instruction* instr)
{
    DepInfo info(DepInfo::INPUT_INDEP);
    for (auto op = instr->op_begin(); op != instr->op_end(); ++op) {
        if (auto* opInst = llvm::dyn_cast<llvm::Instruction>(op)) {
            const auto& opDeps = getInstructionDependencies(opInst);
            info.mergeDependencies(opDeps);
        } else if (auto* opVal = llvm::dyn_cast<llvm::Value>(op)) {
            if (auto* constOp = llvm::dyn_cast<llvm::Constant>(opVal)) {
                info.mergeDependencies(DepInfo(DepInfo::INPUT_INDEP));
            } else { 
                const auto& valDeps = getValueDependencies(opVal);
                info.mergeDependencies(valDeps);
            }
        }
    }
    return info;
}

void ReflectingBasicBlockAnaliser::updateFunctionCallInfo(llvm::CallInst* callInst, bool isExternalF)
{
    auto F = callInst->getCalledFunction();
    assert(F != nullptr);
    BasicBlockAnalysisResult::updateFunctionCallInfo(callInst, isExternalF);
    auto pos = m_calledFunctionsInfo.find(F);
    if (pos == m_calledFunctionsInfo.end()) {
        return;
    }
    for (const auto& item : pos->second) {
        if (!item.second.isValueDep()) {
            continue;
        }
        for (const auto& val : item.second.getValueDependencies()) {
            m_valueDependentFunctionArguments[val][F].insert(item.first);
        }
    }
}

void ReflectingBasicBlockAnaliser::updateValueDependentInstructions(const DepInfo& info,
                                                                    llvm::Instruction* instr)
{
    for (const auto& val : info.getValueDependencies()) {
        m_valueDependentInstrs[val].insert(instr);
    }
}

void ReflectingBasicBlockAnaliser::reflect(llvm::Value* value, const DepInfo& deps)
{
    assert(deps.isInputDep() || deps.isInputIndep());
    reflectOnValues(value, deps);
    reflectOnInstructions(value, deps); // need to go trough instructions one more time and add to correspoinding set
    reflectOnOutArguments(value, deps);
    reflectOnCalledFunctionArguments(value, deps);
    reflectOnReturnValue(value, deps);
}

void ReflectingBasicBlockAnaliser::reflectOnValues(llvm::Value* value, const DepInfo& depInfo)
{
    auto valDepPos = m_valueDependentValues.find(value);
    if (valDepPos == m_valueDependentValues.end()) {
        return;
    }
    for (const auto& val : valDepPos->second) {
        auto pos = m_valueDependencies.find(val);
        assert(pos != m_valueDependencies.end());
        // TODO: is this possible after resolving values?
        if (pos->second.isValueDep()) {
            reflectOnDepInfo(value, pos->second, depInfo);
        }
    }
    m_valueDependentValues.erase(valDepPos);
}

void ReflectingBasicBlockAnaliser::reflectOnInstructions(llvm::Value* value, const DepInfo& depInfo)
{
    auto instrDepPos = m_valueDependentInstrs.find(value);
    if (instrDepPos == m_valueDependentInstrs.end()) {
        return;
    }
    for (const auto& instr : instrDepPos->second) {
        auto instrPos = m_instructionValueDependencies.find(instr);
        assert(instrPos != m_instructionValueDependencies.end());
        reflectOnDepInfo(value, instrPos->second, depInfo);
        if (instrPos->second.isInputDep()) {
            m_inputDependentInstrs[instr].insert(instrPos->second.getArgumentDependencies().begin(),
                                                 instrPos->second.getArgumentDependencies().end());
            m_instructionValueDependencies.erase(instrPos);
        } else if (instrPos->second.isInputIndep()) {
            m_inputIndependentInstrs.insert(instr);
            m_instructionValueDependencies.erase(instrPos);
        }
    }
    m_valueDependentInstrs.erase(instrDepPos);
}

void ReflectingBasicBlockAnaliser::reflectOnOutArguments(llvm::Value* value, const DepInfo& depInfo)
{
    auto outArgPos = m_valueDependentOutArguments.find(value);
    if (outArgPos == m_valueDependentOutArguments.end()) {
        return;
    }
    for (const auto& outArg : outArgPos->second) {
        auto argPos = m_outArgDependencies.find(outArg);
        assert(argPos != m_outArgDependencies.end());
        reflectOnDepInfo(value, argPos->second, depInfo);
    }
    m_valueDependentOutArguments.erase(outArgPos);
}

void ReflectingBasicBlockAnaliser::reflectOnCalledFunctionArguments(llvm::Value* value, const DepInfo& depInfo)
{
    auto valPos = m_valueDependentFunctionArguments.find(value);
    if (valPos == m_valueDependentFunctionArguments.end()) {
        return;
    }
    for (const auto& fargs : valPos->second) {
        auto F = fargs.first;
        auto Fpos = m_calledFunctionsInfo.find(F);
        assert(Fpos != m_calledFunctionsInfo.end());
        for (auto& arg : fargs.second) {
            auto argPos = Fpos->second.find(arg);
            assert(argPos != Fpos->second.end());
            reflectOnDepInfo(value, argPos->second, depInfo);
        }
    }
    m_valueDependentFunctionArguments.erase(valPos);
}

void ReflectingBasicBlockAnaliser::reflectOnReturnValue(llvm::Value* value, const DepInfo& depInfo)
{
    if (!m_returnValueDependencies.isValueDep()) {
        return;
    }
    auto pos = m_returnValueDependencies.getValueDependencies().find(value);
    if (pos == m_returnValueDependencies.getValueDependencies().end()) {
        return;
    }
    reflectOnDepInfo(value, m_returnValueDependencies, depInfo);
}

bool ReflectingBasicBlockAnaliser::reflectOnSingleValue(llvm::Value* value,
                                                        const DependencyAnaliser::ValueDependencies& reflectFrom)
{
    auto valPos = m_valueDependencies.find(value);
    assert(valPos != m_valueDependencies.end());
    assert(valPos->second.isValueDep());

    auto& valDeps = valPos->second.getValueDependencies(); 
    auto valDep = valDeps.begin();
    while (valDep != valDeps.end()) {
        auto pos = reflectFrom.find(*valDep);
        if (pos == reflectFrom.end() || pos->second.isValueDep()) {
            ++valDep;
            continue;
        }
        reflectOnDepInfo(*valDep, valPos->second, pos->second, false);
        auto valDepPos = m_valueDependentValues[*valDep].find(value);
        assert(valDepPos != m_valueDependentValues[*valDep].end());
        m_valueDependentValues[*valDep].erase(valDepPos);
        if (m_valueDependentValues[*valDep].empty()) {
            m_valueDependentValues.erase(*valDep);
        }

        auto oldDep = valDep;
        ++valDep;
        valDeps.erase(oldDep);
    }
    if (valDeps.empty()) {
        valPos->second.appyIntermediateDependency();
    }
    return !valPos->second.isValueDep();
}

void ReflectingBasicBlockAnaliser::reflectOnDepInfo(llvm::Value* value,
                                                    DepInfo& depInfoTo,
                                                    const DepInfo& depInfoFrom,
                                                    bool eraseAfterReflection)
{
    // note: this won't change pos dependency, if it is of maximum value valuedep
    depInfoTo.mergeDependencies(depInfoFrom);
    depInfoTo.collectIntermediateDependency(depInfoFrom.getDependency());
    if (!eraseAfterReflection) {
        return;
    }
    assert(depInfoTo.isValueDep());
    auto& valueDeps = depInfoTo.getValueDependencies();
    auto valPos = valueDeps.find(value);
    assert(valPos != valueDeps.end());
    valueDeps.erase(valPos);
    if (valueDeps.empty()) {
        depInfoTo.appyIntermediateDependency();
    }

}

void ReflectingBasicBlockAnaliser::resolveValueDependencies(const DependencyAnaliser::ValueDependencies& initialDependencies)
{
    for (auto& item : m_valueDependencies) {
        if (item.second.isValueDep()) {
            //llvm::dbgs() << "resolve dependencies for value " << *item.first << "\n";
            if (!reflectOnSingleValue(item.first, m_valueDependencies)) {
                //llvm::dbgs() << "Not resolved with self value dependencies. Remaining dependencies are: \n";
                bool refres = reflectOnSingleValue(item.first, initialDependencies);
                assert(refres);
            }
        }
    }
}

void ReflectingBasicBlockAnaliser::resolveValueDependencies(const DependencyAnaliser::ValueDependencies& successorDependencies,
                                                            const DependencyAnaliser::ValueDependencies& initialDependencies)
{
    for (auto& item : m_valueDependencies) {
        if (!item.second.isValueDep()) {
            //llvm::dbgs() << "Value " << *item.first << "\n";
            auto inSuccPos = successorDependencies.find(item.first);
            if (inSuccPos != successorDependencies.end()) {
                assert(!inSuccPos->second.isValueDep());
                item.second.mergeDependencies(inSuccPos->second);
            }
            continue;
        }
        //llvm::dbgs() << "Value dep value " << *item.first << "\n";
        if (!reflectOnSingleValue(item.first, successorDependencies)) {
            bool refres = reflectOnSingleValue(item.first, initialDependencies);
            assert(refres);
        }
    }
}

void ReflectingBasicBlockAnaliser::addOnValueDependencies(const DependencyAnaliser::ValueDependencies& initialDependencies)
{
    for (const auto& dep : initialDependencies) {
        //llvm::dbgs() << "adding from initial values " << *dep.first << " " << dep.second.dependency << "\n"; 
        if (dep.second.isDefined()) {
            // won't insert if already has
            auto res = m_valueDependencies.insert(dep);
            //if (!res.second) {
            //    llvm::dbgs() << "could not add value " << *dep.first << "\n";
            //    llvm::dbgs() << "exists with dependency " << res.first->second.dependency << "\n";
            //    for (auto& d : res.first->second.valueDependencies) {
            //        llvm::dbgs() << "   " << *d << "\n";
            //    }
            //}
        }
    }
}

void ReflectingBasicBlockAnaliser::addOnValueDependencies(const DependencyAnaliser::ValueDependencies& successorDependencies,
                                                          const DependencyAnaliser::ValueDependencies& initialDependencies)
{
    for (const auto& dep : successorDependencies) {
        //llvm::dbgs() << "adding from successor values " << *dep.first << " " << dep.second.dependency << "\n"; 
        if (dep.second.isDefined()) {
            m_valueDependencies.insert(dep);
        }
    }
    for (const auto& dep : initialDependencies) {
        //llvm::dbgs() << "adding from initial values " << *dep.first << " " << dep.second.dependency << "\n"; 
        if (dep.second.isDefined()) {
            m_valueDependencies.insert(dep);
        }
    }
}

} // namespace input_dependency

