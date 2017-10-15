#include "DependencyAnaliser.h"

#include "InputDepInstructionsRecorder.h"
#include "FunctionAnaliser.h"
#include "LibFunctionInfo.h"
#include "LibraryInfoManager.h"
#include "IndirectCallSitesAnalysis.h"
#include "Utils.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"

namespace input_dependency {

namespace {

llvm::Argument* getFunctionArgument(llvm::Function* F, unsigned index)
{
    auto it = F->getArgumentList().begin();
    while (index-- != 0) {
        ++it;
    }
    if (it == F->getArgumentList().end()) {
        return nullptr;
    }
    return &*it;
}

DepInfo getFinalizedDepInfo(const ValueSet& values,
                            const DependencyAnaliser::GlobalVariableDependencyMap& globalDeps)
{
    DepInfo newInfo(DepInfo::INPUT_INDEP);
    for (auto& item : values) {
        auto global = llvm::dyn_cast<llvm::GlobalVariable>(item);
        if (global == nullptr) {
            //llvm::dbgs() << *item << "\n";
            continue;
        }
        assert(global != nullptr);
        auto pos = globalDeps.find(global);
        if (pos == globalDeps.end()) {
            continue;
        }
        assert(pos != globalDeps.end());
        assert(pos->second.isDefined());
        assert(!pos->second.getDependency() != DepInfo::VALUE_DEP);
        newInfo.mergeDependencies(pos->second.getArgumentDependencies());
        newInfo.mergeDependency(pos->second.getDependency());
    }
    return newInfo;
}

llvm::Function* getAliasingFunction(llvm::Value* calledValue)
{
    if (auto* alias = llvm::dyn_cast<llvm::GlobalAlias>(calledValue)) {
        auto module = alias->getParent();
        return module->getFunction(alias->getAliasee()->getName());
    }
    return nullptr;
}


} // unnamed namespace


DependencyAnaliser::DependencyAnaliser(llvm::Function* F,
                                       llvm::AAResults& AAR,
                                       const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                       const IndirectCallSitesAnalysisResult& indirectCallsInfo,
                                       const Arguments& inputs,
                                       const FunctionAnalysisGetter& Fgetter)
                                : m_F(F)
                                , m_AAR(AAR)
                                , m_virtualCallsInfo(virtualCallsInfo)
                                , m_indirectCallsInfo(indirectCallsInfo)
                                , m_inputs(inputs)
                                , m_FAG(Fgetter)
                                , m_finalized(false)
                                , m_globalsFinalized(false)
{
}

void DependencyAnaliser::finalize(const ArgumentDependenciesMap& dependentArgs)
{
    m_finalInputDependentInstrs.clear();
    for (auto& item : m_inputDependentInstrs) {
        if (item.second.isInputDep()) {
            m_finalInputDependentInstrs.insert(item.first);
            if (m_inputIndependentInstrs.find(item.first) != m_inputIndependentInstrs.end()) {
                m_inputIndependentInstrs.erase(item.first);
            }
        } else if (item.second.isInputArgumentDep()
                   && Utils::haveIntersection(dependentArgs, item.second.getArgumentDependencies())) {
            m_finalInputDependentInstrs.insert(item.first);
            if (m_inputIndependentInstrs.find(item.first) != m_inputIndependentInstrs.end()) {
                m_inputIndependentInstrs.erase(item.first);
            }
        } else {
            m_inputIndependentInstrs.insert(item.first);
        }
    }
    for (auto& callInfo : m_functionCallInfo) {
        callInfo.second.finalizeArgumentDependencies(dependentArgs);
    }
    m_finalized = true;
}

void DependencyAnaliser::finalize(const GlobalVariableDependencyMap& globalDeps)
{
    assert(!m_globalsFinalized);
    finalizeValues(globalDeps);
    finalizeInstructions(globalDeps);
    for (auto& callInfo : m_functionCallInfo) {
        callInfo.second.finalizeGlobalsDependencies(globalDeps);
    }
    m_globalsFinalized = true;
}

void DependencyAnaliser::dump() const
{
    llvm::dbgs() << "Input independent instructions --------\n";
    for (auto& item : m_inputIndependentInstrs) {
        llvm::dbgs() << *item << "\n";
    }
    llvm::dbgs() << "Finalized input dependent instructions\n";
    for (auto& item : m_finalInputDependentInstrs) {
        llvm::dbgs() << *item << "\n";
    }
    llvm::dbgs() << "\nNot final input dependent instructions\n";
    for (auto& item : m_inputDependentInstrs) {
        llvm::dbgs() << *item.first << " depends on ---------- ";
        if (item.second.isInputDep()) {
            llvm::dbgs() << " new input, ";
        }
        for (auto& arg : item.second.getArgumentDependencies()) {
            llvm::dbgs() << arg->getArgNo() << " ";
        } 
        llvm::dbgs() << "\n";
        for (auto& val : item.second.getValueDependencies()) {
            llvm::dbgs() << "   " << *val << "\n";
        }
        llvm::dbgs() << "\n";
    }

    llvm::dbgs() << "\nOutput parameters dependencies\n";
    for (const auto& item : m_outArgDependencies) {
        llvm::dbgs() << *item.first;
        if (item.second.isInputIndep()) {
            llvm::dbgs() << " became input independent\n";
            continue;
        } else if (item.second.getArgumentDependencies().empty()) {
            llvm::dbgs() << " became dependent on new input\n";
            continue;
        }
        llvm::dbgs() << " depends on ---------- ";
        for (const auto& arg : item.second.getArgumentDependencies()) {
            llvm::dbgs() << arg->getArgNo() << " ";
        }
        llvm::dbgs() << "\n";
        for (const auto& val : item.second.getValueDependencies()) {
            llvm::dbgs() << "   " << *val << "\n";
        }
        llvm::dbgs() << "\n";
    }

    llvm::dbgs() << "\nReturn Value dependency\n";
    if (m_returnValueDependencies.isInputIndep()) {
        llvm::dbgs() << " is input independent\n";
    } else if (m_returnValueDependencies.isInputDep()) {
        llvm::dbgs() << " is dependent on new input\n";
    } else {
        for (const auto& item : m_returnValueDependencies.getArgumentDependencies()) {
            llvm::dbgs() << *item << " ";
        }
        llvm::dbgs() << "\n";
        for (const auto& val : m_returnValueDependencies.getValueDependencies()) {
            llvm::dbgs() << "   " << *val << "\n";
        }
    }
    llvm::dbgs() << "\n";
}

void DependencyAnaliser::processInstruction(llvm::Instruction* inst)
{
    updateInstructionDependencies(inst, getInstructionDependencies(inst));
}

void DependencyAnaliser::processPhiNode(llvm::PHINode* phi)
{
    DepInfo info;
    for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
        llvm::Value* val = phi->getIncomingValue(i);
        if (val == nullptr) {
            continue;
        }
        if (llvm::dyn_cast<llvm::Constant>(val)) {
            info.mergeDependencies(DepInfo(DepInfo::INPUT_INDEP));
            continue;
        }
        const auto& valDeps = getValueDependencies(val);
        if (valDeps.isDefined()) {
            info.mergeDependencies(valDeps.getValueDep());
        } else {
            auto selfF = phi->getParent()->getParent();
            assert(selfF != nullptr);
            auto selfFunctionResults = m_FAG(selfF);
            assert(selfFunctionResults);
            const auto& depInfofromBlock = selfFunctionResults->getDependencyInfoFromBlock(val,
                                                    phi->getIncomingBlock(i)).getValueDep();
            if (!depInfofromBlock.isDefined()) {
                continue;
            }
            info.mergeDependencies(depInfofromBlock);
        }
        if (info.isInputDep()) {
            break;
        }
    }
    if (!info.isDefined()) {
        info.mergeDependencies(DepInfo(DepInfo::INPUT_DEP));
    }
    updateInstructionDependencies(phi, info);
}

