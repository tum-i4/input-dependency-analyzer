#include "DependencyAnaliser.h"

#include "InputDepInstructionsRecorder.h"
#include "InputDepConfig.h"
#include "FunctionAnaliser.h"
#include "LibFunctionInfo.h"
#include "LLVMIntrinsicsInfo.h"
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

llvm::Function* getAliasingFunction(llvm::Value* calledValue)
{
    if (auto* alias = llvm::dyn_cast<llvm::GlobalAlias>(calledValue)) {
        auto module = alias->getParent();
        return module->getFunction(alias->getAliasee()->getName());
    }
    return nullptr;
}

llvm::FunctionType* getFunctionType(llvm::Value* val)
{
    llvm::FunctionType* func_type = llvm::dyn_cast<llvm::FunctionType>(val->getType());
    if (!func_type) {
        if (auto* ptr_type = llvm::dyn_cast<llvm::PointerType>(val->getType())) {
            func_type = llvm::dyn_cast<llvm::FunctionType>(ptr_type->getElementType());
        }
    }
    return func_type;
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
                                , m_returnValueDependencies(F->getReturnType())
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
        auto selfF = phi->getParent()->getParent();
        assert(selfF != nullptr);
        auto selfFunctionResults = m_FAG(selfF);
        assert(selfFunctionResults);
        llvm::BasicBlock* incommingBlock = phi->getIncomingBlock(i);
        const auto& blockDep = selfFunctionResults->getBlockDependencyInfo(incommingBlock);
        if (blockDep.isDefined()) {
            info.mergeDependencies(blockDep);
        } else {
            info.mergeDependencies(DepInfo(DepInfo::INPUT_INDEP));
        }
        if (llvm::dyn_cast<llvm::Constant>(val)) {
            continue;
        }
        const auto& valDeps = getValueDependencies(val);
        if (valDeps.isDefined()) {
            info.mergeDependencies(valDeps.getValueDep());
        } else {
            const auto& deps = selfFunctionResults->getDependencyInfoFromBlock(val,
                                                    phi->getIncomingBlock(i));
            const auto& depInfofromBlock = deps.getValueDep();
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
    updateValueDependencies(bitcast, ValueDepInfo(depInfo), false);
    updateInstructionDependencies(bitcast, depInfo);
}

void DependencyAnaliser::processGetElementPtrInst(llvm::GetElementPtrInst* getElPtr)
{
    // example of getElementPtrInstr
    // p.x where x is the second field of struct p
    // %x = getelementptr inbounds %struct.point, %struct.point* %p, i32 0, i32 1
    // for int *p; p[0]
    // %arrayidx = getelementptr inbounds i32, i32* %0, i64 0, where %0 is load of p
    // first check input dependency of indices
    DepInfo indexDepInfo;
    auto idx_it = getElPtr->idx_begin();
    while (idx_it != getElPtr->idx_end()) {
        if (auto* idx_inst = llvm::dyn_cast<llvm::Instruction>(&*idx_it)) {
            indexDepInfo = getInstructionDependencies(idx_inst);
            if (indexDepInfo.isInputDep()) {
                break;
            }
        }
        ++idx_it;
    }
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
            depInfo = ValueDepInfo(compositeValue->getType(), getInstructionDependencies(instr));
        } else {
            depInfo = ValueDepInfo(compositeValue->getType(), DepInfo(DepInfo::INPUT_DEP));
        }
    }
    depInfo.mergeDependencies(ValueDepInfo(indexDepInfo));
    updateInstructionDependencies(getElPtr, depInfo.getValueDep());
    updateValueDependencies(getElPtr, depInfo, false); // add getElPtr as value
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
            retDepInfo = ValueDepInfo(retValue->getType(), getInstructionDependencies(retValInst));
        }
    }
    if (!retDepInfo.isDefined()) {
        retDepInfo = ValueDepInfo(retValue->getType(), DepInfo(DepInfo::INPUT_INDEP));
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
    auto storeTo = storeInst->getPointerOperand();
    removeCallbackFunctionsForValue(storeTo);
    ValueDepInfo info(op->getType(), DepInfo()); // the value here is not important, could be null
    // assigning function to a variable
    if (auto* function = llvm::dyn_cast<llvm::Function>(op)) {
        info.updateCompositeValueDep(DepInfo(DepInfo::INPUT_INDEP));
        m_functionValues[storeTo].insert(function);
    } else if (auto* func_type = getFunctionType(op)) {
        if (m_indirectCallsInfo.hasIndirectTargets(func_type)) {
            for (const auto& target : m_indirectCallsInfo.getIndirectTargets(func_type)) {
                m_functionValues[storeTo].insert(target);
            }
        } else {
            llvm::dbgs() << "Did not find function assigned " << *storeInst << "\n";
        }
    } else if (llvm::dyn_cast<llvm::Constant>(op)) {
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
    if (auto global = llvm::dyn_cast<llvm::GlobalVariable>(storeTo)) {
        m_modifiedGlobals.insert(global);
    }
    updateInstructionDependencies(storeInst, info.getValueDep());
    // Whatever storeTo is (value or instruction) is going to be collected in value list. 
    if (auto* getElPtr = llvm::dyn_cast<llvm::GetElementPtrInst>(storeTo)) {
        updateDependencyForGetElementPtr(getElPtr, info);
    } else {
        updateValueDependencies(storeTo, info, true);
        //updateModAliasesDependencies(storeInst, info);
        llvm::Value* memoryValue = getMemoryValue(storeTo);
        if (memoryValue && memoryValue != storeTo) {
            updateValueDependencies(memoryValue, info, true);
        }
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
            } else if (m_indirectCallsInfo.hasIndirectTargets(callInst)) {
                processCallSiteWithMultipleTargets(callInst, m_indirectCallsInfo.getIndirectTargets(callInst));
            } else {
                // make all out args input dependent
                updateCallInputDependentOutArgDependencies(callInst);
                // make return value input dependent
                updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_DEP));
                updateValueDependencies(callInst, DepInfo(DepInfo::INPUT_DEP), false);
                InputDepInstructionsRecorder::get().record(callInst);
                // make all globals input dependent?
            }
        }
        return;
    }
    if (Utils::isLibraryFunction(F, m_F->getParent())) {
        const ArgumentDependenciesMap& argDepMap = gatherFunctionCallSiteInfo(callInst, F);
        updateLibFunctionCallInstOutArgDependencies(callInst, argDepMap);
        updateLibFunctionCallInstructionDependencies(callInst, argDepMap);
    } else {
        updateFunctionCallSiteInfo(callInst, F);
        if (m_FAG(F) != nullptr) {
            updateCallSiteOutArgDependencies(callInst, F);
        } else {
            updateCallInputDependentOutArgDependencies(callInst);
        }
        // cyclic call
        // analysis result of callee is not available. e.g cyclic calls, recursive calls
        if (m_FAG(F) == nullptr || m_FAG(F)->isInputDepFunction()) {
            updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_DEP));
            updateValueDependencies(callInst, DepInfo(DepInfo::INPUT_DEP), false);
        } else {
            updateCallInstructionDependencies(callInst, F);
            updateGlobalsAfterFunctionCall(callInst, F);
        }
    }
}

