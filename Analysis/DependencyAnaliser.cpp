#include "DependencyAnaliser.h"

#include "Utils.h"
#include "FunctionAnaliser.h"
#include "LibFunctionInfo.h"
#include "LibraryInfoManager.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"

namespace {

llvm::Argument* getFunctionArgument(llvm::Function* F, unsigned index)
{
    auto it = F->getArgumentList().begin();
    while (index-- != 0) {
        ++it;
    }
    return &*it;
}
} // unnamed namespace

namespace input_dependency {

DependencyAnaliser::DependencyAnaliser(llvm::Function* F,
                                       llvm::AAResults& AAR,
                                       const Arguments& inputs,
                                       const FunctionAnalysisGetter& Fgetter)
                                : m_F(F)
                                , m_AAR(AAR)
                                , m_inputs(inputs)
                                , m_FAG(Fgetter)
                                , m_finalized(false)
{
}

void DependencyAnaliser::finalize(const ArgumentDependenciesMap& dependentArgs)
{
    for (auto& item : m_inputDependentInstrs) {
        if (item.second.isInputDep()) {
            m_finalInputDependentInstrs.insert(item.first);
        } else if (item.second.isInputArgumentDep() && Utils::haveIntersection(dependentArgs, item.second.getArgumentDependencies())) {
            m_finalInputDependentInstrs.insert(item.first);
        } else {
            m_inputIndependentInstrs.insert(item.first);
        }
    }
    for (auto& callInfo : m_functionCallInfo) {
        callInfo.second.finalize(dependentArgs);
    }
    m_finalized = true;
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
    }
    llvm::dbgs() << "\n";
}

void DependencyAnaliser::processInstruction(llvm::Instruction* inst)
{
    updateInstructionDependencies(inst, getInstructionDependencies(inst));
}