void DependencyAnaliser::processBitCast(llvm::BitCastInst* bitcast)
{
    auto castedValue = bitcast->getOperand(0);
    assert(castedValue != nullptr);
    DepInfo depInfo;
    auto args = isInput(castedValue);
    if (!args.empty()) {
        depInfo = DepInfo(DepInfo::INPUT_ARGDEP, args);
    }
    const auto& valueDeps = getValueDependencies(castedValue);
    depInfo.mergeDependencies(valueDeps.getValueDep());
    if (!depInfo.isDefined()) {
        if (auto instr = llvm::dyn_cast<llvm::Instruction>(castedValue)) {
            const auto& instrDeps = getInstructionDependencies(instr);
            depInfo.mergeDependencies(instrDeps);
        }
    }

    assert(depInfo.isDefined());
    m_valueDependencies[bitcast] = ValueDepInfo(depInfo);
    updateInstructionDependencies(bitcast, depInfo);
}

void DependencyAnaliser::processGetElementPtrInst(llvm::GetElementPtrInst* getElPtr)
{
    // example of getElementPtrInstr
    // p.x where x is the second field of struct p
    // %x = getelementptr inbounds %struct.point, %struct.point* %p, i32 0, i32 1
    // for int *p; p[0]
    // %arrayidx = getelementptr inbounds i32, i32* %0, i64 0, where %0 is load of p
    auto compositeValue = getElPtr->getOperand(0);
    auto depInfo = getCompositeValueDependencies(compositeValue, getElPtr);
    if (!depInfo.isDefined()) {
        auto memory_value = getMemoryValue(compositeValue);
        if (memory_value) {
            depInfo = getCompositeValueDependencies(memory_value, getElPtr);
        }
    }
    if (!depInfo.isDefined()) {
        if (auto instr = llvm::dyn_cast<llvm::Instruction>(compositeValue)) {
            depInfo = ValueDepInfo(compositeValue, getInstructionDependencies(instr));
        } else {
            depInfo = ValueDepInfo(compositeValue, DepInfo(DepInfo::INPUT_DEP));
        }
    }

    updateInstructionDependencies(getElPtr, depInfo.getValueDep());
    updateValueDependencies(getElPtr, depInfo); // add getElPtr as value
}