void DependencyAnaliser::processInvokeInst(llvm::InvokeInst* invokeInst)
{
    llvm::Function* F = invokeInst->getCalledFunction();
    // can throw
    bool throws = invokeInst->getNormalDest() || invokeInst->getUnwindDest();
    if (F == nullptr) {
        // This could happen for example when calling virtual functions
        // try see if has alias
        F = getAliasingFunction(invokeInst->getCalledValue());
        if (F == nullptr) {
            if (m_virtualCallsInfo.hasVirtualCallCandidates(invokeInst)) {
                processInvokeSiteWithMultipleTargets(invokeInst, m_virtualCallsInfo.getVirtualCallCandidates(invokeInst));
            } else if (m_indirectCallsInfo.hasIndirectTargets(invokeInst)) {
                processInvokeSiteWithMultipleTargets(invokeInst, m_indirectCallsInfo.getIndirectTargets(invokeInst));
            } else {
                // make all out args input dependent
                updateInvokeInputDependentOutArgDependencies(invokeInst);
                // make return value input dependent
                updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_DEP));
                updateValueDependencies(invokeInst, DepInfo(DepInfo::INPUT_DEP), false);
                InputDepInstructionsRecorder::get().record(invokeInst);
                // make all globals input dependent?
            }
        }
        if (throws) {
            updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_DEP));
        }
        return;
    }
    if (Utils::isLibraryFunction(F, m_F->getParent())) {
        const ArgumentDependenciesMap& argDepMap = gatherFunctionInvokeSiteInfo(invokeInst, F);
        updateLibFunctionInvokeInstOutArgDependencies(invokeInst, argDepMap);
        updateLibFunctionInvokeInstructionDependencies(invokeInst, argDepMap);
    } else {
        updateFunctionInvokeSiteInfo(invokeInst, F);
        // cyclic call
        if (m_FAG(F) == nullptr || m_FAG(F)->isInputDepFunction()) {
            updateInvokeInputDependentOutArgDependencies(invokeInst);
            updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_DEP));
            updateValueDependencies(invokeInst, DepInfo(DepInfo::INPUT_DEP), false);
        } else {
            updateInvokeSiteOutArgDependencies(invokeInst, F);
            updateInvokeInstructionDependencies(invokeInst, F);
            updateGlobalsAfterFunctionInvoke(invokeInst, F);
        }
    }
    if (throws) {
        updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_DEP));
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
            updateValueDependencies(callInst, DepInfo(DepInfo::INPUT_DEP), false);
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
            updateValueDependencies(invokeInst, DepInfo(DepInfo::INPUT_DEP), false);
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
        // just adding a value. in fact should not alias with any other value
        updateValueDependencies(callInst, ValueDepInfo(callInst->getType(), DepInfo(DepInfo::INPUT_INDEP)), false);
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
    updateValueDependencies(callInst, retDeps, false);
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
        updateValueDependencies(invokeInst, ValueDepInfo(invokeInst->getType(), DepInfo(DepInfo::INPUT_INDEP)), false);
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
    updateValueDependencies(invokeInst, retDeps, false);
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
        ValueDepInfo depInfo(global->getType());
        if (is_recurs) {
            depInfo = getValueDependencies(global);
            if (depInfo.isDefined()) {
                addControlDependencies(depInfo);
            }
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
            updateValueDependencies(val, depInfo, true);
            continue;
        }
        resolveReturnedValueDependencies(depInfo, functionArgDeps);
        updateValueDependencies(val, depInfo, true);
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
        updateOutArgumentDependencies(argVal,  ValueDepInfo(argVal->getType(), DepInfo(DepInfo::INPUT_DEP)));
    }
}