void DependencyAnaliser::processReturnInstr(llvm::ReturnInst* retInst)
{
    auto retValue = retInst->getReturnValue();
    if (!retValue) {
        return;
    }
    if (auto* constVal = llvm::dyn_cast<llvm::Constant>(retValue)) {
        updateInstructionDependencies(retInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }
    DepInfo depnums;
    if (auto* retValInst = llvm::dyn_cast<llvm::Instruction>(retValue)) {
        depnums = getInstructionDependencies(retValInst);
    } else {
        depnums = getValueDependencies(retValue);
    }
    updateInstructionDependencies(retInst, depnums);
    updateReturnValueDependencies(depnums);
}

void DependencyAnaliser::processBranchInst(llvm::BranchInst* branchInst)
{
    if (branchInst->isUnconditional()) {
        updateInstructionDependencies(branchInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }

    auto condition = branchInst->getCondition();
    if (auto* constCond = llvm::dyn_cast<llvm::Constant>(condition)) {
        updateInstructionDependencies(branchInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }
    DepInfo dependencies;
    if (auto* condInstr = llvm::dyn_cast<llvm::Instruction>(condition)) {
        dependencies = getInstructionDependencies(condInstr);
    } else {
        // Note: it is important to have this check after instruction as Instruction inherits from Value
        if (auto* condVal = llvm::dyn_cast<llvm::Value>(condition)) {
            dependencies = getValueDependencies(condVal);
        }
    }
    updateInstructionDependencies(branchInst, dependencies);
}

void DependencyAnaliser::processStoreInst(llvm::StoreInst* storeInst)
{
    auto storeTo = storeInst->getPointerOperand();
    auto storedValue = getMemoryValue(storeTo);
    assert(storedValue);
    auto op = storeInst->getOperand(0);
    if (auto* constOp = llvm::dyn_cast<llvm::Constant>(op)) {
        updateInstructionDependencies(storeInst, DepInfo(DepInfo::INPUT_INDEP));
        updateValueDependencies(storedValue, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }

    auto args = isInput(op);
    if (!args.empty()) {
        updateInstructionDependencies(storeInst, DepInfo(DepInfo::INPUT_ARGDEP, args));
        updateValueDependencies(storedValue, DepInfo(DepInfo::INPUT_ARGDEP, args));
        return;
    }
    auto pos = m_valueDependencies.find(op);
    if (pos != m_valueDependencies.end()) {
        updateInstructionDependencies(storeInst, pos->second);
        updateValueDependencies(storedValue, pos->second);
        return;
    }
    auto* opInstr = llvm::dyn_cast<llvm::Instruction>(op);
    assert(opInstr);
    // Store instruction depends only on the value which is being stored.
    const auto& deps = getInstructionDependencies(opInstr);
    updateInstructionDependencies(storeInst, deps);
    updateValueDependencies(storedValue, deps);
    updateAliasesDependencies(storeTo, deps);
}

void DependencyAnaliser::processCallInst(llvm::CallInst* callInst)
{
    llvm::Function* F = callInst->getCalledFunction();
    if (F == nullptr) {
        // This could happen for example when calling virtual functions
        return;
    }
    if (Utils::isLibraryFunction(F, m_F->getParent())) {
        const ArgumentDependenciesMap& argDepMap = gatherFunctionCallSiteInfo(callInst);
        updateLibFunctionCallInstOutArgDependencies(callInst, argDepMap);
        updateLibFunctionCallInstructionDependencies(callInst, argDepMap);
    } else {
        updateFunctionCallSiteInfo(callInst);
        updateCallSiteOutArgDependencies(callInst);
        updateCallInstructionDependencies(callInst);
    }
}

void DependencyAnaliser::processInvokeInst(llvm::InvokeInst* invokeInst)
{
    llvm::Function* F = invokeInst->getCalledFunction();
    if (F == nullptr) {
        // This could happen for example when calling virtual functions
        return;
    }
    if (Utils::isLibraryFunction(F, m_F->getParent())) {
        const ArgumentDependenciesMap& argDepMap = gatherFunctionInvokeSiteInfo(invokeInst);
        updateLibFunctionInvokeInstOutArgDependencies(invokeInst, argDepMap);
        updateLibFunctionInvokeInstructionDependencies(invokeInst, argDepMap);
    } else {
        updateFunctionInvokeSiteInfo(invokeInst);
        updateInvokeSiteOutArgDependencies(invokeInst);
        updateInvokeInstructionDependencies(invokeInst);
    }
}

void DependencyAnaliser::processInstrForOutputArgs(llvm::Instruction* I)
{
    if (m_outArgDependencies.empty()) {
        return;
    }
    const auto& DL = I->getModule()->getDataLayout();
    auto item = m_outArgDependencies.begin();
    while (item != m_outArgDependencies.end()) {
        llvm::Value* val = llvm::dyn_cast<llvm::Value>(item->first);
        // If DepInfo::INPUT_DEP instruction modifies given output argument, this argument depends on the same inputs as the instruction.
        const auto& info = m_AAR.getModRefInfo(I, val, DL.getTypeStoreSize(val->getType()));
        if (info != llvm::ModRefInfo::MRI_Mod) {
            ++item;
            continue;
        }
        auto pos = m_inputDependentInstrs.find(I);
        if (pos != m_inputDependentInstrs.end()) {
            item->second = pos->second;
            //item->second.mergeDependencies(pos->second);
        } else {
            item->second = DepInfo(DepInfo::INPUT_INDEP);
        }
        ++item;
    }
}
    
void DependencyAnaliser::updateFunctionCallSiteInfo(llvm::CallInst* callInst)
{
    auto F = callInst->getCalledFunction();
    auto pos = m_functionCallInfo.insert(std::make_pair(F, FunctionCallDepInfo(*F)));
    m_calledFunctions.insert(F);

    const ArgumentDependenciesMap& argDepMap = gatherFunctionCallSiteInfo(callInst);
    pos.first->second.addCall(callInst, argDepMap);
}

void DependencyAnaliser::updateFunctionInvokeSiteInfo(llvm::InvokeInst* invokeInst)
{
    auto F = invokeInst->getCalledFunction();
    auto pos = m_functionCallInfo.insert(std::make_pair(F, FunctionCallDepInfo(*F)));
    m_calledFunctions.insert(F);

    const ArgumentDependenciesMap& argDepMap = gatherFunctionInvokeSiteInfo(invokeInst);
    pos.first->second.addInvoke(invokeInst, argDepMap);
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


void DependencyAnaliser::updateCallSiteOutArgDependencies(llvm::CallInst* callInst)
{
    auto F = callInst->getCalledFunction();
    auto pos = m_functionCallInfo.find(F);
    assert(pos != m_functionCallInfo.end());
    const auto& callArgDeps = pos->second.getDependenciesForCall(callInst);

    const auto& actualArgumentGetter = [&callInst] (const llvm::Argument& formalArg) -> llvm::Value* {
                                            return callInst->getArgOperand(formalArg.getArgNo());
                                        };
    updateCallOutArgDependencies(F, callArgDeps, actualArgumentGetter);
}

void DependencyAnaliser::updateInvokeSiteOutArgDependencies(llvm::InvokeInst* invokeInst)
{
    auto F = invokeInst->getCalledFunction();
    auto pos = m_functionCallInfo.find(F);
    assert(pos != m_functionCallInfo.end());
    const auto& invokeArgDeps = pos->second.getDependenciesForInvoke(invokeInst);

    const auto& actualArgumentGetter = [&invokeInst] (const llvm::Argument& formalArg) -> llvm::Value* {
                                            return invokeInst->getArgOperand(formalArg.getArgNo());
                                        };
    updateCallOutArgDependencies(F, invokeArgDeps, actualArgumentGetter);
}

void DependencyAnaliser::updateCallInstructionDependencies(llvm::CallInst* callInst)
{
    auto F = callInst->getCalledFunction();
    if (F->doesNotReturn()) {
        updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }
    const FunctionAnaliser* FA = m_FAG(F);
    assert(FA);
    if (!FA->isReturnValueInputDependent()) {
        updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }
    const auto& retDeps = FA->getRetValueDependencies();
    if (retDeps.isInputDep()) {
        // is input dependent, but not dependent on arguments
        updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_DEP));
        return;
    }
    auto pos = m_functionCallInfo.find(F);
    assert(pos != m_functionCallInfo.end());
    auto dependencies = getArgumentActualDependencies(retDeps.getArgumentDependencies(),
                                                      pos->second.getDependenciesForCall(callInst));
    updateInstructionDependencies(callInst, dependencies);
}

void DependencyAnaliser::updateInvokeInstructionDependencies(llvm::InvokeInst* invokeInst)
{
    auto F = invokeInst->getCalledFunction();
    if (F->doesNotReturn()) {
        updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }
    const FunctionAnaliser* FA = m_FAG(F);
    assert(FA);
    if (!FA->isReturnValueInputDependent()) {
        updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }
    const auto& retDeps = FA->getRetValueDependencies();
    if (retDeps.isInputDep()) {
        // is input dependent, but not dependent on arguments
        updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_DEP));
        return;
    }
    auto pos = m_functionCallInfo.find(F);
    assert(pos != m_functionCallInfo.end());
    auto dependencies = getArgumentActualDependencies(retDeps.getArgumentDependencies(),
                                                      pos->second.getDependenciesForInvoke(invokeInst));
    updateInstructionDependencies(invokeInst, dependencies);
}

void DependencyAnaliser::updateLibFunctionCallInstOutArgDependencies(llvm::CallInst* callInst,
                                                                     const DependencyAnaliser::ArgumentDependenciesMap& argDepMap)
{
    auto F = callInst->getCalledFunction();
    const auto& actualArgumentGetter = [&callInst] (const llvm::Argument& formalArg) -> llvm::Value* {
                                            return callInst->getArgOperand(formalArg.getArgNo());
                                        };

    updateLibFunctionCallOutArgDependencies(F, argDepMap, actualArgumentGetter);
}

void DependencyAnaliser::updateLibFunctionInvokeInstOutArgDependencies(llvm::InvokeInst* invokeInst,
                                                                       const DependencyAnaliser::ArgumentDependenciesMap& argDepMap)
{
    auto F = invokeInst->getCalledFunction();
    const auto& actualArgumentGetter = [&invokeInst] (const llvm::Argument& formalArg) -> llvm::Value* {
                                            return invokeInst->getArgOperand(formalArg.getArgNo());
                                        };

    updateLibFunctionCallOutArgDependencies(F, argDepMap, actualArgumentGetter);
}

void DependencyAnaliser::updateLibFunctionCallInstructionDependencies(llvm::CallInst* callInst,
                                                                      const DependencyAnaliser::ArgumentDependenciesMap& argDepMap)
{
    auto F = callInst->getCalledFunction();
    const auto& Fname = F->getName();
    auto& libInfo = LibraryInfoManager::get();
    if (!libInfo.hasLibFunctionInfo(Fname)) {
        updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_DEP));
        return;
    }
    libInfo.resolveLibFunctionInfo(F);
    const auto& libFInfo = libInfo.getLibFunctionInfo(Fname);
    assert(libFInfo.isResolved());
    const auto& libFuncRetDeps = libFInfo.getResolvedReturnDependency();
    if (libFuncRetDeps.isInputIndep()) {
        updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_INDEP));
    } else {
        auto dependencies = getArgumentActualDependencies(libFuncRetDeps.getArgumentDependencies(), argDepMap);
        updateInstructionDependencies(callInst, dependencies);
    }
}