void DependencyAnaliser::processReturnInstr(llvm::ReturnInst* retInst)
{
    auto retValue = retInst->getReturnValue();
    if (!retValue) {
        updateInstructionDependencies(retInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }
    if (llvm::dyn_cast<llvm::Constant>(retValue)) {
        updateInstructionDependencies(retInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }
    ValueDepInfo retDepInfo = getValueDependencies(retValue);
    if (!retDepInfo.isDefined()) {
        if (auto* retValInst = llvm::dyn_cast<llvm::Instruction>(retValue)) {
            retDepInfo = ValueDepInfo(retValue, getInstructionDependencies(retValInst));
        }
    }
    if (!retDepInfo.isDefined()) {
        retDepInfo = ValueDepInfo(retValue, DepInfo(DepInfo::INPUT_INDEP));
    }
    updateInstructionDependencies(retInst, retDepInfo.getValueDep());
    updateReturnValueDependencies(retDepInfo);
}

void DependencyAnaliser::processBranchInst(llvm::BranchInst* branchInst)
{
    if (branchInst->isUnconditional()) {
        updateInstructionDependencies(branchInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }

    auto condition = branchInst->getCondition();
    if (llvm::dyn_cast<llvm::Constant>(condition)) {
        updateInstructionDependencies(branchInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }
    DepInfo dependencies;
    if (auto* condInstr = llvm::dyn_cast<llvm::Instruction>(condition)) {
        dependencies = getInstructionDependencies(condInstr);
    } else {
        // Note: it is important to have this check after instruction as Instruction inherits from Value
        if (auto* condVal = llvm::dyn_cast<llvm::Value>(condition)) {
            dependencies = getValueDependencies(condVal).getValueDep();
            assert(dependencies.isDefined());
        }
    }
    updateInstructionDependencies(branchInst, dependencies);
}

void DependencyAnaliser::processStoreInst(llvm::StoreInst* storeInst)
{
    auto op = storeInst->getOperand(0);
    ValueDepInfo info; // the value here is not important, could be null
    if (llvm::dyn_cast<llvm::Constant>(op)) {
        info.updateCompositeValueDep(DepInfo(DepInfo::INPUT_INDEP));
    } else {
        info.mergeDependencies(getValueDependencies(op));
        if (!info.isDefined()) {
            if (auto* opInstr = llvm::dyn_cast<llvm::Instruction>(op)) {
                // will always take this branch?
                info.mergeDependencies(getInstructionDependencies(opInstr));
            } else {
                auto args = isInput(op);
                if (!args.empty()) {
                    info.updateCompositeValueDep(DepInfo(DepInfo::INPUT_ARGDEP, args));
                }
            }
        }
    }
    if (!info.isDefined()) {
        info.updateCompositeValueDep(DepInfo(DepInfo::INPUT_DEP));
        InputDepInstructionsRecorder::get().record(storeInst);
    }
    assert(info.isDefined());
    auto storeTo = storeInst->getPointerOperand();
    if (auto global = llvm::dyn_cast<llvm::GlobalVariable>(storeTo)) {
        m_modifiedGlobals.insert(global);
    }
    updateInstructionDependencies(storeInst, info.getValueDep());
    // Whatever storeTo is (value or instruction) is going to be collected in value list. 
    if (auto* getElPtr = llvm::dyn_cast<llvm::GetElementPtrInst>(storeTo)) {
        updateDependencyForGetElementPtr(getElPtr, info);
    } else {
        updateValueDependencies(storeTo, info);
        updateModAliasesDependencies(storeInst, info);
    }
}

void DependencyAnaliser::processCallInst(llvm::CallInst* callInst)
{
    llvm::Function* F = callInst->getCalledFunction();
    if (F == nullptr) {
        // This could happen for example when calling virtual functions
        F = getAliasingFunction(callInst->getCalledValue());
        if (F == nullptr) {
            if (m_virtualCallsInfo.hasVirtualCallCandidates(callInst)) {
                processCallSiteWithMultipleTargets(callInst, m_virtualCallsInfo.getVirtualCallCandidates(callInst));
            } else if (m_indirectCallsInfo.hasIndirectCallTargets(callInst)) {
                processCallSiteWithMultipleTargets(callInst, m_indirectCallsInfo.getIndirectCallTargets(callInst));
            } else {
                // make all out args input dependent
                updateCallInputDependentOutArgDependencies(callInst);
                // make return value input dependent
                updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_DEP));
                InputDepInstructionsRecorder::get().record(callInst);
                // make all globals input dependent?
            }
        }
        return;
    }
    if (F->isIntrinsic()) {
        updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_INDEP));
    } else if (Utils::isLibraryFunction(F, m_F->getParent())) {
        const ArgumentDependenciesMap& argDepMap = gatherFunctionCallSiteInfo(callInst, F);
        updateLibFunctionCallInstOutArgDependencies(callInst, argDepMap);
        updateLibFunctionCallInstructionDependencies(callInst, argDepMap);
    } else {
        updateFunctionCallSiteInfo(callInst, F);
        // cyclic call
        // analysis result of callee is not available. e.g cyclic calls, recursive calls
        if (m_FAG(F) == nullptr) {
            updateCallInputDependentOutArgDependencies(callInst);
            updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_DEP));
        } else {
            updateCallSiteOutArgDependencies(callInst, F);
            updateCallInstructionDependencies(callInst, F);
            updateGlobalsAfterFunctionCall(callInst, F);
        }
    }
}

void DependencyAnaliser::processInvokeInst(llvm::InvokeInst* invokeInst)
{
    llvm::Function* F = invokeInst->getCalledFunction();
    if (F == nullptr) {
        // This could happen for example when calling virtual functions
        // try see if has alias
        F = getAliasingFunction(invokeInst->getCalledValue());
        if (F == nullptr) {
            if (m_virtualCallsInfo.hasVirtualInvokeCandidates(invokeInst)) {
                processInvokeSiteWithMultipleTargets(invokeInst, m_virtualCallsInfo.getVirtualInvokeCandidates(invokeInst));
            } else if (m_indirectCallsInfo.hasIndirectInvokeTargets(invokeInst)) {
                processInvokeSiteWithMultipleTargets(invokeInst, m_indirectCallsInfo.getIndirectInvokeTargets(invokeInst));
            } else {
                // make all out args input dependent
                updateInvokeInputDependentOutArgDependencies(invokeInst);
                // make return value input dependent
                updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_DEP));
                InputDepInstructionsRecorder::get().record(invokeInst);
                // make all globals input dependent?
            }
        }
        return;
    }
    if (F->isIntrinsic()) {
        updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_INDEP));
    } else if (Utils::isLibraryFunction(F, m_F->getParent())) {
        const ArgumentDependenciesMap& argDepMap = gatherFunctionInvokeSiteInfo(invokeInst, F);
        updateLibFunctionInvokeInstOutArgDependencies(invokeInst, argDepMap);
        updateLibFunctionInvokeInstructionDependencies(invokeInst, argDepMap);
    } else {
        updateFunctionInvokeSiteInfo(invokeInst, F);
        // cyclic call
        if (m_FAG(F) == nullptr) {
            updateInvokeInputDependentOutArgDependencies(invokeInst);
            updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_DEP));
        } else {
            updateInvokeSiteOutArgDependencies(invokeInst, F);
            updateInvokeInstructionDependencies(invokeInst, F);
            updateGlobalsAfterFunctionInvoke(invokeInst, F);
        }
    }
}