void DependencyAnaliser::updateOutArgumentDependencies(llvm::Value* val, const ValueDepInfo& depInfo)
{
    // val is the value passed as argument
    updateValueDependencies(val, depInfo, true);

    // if it's a global snake_move_player
    if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(val)) {
        m_modifiedGlobals.insert(global);
        return;
    }
    // see if needs to update dependencies of underlying types
    llvm::Value* memory_value = getFunctionOutArgumentValue(val);
    if (!memory_value) {
        return;
    }
    if (auto load_inst = llvm::dyn_cast<llvm::LoadInst>(val)) {
        llvm::Value* loaded_val = load_inst->getPointerOperand();
        if (loaded_val != memory_value) {
            if (auto* loaded_inst = llvm::dyn_cast<llvm::Instruction>(loaded_val)) {
                // in case if loaded_val is getElementPtr this will updte corresponding element of memory_value
                // if it's not, will update memory_value
                updateCompositeValueDependencies(memory_value, loaded_inst, depInfo);
            }
        } else {
            updateValueDependencies(memory_value, depInfo, true);
        }
    } else if (auto* inst = llvm::dyn_cast<llvm::Instruction>(val)) {
        updateCompositeValueDependencies(memory_value, inst, depInfo);
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
    if (F->isIntrinsic()) {
        const auto& intrinsic_name = LLVMIntrinsicsInfo::get_intrinsic_name(Fname);
        if (!intrinsic_name.empty()) {
            Fname = intrinsic_name;
        }
    }
    auto& libInfo = LibraryInfoManager::get();
    if (!libInfo.hasLibFunctionInfo(Fname)) {
        updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_DEP));
        updateValueDependencies(callInst, DepInfo(DepInfo::INPUT_DEP), false);
        InputDepInstructionsRecorder::get().record(callInst);
        return;
    }
    libInfo.resolveLibFunctionInfo(F, Fname);
    const auto& libFInfo = libInfo.getLibFunctionInfo(Fname);
    assert(libFInfo.isResolved());
    auto libFuncRetDeps = libFInfo.getResolvedReturnDependency();
    resolveReturnedValueDependencies(libFuncRetDeps, argDepMap);
    updateValueDependencies(callInst, libFuncRetDeps, false);
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
        updateValueDependencies(invokeInst, DepInfo(DepInfo::INPUT_DEP), false);
        InputDepInstructionsRecorder::get().record(invokeInst);
        return;
    }
    libInfo.resolveLibFunctionInfo(F, Fname);
    const auto& libFInfo = libInfo.getLibFunctionInfo(Fname);
    assert(libFInfo.isResolved());
    auto libFuncRetDeps = libFInfo.getResolvedReturnDependency();
    resolveReturnedValueDependencies(libFuncRetDeps, argDepMap);
    updateValueDependencies(invokeInst, libFuncRetDeps, false);
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
        updateOutArgumentDependencies(actualArg, ValueDepInfo(DepInfo(DepInfo::INPUT_DEP)));
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
            updateOutArgumentDependencies(actualArg, ValueDepInfo(DepInfo(DepInfo::INPUT_DEP)));
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
        auto depInfo = getValueDependencies(val);
        if (!depInfo.isDefined()) {
            globals.insert(val);
            continue;
        }
        addControlDependencies(depInfo);
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
    finalizeInstructions(globalDeps, m_inputDependentInstrs);
    auto instrpos = m_inputDependentInstrs.begin();
    while (instrpos != m_inputDependentInstrs.end()) {
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

void DependencyAnaliser::updateDependencyForGetElementPtr(llvm::GetElementPtrInst* getElPtr, const ValueDepInfo& info)
{
    llvm::Value* memory_value = getMemoryValue(getElPtr);
    llvm::Value* value = getElPtr->getOperand(0);
    assert(value);
    if (m_valueDependencies.find(value) == m_valueDependencies.end()
        && m_initialDependencies.find(value) == m_initialDependencies.end()) {
        value = memory_value;
        memory_value = nullptr; // just not to process the same twice
    }
    updateValueDependencies(getElPtr, info, true);
    if (!value) {
        return;
    }
    updateCompositeValueDependencies(value, getElPtr, info);
    if (memory_value && memory_value != value) {
        updateCompositeValueDependencies(memory_value, getElPtr, info);
        updateValueDependencies(memory_value, getValueDependencies(value), true);
    }
    if (!memory_value) {
        updateValueDependencies(getElPtr->getOperand(0), getValueDependencies(value), true);
    }
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

// TODO: these two functions can be mered in one template function
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
        m_referencedGlobals.insert(global);
        assert(globalVal != nullptr);
        auto depInfo = getValueDependencies(globalVal);
        if (!depInfo.isDefined()) {
            depInfo = ValueDepInfo(DepInfo(DepInfo::VALUE_DEP, ValueSet{globalVal}));
        }
        addControlDependencies(depInfo);
        globalsDepMap[global] = depInfo;
    }
    return globalsDepMap;
}