void DependencyAnaliser::updateLibFunctionInvokeInstructionDependencies(llvm::InvokeInst* invokeInst,
                                                                        const DependencyAnaliser::ArgumentDependenciesMap& argDepMap)
{
    auto F = invokeInst->getCalledFunction();
    const auto& Fname = F->getName();
    auto& libInfo = LibraryInfoManager::get();
    if (!libInfo.hasLibFunctionInfo(Fname)) {
        updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_DEP));
        return;
    }
    libInfo.resolveLibFunctionInfo(F);
    const auto& libFInfo = libInfo.getLibFunctionInfo(Fname);
    assert(libFInfo.isResolved());
    const auto& libFuncRetDeps = libFInfo.getResolvedReturnDependency();
    if (libFuncRetDeps.isInputIndep()) {
        updateInstructionDependencies(invokeInst, DepInfo(DepInfo::INPUT_INDEP));
    } else {
        auto dependencies = getArgumentActualDependencies(libFuncRetDeps.getArgumentDependencies(), argDepMap);
        updateInstructionDependencies(invokeInst, dependencies);
    }
}

void DependencyAnaliser::updateInputDepLibFunctionCallOutArgDependencies(
                                                                llvm::Function* F,
                                                                const DependencyAnaliser::ActuralArgumentGetter& actualArgumentGetter)
{
    for (auto& arg : F->getArgumentList()) {
        llvm::Value* actualArg = actualArgumentGetter(arg);
        llvm::Value* val = getFunctionOutArgumentValue(actualArg, arg);
        if (val == nullptr) {
            continue;
        }
        updateValueDependencies(val, DepInfo(DepInfo::INPUT_DEP));
    }
}