void DependencyAnaliser::processCallSiteWithMultipleTargets(llvm::CallInst* callInst, const FunctionSet& targets)
{
    for (auto F : targets) {
        updateFunctionCallSiteInfo(callInst, F);
        if (m_FAG(F) == nullptr) {
            //llvm::dbgs() << "Analysis results not available for indirect call target: " << F->getName() << "\n";
            updateCallInputDependentOutArgDependencies(callInst);
            updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_DEP));
            InputDepInstructionsRecorder::get().record(callInst);
            // update globals??? May result to inaccuracies 
        } else {
            //llvm::dbgs() << "Analysis results available for indirect call target: " << F->getName() << "\n";
            updateCallSiteOutArgDependencies(callInst, F);
            updateCallInstructionDependencies(callInst, F);
            updateGlobalsAfterFunctionCall(callInst, F);
        }
    }
}

void DependencyAnaliser::processInvokeSiteWithMultipleTargets(llvm::InvokeInst* invokeInst, const FunctionSet& targets)
{
    for (auto F : targets) {
        updateFunctionInvokeSiteInfo(invokeInst, F);
        if (m_FAG(F) == nullptr) {
            updateInvokeInputDependentOutArgDependencies(invokeInst);
            updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_DEP));
            InputDepInstructionsRecorder::get().record(invokeInst);
            // update globals??? May result to inaccuracies 
        } else {
            updateInvokeSiteOutArgDependencies(invokeInst, F);
            updateInvokeInstructionDependencies(invokeInst, F);
            updateGlobalsAfterFunctionInvoke(invokeInst, F);
        }
    }
}

void DependencyAnaliser::updateFunctionCallSiteInfo(llvm::CallInst* callInst, llvm::Function* F)
{
    auto pos = m_functionCallInfo.insert(std::make_pair(F, FunctionCallDepInfo(*F)));
    m_calledFunctions.insert(F);

    const auto& argDepMap = gatherFunctionCallSiteInfo(callInst, F);
    const auto& globalsDepMap = gatherGlobalsForFunctionCall(F);
    pos.first->second.addCall(callInst, argDepMap);
    pos.first->second.addCall(callInst, globalsDepMap);
}

void DependencyAnaliser::updateFunctionInvokeSiteInfo(llvm::InvokeInst* invokeInst, llvm::Function* F)
{
    auto pos = m_functionCallInfo.insert(std::make_pair(F, FunctionCallDepInfo(*F)));
    m_calledFunctions.insert(F);

    const auto& argDepMap = gatherFunctionInvokeSiteInfo(invokeInst, F);
    const auto& globalsDepMap = gatherGlobalsForFunctionCall(F);
    pos.first->second.addInvoke(invokeInst, argDepMap);
    pos.first->second.addInvoke(invokeInst, globalsDepMap);
}

ArgumentSet DependencyAnaliser::isInput(llvm::Value* val) const
{
    for (auto& arg : m_inputs) {
        if (auto* argVal = llvm::dyn_cast<llvm::Value>(arg)) {
            if (argVal == val) {
                return ArgumentSet{arg};
            }
        }
    }
    ArgumentSet set;
    for (auto& arg : m_inputs) {
        if (auto* argVal = llvm::dyn_cast<llvm::Value>(arg)) {
            auto aliasResult = m_AAR.alias(argVal, val);
            if (aliasResult != llvm::AliasResult::NoAlias) {
                set.insert(arg);
            }
        }
    }
    return set;
}

void DependencyAnaliser::updateCallSiteOutArgDependencies(llvm::CallInst* callInst, llvm::Function* F)
{
    auto pos = m_functionCallInfo.find(F);
    assert(pos != m_functionCallInfo.end());
    const auto& callArgDeps = pos->second.getArgumentDependenciesForCall(callInst);

    const auto& argumentValueGetter = [&callInst] (unsigned formalArgNo) -> llvm::Value* {
                                            if (formalArgNo >= callInst->getNumArgOperands()) {
                                                return nullptr;
                                            }
                                            return callInst->getArgOperand(formalArgNo);
                                        };
    updateCallOutArgDependencies(F, callArgDeps, argumentValueGetter);
}

void DependencyAnaliser::updateInvokeSiteOutArgDependencies(llvm::InvokeInst* invokeInst, llvm::Function* F)
{
    auto pos = m_functionCallInfo.find(F);
    assert(pos != m_functionCallInfo.end());
    const auto& invokeArgDeps = pos->second.getArgumentDependenciesForInvoke(invokeInst);

    const auto& argumentValueGetter = [&invokeInst] (unsigned formalArgNo) -> llvm::Value* {
                                            if (formalArgNo >= invokeInst->getNumArgOperands()) {
                                                return nullptr;
                                            }
                                            return invokeInst->getArgOperand(formalArgNo);
                                        };
    updateCallOutArgDependencies(F, invokeArgDeps, argumentValueGetter);
}

void DependencyAnaliser::updateCallInstructionDependencies(llvm::CallInst* callInst,
                                                           llvm::Function* F)
{
    if (F->doesNotReturn()) {
        updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }
    const FunctionAnaliser* FA = m_FAG(F);
    assert(FA != nullptr);
    if (FA->isReturnValueInputIndependent()) {
        updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_INDEP));
        updateValueDependencies(callInst, ValueDepInfo(DepInfo(DepInfo::INPUT_INDEP)));
        return;
    }
    auto retDeps = FA->getRetValueDependencies();
    if (!retDeps.isDefined()) {
        // Constructors are going with this branch
        updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }
    auto pos = m_functionCallInfo.find(F);
    assert(pos != m_functionCallInfo.end());
    resolveReturnedValueDependencies(retDeps, pos->second.getArgumentDependenciesForCall(callInst));
    updateInstructionDependencies(callInst, retDeps.getValueDep());
    updateValueDependencies(callInst, retDeps);
}