ValueDepInfo DependencyAnaliser::getArgumentValueDependecnies(llvm::Value* argVal)
{
    if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(argVal)) {
        auto depInfo = getValueDependencies(argVal);
        if (!depInfo.isDefined()) {
            depInfo = ValueDepInfo(argVal->getType(), DepInfo(DepInfo::VALUE_DEP, ValueSet{global}));
        }
        addControlDependencies(depInfo);
        return depInfo;
    }
    if (auto constVal = llvm::dyn_cast<llvm::Constant>(argVal)) {
        return ValueDepInfo(DepInfo(DepInfo::INPUT_INDEP));
    }
    auto depInfo = getValueDependencies(argVal);
    if (depInfo.isDefined()) {
        //addControlDependencies(depInfo);
        return depInfo;
    }
    if (auto* argInst = llvm::dyn_cast<llvm::Instruction>(argVal)) {
        auto instrDeps = getInstructionDependencies(argInst);
        //addControlDependencies(instrDeps);
        return ValueDepInfo(argVal->getType(), instrDeps);
    }
    auto args = isInput(argVal);
    if (!args.empty()) {
        DepInfo depInfo(DepInfo::INPUT_ARGDEP, args);
        //addControlDependencies(depInfo);
        return ValueDepInfo(argVal->getType(), depInfo);
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
        auto* instr = llvm::dyn_cast<llvm::Instruction>(actualArg);
        if (FA->isOutArgInputIndependent(&arg)) {
            updateOutArgumentDependencies(actualArg, ValueDepInfo(DepInfo(DepInfo::INPUT_INDEP)));
            if (instr) {
                updateRefAliasesDependencies(instr, ValueDepInfo(instr->getType(), DepInfo::INPUT_INDEP));
            }
            continue;
        }
        auto argDeps = FA->getOutArgDependencies(&arg);
        resolveReturnedValueDependencies(argDeps, callArgDeps);
        updateOutArgumentDependencies(actualArg, argDeps);
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
        // Try with non-demangled name
        Fname = F->getName();
    }
    if (F->isIntrinsic()) {
        const auto& intrinsic_name = LLVMIntrinsicsInfo::get_intrinsic_name(Fname);
        if (!intrinsic_name.empty()) {
            Fname = intrinsic_name;
        }
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
        llvm::Value* actualArg = argumentValueGetter(arg.getArgNo());
        if (!actualArg) {
            llvm::dbgs() << "No actual value for formal argument " << arg << "\n";
        }
        if (libFInfo.isCallbackArgument(&arg)) {
            if (auto* arg_F = llvm::dyn_cast<llvm::Function>(actualArg)) {
                llvm::dbgs() << "Set input dependency of a function " << arg_F->getName() << "\n";
                auto arg_FA = m_FAG(arg_F);
                if (arg_FA) {
                    arg_FA->setIsInputDepFunction(true);
                }
                InputDepConfig::get().add_input_dep_function(arg_F);
                auto pos = m_functionCallInfo.insert(std::make_pair(arg_F, FunctionCallDepInfo(*arg_F)));
                pos.first->second.setIsCallback(true);
                m_calledFunctions.insert(arg_F);
            } else {
                markCallbackFunctionsForValue(actualArg);
            }
        }
        if (!arg.getType()->isPointerTy()) {
            continue;
        }
        if (!libFInfo.hasResolvedArgument(&arg)) {
            continue;
        }
        auto libArgDeps = libFInfo.getResolvedArgumentDependencies(&arg);
        resolveReturnedValueDependencies(libArgDeps, callArgDeps);
        updateOutArgumentDependencies(actualArg, libArgDeps);
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
    valueDeps.updateValueDep(resolvedDep.getValueDep());

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

    auto instr = llvm::dyn_cast<llvm::Instruction>(instrOp);
    if (!instr) {
        if (auto* constExpr = llvm::dyn_cast<llvm::ConstantExpr>(instrOp)) {
            auto constInstr = constExpr->getAsInstruction();
            auto memVal = getMemoryValue(constInstr->getOperand(0));
            delete constInstr;
            return memVal;
        }
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
    if (auto* constVal = llvm::dyn_cast<llvm::Constant>(instrOp)) {
        return nullptr;
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
    //global = llvm::dyn_cast<llvm::GlobalValue>(op);
    if (clean) {
        // Deleting as does not belong to any basic block. 
        delete elPtrInst;
    }
    if (op == nullptr) {
        return getMemoryValue(op);
    }
    return op;
}

void DependencyAnaliser::finalizeInstructions(const GlobalVariableDependencyMap& globalDeps,
                                              InstrDependencyMap& instructions)
{
    auto instrpos = instructions.begin();
    while (instrpos != instructions.end()) {
        if (!instrpos->second.isValueDep()) {
            ++instrpos;
            continue;
        }
        finalizeValueDependencies(globalDeps, instrpos->second);
        ++instrpos;
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

DepInfo DependencyAnaliser::getFinalizedDepInfo(const ValueSet& values,
                                                const DependencyAnaliser::GlobalVariableDependencyMap& globalDeps)
{
    DepInfo newInfo(DepInfo::INPUT_INDEP);
    for (auto& item : values) {
        auto global = llvm::dyn_cast<llvm::GlobalVariable>(item);
        if (global == nullptr) {
            continue;
        }
        assert(global != nullptr);
        auto pos = globalDeps.find(global);
        if (pos == globalDeps.end()) {
            continue;
        }
        assert(pos->second.isDefined());
        assert(!pos->second.getDependency() != DepInfo::VALUE_DEP);
        ValueDepInfo globalDepInfo = pos->second;
        auto internal_pos = m_valueDependencies.find(global);
        if (internal_pos != m_valueDependencies.end()) {
            // TODO: internal value dependency?
            globalDepInfo.mergeDependencies(internal_pos->second);
        }
        newInfo.mergeDependencies(globalDepInfo.getArgumentDependencies());
        newInfo.mergeDependency(globalDepInfo.getDependency());
    }
    return newInfo;
}


} // namespace input_dependency