DependencyAnaliser::ArgumentDependenciesMap DependencyAnaliser::gatherFunctionCallSiteInfo(llvm::CallInst* callInst)
{
    llvm::Function* F = callInst->getCalledFunction();
    ArgumentDependenciesMap argDepMap;
    for (unsigned i = 0; i < callInst->getNumArgOperands(); ++i) {
        llvm::Value* argVal = callInst->getArgOperand(i);
        const auto& deps = getArgumentValueDependecnies(argVal);
        if (!deps.isDefined() || deps.isInputIndep()) {
            continue;
        }
        auto arg = getFunctionArgument(F, i);
        argDepMap[arg] = deps;
    }
    return argDepMap;
}

DependencyAnaliser::ArgumentDependenciesMap DependencyAnaliser::gatherFunctionInvokeSiteInfo(llvm::InvokeInst* invokeInst)
{
    llvm::Function* F = invokeInst->getCalledFunction();
    ArgumentDependenciesMap argDepMap;
    for (unsigned i = 0; i < invokeInst->getNumArgOperands(); ++i) {
        llvm::Value* argVal = invokeInst->getArgOperand(i);
        const auto& deps = getArgumentValueDependecnies(argVal);
        if (!deps.isDefined() || deps.isInputIndep()) {
            continue;
        }
        auto arg = getFunctionArgument(F, i);
        argDepMap[arg] = deps;
    }
    return argDepMap;
}

DepInfo DependencyAnaliser::getArgumentValueDependecnies(llvm::Value* argVal)
{
    if (auto constVal = llvm::dyn_cast<llvm::Constant>(argVal)) {
        return DepInfo();
    }
    auto pos = m_valueDependencies.find(argVal);
    if (pos != m_valueDependencies.end()) {
        return pos->second;
    }
    auto args = isInput(argVal);
    if (!args.empty()) {
        return DepInfo(DepInfo::INPUT_ARGDEP, args);
    }
    if (auto* argInst = llvm::dyn_cast<llvm::Instruction>(argVal)) {
        return getInstructionDependencies(argInst);
    }
    return DepInfo();
}