void DependencyAnaliser::updateInvokeInstructionDependencies(llvm::InvokeInst* invokeInst, llvm::Function* F)
{
    if (F->doesNotReturn()) {
        updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }
    const FunctionAnaliser* FA = m_FAG(F);
    assert(FA != nullptr);
    if (FA->isReturnValueInputIndependent()) {
        updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_INDEP));
        updateValueDependencies(invokeInst, ValueDepInfo(DepInfo(DepInfo::INPUT_INDEP)));
        return;
    }
    auto retDeps = FA->getRetValueDependencies();
    if (!retDeps.isDefined()) {
        // Constructors are going with this branch
        updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }
    auto pos = m_functionCallInfo.find(F);
    assert(pos != m_functionCallInfo.end());
    resolveReturnedValueDependencies(retDeps, pos->second.getArgumentDependenciesForInvoke(invokeInst));
    updateInstructionDependencies(invokeInst, retDeps.getValueDep());
    updateValueDependencies(invokeInst, retDeps);
}

void DependencyAnaliser::updateGlobalsAfterFunctionCall(llvm::CallInst* callInst, llvm::Function* F)
{
    assert(F != nullptr);
    auto pos = m_functionCallInfo.find(F);
    assert(pos != m_functionCallInfo.end());
    bool is_recurs = (F == callInst->getParent()->getParent());
    updateGlobalsAfterFunctionExecution(F, pos->second.getArgumentDependenciesForCall(callInst), is_recurs);
}

void DependencyAnaliser::updateGlobalsAfterFunctionInvoke(llvm::InvokeInst* invokeInst, llvm::Function* F)
{
    assert(F != nullptr);
    auto pos = m_functionCallInfo.find(F);
    assert(pos != m_functionCallInfo.end());
    bool is_recurs = (F == invokeInst->getParent()->getParent());
    updateGlobalsAfterFunctionExecution(F, pos->second.getArgumentDependenciesForInvoke(invokeInst), is_recurs);
}

void DependencyAnaliser::updateGlobalsAfterFunctionExecution(llvm::Function* F,
                                                             const ArgumentDependenciesMap& functionArgDeps,
                                                             bool is_recurs)
{
    const FunctionAnaliser* FA = m_FAG(F);
    assert(FA);

    const auto& refGlobals = FA->getReferencedGlobals();
    m_referencedGlobals.insert(refGlobals.begin(), refGlobals.end());

    const auto& modGlobals = FA->getModifiedGlobals();
    m_modifiedGlobals.insert(modGlobals.begin(), modGlobals.end());

    for (const auto& global : modGlobals) {
        ValueDepInfo depInfo(global);
        if (is_recurs) {
            depInfo = getValueDependencies(global);
        } else {
            assert(FA->hasGlobalVariableDepInfo(global));
            depInfo = FA->getGlobalVariableDependencies(global);
        }
        llvm::Value* val = llvm::dyn_cast<llvm::Value>(global);
        assert(val != nullptr);
        if (!depInfo.isDefined()) {
            continue;
        }
        if (!depInfo.isInputArgumentDep() && !depInfo.isValueDep()) {
            updateValueDependencies(val, depInfo);
            continue;
        }
        resolveReturnedValueDependencies(depInfo, functionArgDeps);
        updateValueDependencies(val, depInfo);
    }
}

void DependencyAnaliser::updateCallInputDependentOutArgDependencies(llvm::CallInst* callInst)
{
    auto FType = callInst->getFunctionType();
    auto argGetterByIndex = [&callInst] (unsigned index) -> llvm::Value* { return callInst->getArgOperand(index); };
    updateFunctionInputDepOutArgDependencies(FType, argGetterByIndex);
}

void DependencyAnaliser::updateInvokeInputDependentOutArgDependencies(llvm::InvokeInst* invokeInst)
{
    auto FType = invokeInst->getFunctionType();
    auto argGetterByIndex = [&invokeInst] (unsigned index) -> llvm::Value* { return invokeInst->getArgOperand(index); };
    updateFunctionInputDepOutArgDependencies(FType, argGetterByIndex);
}

void DependencyAnaliser::updateFunctionInputDepOutArgDependencies(llvm::FunctionType* FType,
                                                                  const ArgumentValueGetterByIndex& actualArgumentGetter)
{
    for (unsigned i = 0; i < FType->getNumParams(); ++i) {
        if (!FType->getParamType(i)->isPointerTy()) {
            continue;
        }
        auto argVal = actualArgumentGetter(i);
        llvm::Value* val = getFunctionOutArgumentValue(argVal);
        if (val == nullptr) {
            continue;
        }
        updateValueDependencies(val, ValueDepInfo(val, DepInfo(DepInfo::INPUT_DEP)));
    }
}

void DependencyAnaliser::updateLibFunctionCallInstOutArgDependencies(llvm::CallInst* callInst,
                                                                     const DependencyAnaliser::ArgumentDependenciesMap& argDepMap)
{
    auto F = callInst->getCalledFunction();
    const auto& argumentValueGetter = [&callInst] (unsigned formalArgNo) -> llvm::Value* {
                                            if (formalArgNo >= callInst->getNumArgOperands()) {
                                                return nullptr;
                                            }
                                            return callInst->getArgOperand(formalArgNo);
                                        };

    updateLibFunctionCallOutArgDependencies(F, argDepMap, argumentValueGetter);
}

void DependencyAnaliser::updateLibFunctionInvokeInstOutArgDependencies(llvm::InvokeInst* invokeInst,
                                                                       const DependencyAnaliser::ArgumentDependenciesMap& argDepMap)
{
    auto F = invokeInst->getCalledFunction();
    const auto& argumentValueGetter = [&invokeInst] (unsigned formalArgNo) -> llvm::Value* {
                                            if (formalArgNo >= invokeInst->getNumArgOperands()) {
                                                return nullptr;
                                            }
                                            return invokeInst->getArgOperand(formalArgNo);
                                        };

    updateLibFunctionCallOutArgDependencies(F, argDepMap, argumentValueGetter);
}

void DependencyAnaliser::updateLibFunctionCallInstructionDependencies(llvm::CallInst* callInst,
                                                                      const DependencyAnaliser::ArgumentDependenciesMap& argDepMap)
{
    auto F = callInst->getCalledFunction();
    auto Fname = Utils::demangle_name(F->getName());
    if (Fname.empty()) {
        // log msg
        // Try with non-demangled name
        Fname = F->getName();
    }
    auto& libInfo = LibraryInfoManager::get();
    if (!libInfo.hasLibFunctionInfo(Fname)) {
        updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_DEP));
        InputDepInstructionsRecorder::get().record(callInst);
        return;
    }
    libInfo.resolveLibFunctionInfo(F, Fname);
    const auto& libFInfo = libInfo.getLibFunctionInfo(Fname);
    assert(libFInfo.isResolved());
    auto libFuncRetDeps = libFInfo.getResolvedReturnDependency();
    resolveReturnedValueDependencies(libFuncRetDeps, argDepMap);
    updateValueDependencies(callInst, libFuncRetDeps);
    updateInstructionDependencies(callInst, libFuncRetDeps.getValueDep());
}

void DependencyAnaliser::updateLibFunctionInvokeInstructionDependencies(llvm::InvokeInst* invokeInst,
                                                                        const DependencyAnaliser::ArgumentDependenciesMap& argDepMap)
{
    auto F = invokeInst->getCalledFunction();
    auto Fname = Utils::demangle_name(F->getName());
    if (Fname.empty()) {
        // log msg
        // Try with non-demangled name
        Fname = F->getName();
    }
    auto& libInfo = LibraryInfoManager::get();
    if (!libInfo.hasLibFunctionInfo(Fname)) {
        updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_DEP));
        InputDepInstructionsRecorder::get().record(invokeInst);
        return;
    }
    libInfo.resolveLibFunctionInfo(F, Fname);
    const auto& libFInfo = libInfo.getLibFunctionInfo(Fname);
    assert(libFInfo.isResolved());
    auto libFuncRetDeps = libFInfo.getResolvedReturnDependency();
    resolveReturnedValueDependencies(libFuncRetDeps, argDepMap);
    updateValueDependencies(invokeInst, libFuncRetDeps);
    updateInstructionDependencies(invokeInst, libFuncRetDeps.getValueDep());
}

void DependencyAnaliser::updateInputDepLibFunctionCallOutArgDependencies(
                                                                llvm::Function* F,
                                                                const DependencyAnaliser::ArgumentValueGetter& argumentValueGetter)
{
    for (auto& arg : F->getArgumentList()) {
        llvm::Value* actualArg = argumentValueGetter(arg.getArgNo());
        if (!arg.getType()->isPointerTy()) {
            continue;
        }
        llvm::Value* val = getFunctionOutArgumentValue(actualArg);
        if (val == nullptr) {
            continue;
        }
        updateValueDependencies(val, DepInfo(DepInfo::INPUT_DEP));
    }
    // TODO: add this for all call instruction processors
    if (F->isVarArg()) {
        int index = F->getArgumentList().size();
        llvm::Value* actualArg = argumentValueGetter(index);
        while (actualArg != nullptr) {
            if (!actualArg->getType()->isPointerTy()) {
                actualArg = argumentValueGetter(index++);
                continue;
            }
            llvm::Value* val = getFunctionOutArgumentValue(actualArg);
            if (val == nullptr) {
                break;
                //continue;
            }
            updateValueDependencies(val, DepInfo(DepInfo::INPUT_DEP));
            actualArg = argumentValueGetter(index++);
        }
    }
}

ValueDepInfo DependencyAnaliser::getArgumentActualValueDependencies(const ValueSet& valueDeps)
{
    ValueDepInfo info(DepInfo::INPUT_INDEP);
    ValueSet globals;
    for (const auto& val : valueDeps) {
        // Can be non global if the current block is in a loop.
        //assert(llvm::dyn_cast<llvm::GlobalVariable>(val));
        if (!llvm::dyn_cast<llvm::GlobalVariable>(val)) {
            // TODO:
            continue;
        }
        // what if from loop?
        assert(llvm::dyn_cast<llvm::GlobalVariable>(val));
        const auto& depInfo = getValueDependencies(val);
        if (!depInfo.isDefined()) {
            globals.insert(val);
            continue;
        }
        info.mergeDependencies(depInfo);
    }
    if (!globals.empty()) {
        info.mergeDependencies(DepInfo(DepInfo::VALUE_DEP, globals));
    }
    return info;
}

void DependencyAnaliser::finalizeValues(const GlobalVariableDependencyMap& globalDeps)
{
    for (auto& valueDep : m_valueDependencies) {
        auto& info = valueDep.second.getValueDep();
        if (!info.isValueDep()) {
            continue;
        }
        finalizeValueDependencies(globalDeps, info);
        for (auto& el_info : valueDep.second.getCompositeValueDeps()) {
            if (!el_info.isValueDep()) {
                continue;
            }
            finalizeValueDependencies(globalDeps, el_info.getValueDep());
        }
    }
}

void DependencyAnaliser::finalizeInstructions(const GlobalVariableDependencyMap& globalDeps)
{
    auto instrpos = m_inputDependentInstrs.begin();
    while (instrpos != m_inputDependentInstrs.end()) {
        if (!instrpos->second.isValueDep()) {
            ++instrpos;
            continue;
        }
        finalizeValueDependencies(globalDeps, instrpos->second);
        if (instrpos->second.isInputIndep()) {
            auto old = instrpos;
            ++instrpos;
            m_inputIndependentInstrs.insert(old->first);
            m_inputDependentInstrs.erase(old);
        } else {
            ++instrpos;
        }

    }
}

void DependencyAnaliser::finalizeValueDependencies(const GlobalVariableDependencyMap& globalDeps,
                                                   DepInfo& toFinalize)
{
    assert(toFinalize.isValueDep());
    auto& valueDependencies = toFinalize.getValueDependencies();
    const auto& newInfo = getFinalizedDepInfo(valueDependencies, globalDeps);
    assert(newInfo.isDefined());
    if (toFinalize.getDependency() == DepInfo::VALUE_DEP) {
        toFinalize.setDependency(newInfo.getDependency());
    }
    toFinalize.mergeDependencies(newInfo);
    valueDependencies.clear();
}