void DependencyAnaliser::updateCallOutArgDependencies(llvm::Function* F,
                                                      const ArgumentDependenciesMap& callArgDeps,
                                                      const DependencyAnaliser::ActuralArgumentGetter& actualArgumentGetter)
{
    const FunctionAnaliser* FA = m_FAG(F);
    assert(FA != nullptr);
    for (auto& arg : F->getArgumentList()) {
        llvm::Value* actualArg = actualArgumentGetter(arg);
        llvm::Value* val = getFunctionOutArgumentValue(actualArg, arg);
        if (val == nullptr) {
            continue;
        }
        if (!FA->isOutArgInputDependent(&arg)) {
            updateValueDependencies(val, DepInfo(DepInfo::INPUT_INDEP));
            continue;
        }
        const auto& argDeps = FA->getOutArgDependencies(&arg);
        // argDeps may also have argument dependencies, but it is not important, if it also depends on new input.
        if (argDeps.isInputDep()) {
            updateValueDependencies(val, DepInfo(DepInfo::INPUT_DEP));
            continue;
        }
        auto argDependencies = getArgumentActualDependencies(argDeps.getArgumentDependencies(), callArgDeps);
        updateValueDependencies(val, argDependencies);
    }
}

void DependencyAnaliser::updateLibFunctionCallOutArgDependencies(llvm::Function* F,
                                                                 const ArgumentDependenciesMap& callArgDeps,
                                                                 const DependencyAnaliser::ActuralArgumentGetter& actualArgumentGetter)
{
    const auto& Fname = F->getName();
    auto& libInfo = LibraryInfoManager::get();
    if (!libInfo.hasLibFunctionInfo(Fname)) {
        updateInputDepLibFunctionCallOutArgDependencies(F, actualArgumentGetter);
        return;
    }
    libInfo.resolveLibFunctionInfo(F);
    const auto& libFInfo = libInfo.getLibFunctionInfo(Fname);
    assert(libFInfo.isResolved());
    for (auto& arg : F->getArgumentList()) {
        llvm::Value* actualArg = actualArgumentGetter(arg);
        llvm::Value* val = getFunctionOutArgumentValue(actualArg, arg);
        if (val == nullptr) {
            continue;
        }
        if (!libFInfo.hasResolvedArgument(&arg)) {
            continue;
        }
        const auto& libArgDeps = libFInfo.getResolvedArgumentDependencies(&arg);
        if (libArgDeps.isInputDep()) {
            updateValueDependencies(val, DepInfo(DepInfo::INPUT_DEP));
        } else if (libArgDeps.isInputIndep()) {
            updateValueDependencies(val, DepInfo(DepInfo::INPUT_INDEP));
        } else {
            auto argDependencies = getArgumentActualDependencies(libArgDeps.getArgumentDependencies(), callArgDeps);
            updateValueDependencies(val, argDependencies);
        }
    }
}

DepInfo DependencyAnaliser::getArgumentActualDependencies(const ArgumentSet& dependencies,
                                                          const ArgumentDependenciesMap& argDepInfo)
{
    DepInfo info(DepInfo::INPUT_INDEP);
    for (const auto& arg : dependencies) {
        auto pos = argDepInfo.find(arg);
        if (pos == argDepInfo.end()) {
            continue;
        }
        info.mergeDependencies(pos->second);
    }
    return info;
}

llvm::Value* DependencyAnaliser::getFunctionOutArgumentValue(llvm::Value* actualArg,
                                                             const llvm::Argument& arg)
{
    if (!arg.getType()->isPointerTy()) {
        return nullptr;
    }
    if (llvm::dyn_cast<llvm::GlobalVariable>(actualArg)) {
        return nullptr;
    }
    if (auto* argInstr = llvm::dyn_cast<llvm::Instruction>(actualArg)) {
        if (llvm::dyn_cast<llvm::CallInst>(argInstr)) {
            return nullptr;
        }
        if (llvm::dyn_cast<llvm::AllocaInst>(argInstr)) {
            return getMemoryValue(argInstr);
        }
        return getMemoryValue(argInstr->getOperand(0));
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
        // need more elaborate processing for globals
        return globalVal;
    }
    if (auto* bitcast = llvm::dyn_cast<llvm::BitCastInst>(instrOp)) {
        // creating array in a heap (new).
        // Operand 0 is a malloc call, which is marked as input dependent as calls external function.
        // return instruction for now
        return bitcast;
    }
    if (auto* constVal = llvm::dyn_cast<llvm::Constant>(instrOp)) {
        assert(false);
    }
    auto instr = llvm::dyn_cast<llvm::Instruction>(instrOp);
    if (!instr) {
        //???
        return instrOp;
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
    // Very useful assert, helps to find new value types which could be store instruction operand
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