void DependencyAnaliser::updateDependencyForGetElementPtr(llvm::GetElementPtrInst* getElPtr, const ValueDepInfo& info)
{
    llvm::Value* value = getElPtr->getOperand(0);
    assert(value);
    if (m_valueDependencies.find(value) == m_valueDependencies.end()
        && m_initialDependencies.find(value) == m_initialDependencies.end()) {
        value = getMemoryValue(getElPtr);
    }
    updateValueDependencies(getElPtr, info);
    updateCompositeValueDependencies(value, getElPtr, info);
    updateValueDependencies(getElPtr->getOperand(0), getValueDependencies(value));
    if (auto* value_getElPtr = llvm::dyn_cast<llvm::GetElementPtrInst>(value)) {
        updateDependencyForGetElementPtr(value_getElPtr, info);
    }
}

DependencyAnaliser::ArgumentDependenciesMap DependencyAnaliser::gatherFunctionCallSiteInfo(llvm::CallInst* callInst, llvm::Function* F)
{
    ArgumentDependenciesMap argDepMap;
    for (unsigned i = 0; i < callInst->getNumArgOperands(); ++i) {
        llvm::Value* argVal = callInst->getArgOperand(i);
        const auto& deps = getArgumentValueDependecnies(argVal);
        if (!deps.isDefined()) {
            continue;
        }
        auto arg = getFunctionArgument(F, i);
        if (arg == nullptr) {
            continue;
        }
        argDepMap[arg] = deps;
    }
    return argDepMap;
}

DependencyAnaliser::ArgumentDependenciesMap DependencyAnaliser::gatherFunctionInvokeSiteInfo(llvm::InvokeInst* invokeInst,
                                                                                             llvm::Function* F)
{
    ArgumentDependenciesMap argDepMap;
    for (unsigned i = 0; i < invokeInst->getNumArgOperands(); ++i) {
        llvm::Value* argVal = invokeInst->getArgOperand(i);
        const auto& deps = getArgumentValueDependecnies(argVal);
        if (!deps.isDefined() || deps.isInputIndep()) {
            continue;
        }
        auto arg = getFunctionArgument(F, i);
        if (arg == nullptr) {
            continue;
        }
        argDepMap[arg] = deps;
    }
    return argDepMap;
}

DependencyAnaliser::GlobalVariableDependencyMap DependencyAnaliser::gatherGlobalsForFunctionCall(llvm::Function* F)
{
    const auto& FAG = m_FAG(F);
    if (FAG == nullptr) {
        return GlobalVariableDependencyMap();
    }
    assert(FAG);
    auto& callRefGlobals = FAG->getReferencedGlobals();
    GlobalVariableDependencyMap globalsDepMap;
    for (auto& global : callRefGlobals) {
        llvm::Value* globalVal = llvm::dyn_cast<llvm::Value>(global);
        assert(globalVal != nullptr);
        auto depInfo = getValueDependencies(globalVal);
        if (!depInfo.isDefined()) {
            continue;
        }
        globalsDepMap[global] = depInfo;
    }
    return globalsDepMap;
}


ValueDepInfo DependencyAnaliser::getArgumentValueDependecnies(llvm::Value* argVal)
{
    if (auto constVal = llvm::dyn_cast<llvm::Constant>(argVal)) {
        return ValueDepInfo();
    }
    auto depInfo = getValueDependencies(argVal);
    if (depInfo.isDefined()) {
        return depInfo;
    }
    if (auto* argInst = llvm::dyn_cast<llvm::Instruction>(argVal)) {
        return ValueDepInfo(argVal, getInstructionDependencies(argInst));
    }
    auto args = isInput(argVal);
    if (!args.empty()) {
        return ValueDepInfo(argVal, DepInfo(DepInfo::INPUT_ARGDEP, args));
    }
    return ValueDepInfo();
}

void DependencyAnaliser::updateCallOutArgDependencies(llvm::Function* F,
                                                      const ArgumentDependenciesMap& callArgDeps,
                                                      const DependencyAnaliser::ArgumentValueGetter& argumentValueGetter)
{
    const FunctionAnaliser* FA = m_FAG(F);
    assert(FA != nullptr);
    for (auto& arg : F->getArgumentList()) {
        if (!arg.getType()->isPointerTy()) {
            continue;
        }
        llvm::Value* actualArg = argumentValueGetter(arg.getArgNo());
        llvm::Value* val = getFunctionOutArgumentValue(actualArg);
        auto* instr = llvm::dyn_cast<llvm::Instruction>(actualArg);
        if (FA->isOutArgInputIndependent(&arg)) {
            if (val) {
                updateValueDependencies(val, ValueDepInfo(val, DepInfo::INPUT_INDEP));
            }
            if (instr) {
                updateRefAliasesDependencies(instr, ValueDepInfo(DepInfo::INPUT_INDEP));
            }
            continue;
        }
        auto argDeps = FA->getOutArgDependencies(&arg);
        resolveReturnedValueDependencies(argDeps, callArgDeps);
        if (val) {
            updateValueDependencies(val, argDeps);
        }
        if (instr) {
            updateRefAliasesDependencies(instr, argDeps);
        }
    }
}

void DependencyAnaliser::updateLibFunctionCallOutArgDependencies(llvm::Function* F,
                                                                 const ArgumentDependenciesMap& callArgDeps,
                                                                 const DependencyAnaliser::ArgumentValueGetter& argumentValueGetter)
{
    auto Fname = Utils::demangle_name(F->getName());
    if (Fname.empty()) {
        // log msg
        // Try with non-demangled name
        Fname = F->getName();
    }
    auto& libInfo = LibraryInfoManager::get();
    if (!libInfo.hasLibFunctionInfo(Fname)) {
        updateInputDepLibFunctionCallOutArgDependencies(F, argumentValueGetter);
        return;
    }
    libInfo.resolveLibFunctionInfo(F, Fname);
    const auto& libFInfo = libInfo.getLibFunctionInfo(Fname);
    assert(libFInfo.isResolved());
    for (auto& arg : F->getArgumentList()) {
        if (!arg.getType()->isPointerTy()) {
            continue;
        }
        llvm::Value* actualArg = argumentValueGetter(arg.getArgNo());
        llvm::Value* val = getFunctionOutArgumentValue(actualArg);
        if (val == nullptr) {
            continue;
        }
        if (!libFInfo.hasResolvedArgument(&arg)) {
            continue;
        }
        auto libArgDeps = libFInfo.getResolvedArgumentDependencies(&arg);
        resolveReturnedValueDependencies(libArgDeps, callArgDeps);
        updateValueDependencies(val, libArgDeps);
    }
}

ValueDepInfo DependencyAnaliser::getArgumentActualDependencies(const ArgumentSet& dependencies,
                                                               const ArgumentDependenciesMap& argDepInfo)
{
    ValueDepInfo info(DepInfo(DepInfo::INPUT_INDEP));
    for (const auto& arg : dependencies) {
        auto pos = argDepInfo.find(arg);
        if (pos == argDepInfo.end()) {
            continue;
        }
        info.mergeDependencies(pos->second);
    }
    return info;
}

void DependencyAnaliser::resolveReturnedValueDependencies(ValueDepInfo& valueDeps, const ArgumentDependenciesMap& argDepInfo)
{
    ValueDepInfo resolvedDep;
    if (valueDeps.isInputIndep()) {
        valueDeps.updateCompositeValueDep(DepInfo(DepInfo::INPUT_INDEP));
        return;
    }
    if (valueDeps.isInputDep()) {
        resolvedDep.updateValueDep(DepInfo(DepInfo::INPUT_DEP));
    } else if (valueDeps.isValueDep()) {
        resolvedDep = getArgumentActualValueDependencies(valueDeps.getValueDependencies());
    }
    if (!valueDeps.isInputDep()) {
        resolvedDep.mergeDependencies(getArgumentActualDependencies(valueDeps.getArgumentDependencies(), argDepInfo));
    }
    valueDeps.updateValueDep(resolvedDep);

    for (auto& elDep : valueDeps.getCompositeValueDeps()) {
        resolveReturnedValueDependencies(elDep, argDepInfo);
    }
}

llvm::Value* DependencyAnaliser::getFunctionOutArgumentValue(llvm::Value* actualArg)
{
    if (auto global = llvm::dyn_cast<llvm::GlobalVariable>(actualArg)) {
        auto globalVal = llvm::dyn_cast<llvm::Value>(global);
        return globalVal;
    }
    if (auto* argInstr = llvm::dyn_cast<llvm::Instruction>(actualArg)) {
        if (llvm::dyn_cast<llvm::CallInst>(argInstr)) {
            return nullptr;
        }
        if (llvm::dyn_cast<llvm::AllocaInst>(argInstr)) {
            return getMemoryValue(argInstr);
        }
        return getMemoryValue(argInstr);
    }
    return nullptr;
}

// This function is called for pointerOperand of either storeInst or loadInst.
// It retrives underlying Values which will be affected by these instructions.
// Underlying values can be: alloca instructions, for example for normal assignment operators
// Global Values
// Load instructions for assigning to a pointer
// These values are either allocaInstructions of global values (at least for now).
// TODO: need to process globals with this function
// maybe it's not bad idea to return null if can not find value
llvm::Value* DependencyAnaliser::getMemoryValue(llvm::Value* instrOp)
{
    if (auto* globalVal = llvm::dyn_cast<llvm::GlobalValue>(instrOp)) {
        return globalVal;
    }
    if (auto* constVal = llvm::dyn_cast<llvm::Constant>(instrOp)) {
        return nullptr;
    }
    auto instr = llvm::dyn_cast<llvm::Instruction>(instrOp);
    if (!instr) {
        return instrOp;
    }
    if (auto* bitcast = llvm::dyn_cast<llvm::BitCastInst>(instrOp)) {
        // creating array in a heap (new).
        // Operand 0 is a malloc call, which is marked as input dependent as calls external function.
        // return instruction for now
        return bitcast;
    }

    auto alloca = llvm::dyn_cast<llvm::AllocaInst>(instrOp);
    if (alloca) {
        return alloca;
    }
    auto load = llvm::dyn_cast<llvm::LoadInst>(instrOp);
    if (load) {
        return getMemoryValue(load->getPointerOperand());
    }
    llvm::GlobalValue* global = nullptr;
    bool clean = false;
    llvm::GetElementPtrInst* elPtrInst = llvm::dyn_cast<llvm::GetElementPtrInst>(instrOp);
    if (elPtrInst == nullptr) {
        // This is the case when array element is accessed with constant index.
        if (auto* constGetEl = llvm::dyn_cast<llvm::ConstantExpr>(instrOp)) {
            // This instruction won't be added to any basic block. It is just to get underlying array.
            // We'll need to delete this pointer afterwards, otherwise IR would be invalid.
            // No worries, if forgot to delete, llvm will remind with an error :D
            elPtrInst = llvm::dyn_cast<llvm::GetElementPtrInst>(constGetEl->getAsInstruction());
            clean = true;
        }
    }
    if (elPtrInst == nullptr) {
        assert(!clean);
        return getMemoryValue(instr->getOperand(0));
    }
    auto* op = elPtrInst->getPointerOperand();
    global = llvm::dyn_cast<llvm::GlobalValue>(op);
    if (clean) {
        // Deleting as does not belong to any basic block. 
        delete elPtrInst;
    }
    if (global == nullptr) {
        return getMemoryValue(op);
    }
    return global;
}

} // namespace input_dependency


