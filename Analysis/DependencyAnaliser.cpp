#include "DependencyAnaliser.h"

#include "Utils.h"
#include "FunctionAnaliser.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Argument.h"
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
    return &*it;
}

} // unnamed namespace

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
        if (item.second.empty() || Utils::haveIntersection(dependentArgs, item.second)) {
            m_finalInputDependentInstrs.insert(item.first);
        } else {
            m_inputIndependentInstrs.insert(item.first);
        }
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
        for (auto& arg : item.second) {
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
    } else if (m_returnValueDependencies.isInputDep()
                    && m_returnValueDependencies.getArgumentDependencies().empty()) {
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
    auto storedValue = getMemoryValue(storeInst->getPointerOperand());
    assert(storedValue);
    auto op = storeInst->getOperand(0);
    if (auto* constOp = llvm::dyn_cast<llvm::Constant>(op)) {
        updateInstructionDependencies(storeInst, DepInfo(DepInfo::INPUT_INDEP));
        updateValueDependencies(storedValue, DepInfo(DepInfo::INPUT_INDEP));
        return;
    }
    llvm::Argument* arg = isInput(op);
    if (arg != nullptr) {
        updateInstructionDependencies(storeInst, DepInfo(DepInfo::INPUT_DEP, ArgumentSet{arg}));
        updateValueDependencies(storedValue, DepInfo(DepInfo::INPUT_DEP, ArgumentSet{arg}));
        return;
    }
    auto* opInstr = llvm::dyn_cast<llvm::Instruction>(op);
    assert(opInstr);
    // Store instruction depends only on the value which is being stored.
    const auto& deps = getInstructionDependencies(opInstr);
    updateInstructionDependencies(storeInst, deps);
    updateValueDependencies(storedValue, deps);
}

void DependencyAnaliser::processCallInst(llvm::CallInst* callInst)
{
    llvm::Function* F = callInst->getCalledFunction();
    if (F == nullptr) {
        return;
    }
    llvm::dbgs() << "Function call " << F->getName() << "\n";
    const bool isExternal = (F->getParent() != m_F->getParent()
                                || F->isDeclaration()
                                || F->getLinkage() == llvm::GlobalValue::LinkOnceODRLinkage);
    if (!isExternal) {
        m_calledFunctionsInfo[F];
    }
    updateFunctionCallInfo(callInst, isExternal);
    updateCallOutArgDependencies(callInst, isExternal);
    updateCallInstructionDependencies(callInst);
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
            const auto& dependencies = pos->second;
            item->second.setDependency(DepInfo::INPUT_DEP);
            item->second.setArgumentDependencies(dependencies);
        } else {
            // making output input independent
            item->second = DepInfo(DepInfo::INPUT_INDEP);
        }
        ++item;
    }
}
    
// For external function calls just indicate if call instruction is dependent or not
// indicates the arguments of the called function which are input dependent in this call site.
// TODO: what will happen if collect external functions call info too. 
// It won't break anything, the worst thing would be collecting uneccessary information
void DependencyAnaliser::updateFunctionCallInfo(llvm::CallInst* callInst, bool isExternalF)
{
    llvm::Function* F = callInst->getCalledFunction();
    //llvm::dbgs() << "Called function " << F->getName() << "\n";
    for (unsigned i = 0; i < callInst->getNumArgOperands(); ++i) {
        llvm::Value* argVal = callInst->getArgOperand(i);
        if (auto constVal = llvm::dyn_cast<llvm::Constant>(argVal)) {
            continue;
        }
        DepInfo deps;
        auto pos = m_valueDependencies.find(argVal);
        if (pos != m_valueDependencies.end()) {
            //if (pos->second.dependency == DepInfo::INPUT_DEP) {
                deps = pos->second;
            //}
        } else if (auto* arg = isInput(argVal)) {
            deps = DepInfo(DepInfo::INPUT_DEP, ArgumentSet{arg});
        } else if (auto* argInst = llvm::dyn_cast<llvm::Instruction>(argVal)) {
            deps = getInstructionDependencies(argInst);
        }
        // Note: this check should not be here
        if (deps.isInputIndep() || isExternalF) {
            continue;
        }
        auto arg = getFunctionArgument(F, i);
        auto& item = m_calledFunctionsInfo[F][arg];
        item.mergeDependencies(deps);
    }
}

// For external functions do worst case assumption, which is:
//    Assume pointer (reference) arguments are becoming input dependent
// Note: after adding configuration files for external library calls this information would be infered more precisely.
void DependencyAnaliser::updateCallOutArgDependencies(llvm::CallInst* callInst, bool isExternalF)
{
    auto F = callInst->getCalledFunction();
    const FunctionAnaliser* FA = m_FAG(F);
    for (auto& arg : F->getArgumentList()) {
        // TODO: find a way to identify constant pointers
        llvm::Value* val = getFunctionOutArgumentValue(callInst, arg);
        if (val == nullptr) {
            continue;
        }
        if (isExternalF) {
            updateValueDependencies(val, DepInfo(DepInfo::INPUT_DEP));
            continue;
        }
        assert(FA);
        if (!FA->isOutArgInputDependent(&arg)) {
            updateValueDependencies(val, DepInfo(DepInfo::INPUT_INDEP));
            continue;
        }
        const auto& argDeps = FA->getOutArgDependencies(&arg);
        if (argDeps.empty()) {
            updateValueDependencies(val, DepInfo(DepInfo::INPUT_DEP));
            continue;
        }
        auto argDependencies = getArgumentActualDependencies(argDeps,
                                                             m_calledFunctionsInfo[F]);
        updateValueDependencies(val, argDependencies);
    }
}

void DependencyAnaliser::updateCallInstructionDependencies(llvm::CallInst* callInst)
{
    auto F = callInst->getCalledFunction();
    const bool isExternal = (F->getParent() != m_F->getParent()
                                || F->isDeclaration()
                                || F->getLinkage() == llvm::GlobalValue::LinkOnceODRLinkage);
    if (F->doesNotReturn()) {
        updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_INDEP));
        return;
    } else if (isExternal) {
        //Assume return value is input dependent
        updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_DEP));
        return;
    }
    const FunctionAnaliser* FA = m_FAG(F);
    assert(FA);
    if (!FA->isReturnValueInputDependent()) {
        updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_INDEP));
    } else {
        const auto& retDeps = FA->getRetValueDependencies();
        if (retDeps.empty()) {
            // is input dependent, but not dependent on arguments
            updateInstructionDependencies(callInst, DepInfo(DepInfo::INPUT_DEP));
            return;
        }
        auto dependencies = getArgumentActualDependencies(retDeps,
                                                          m_calledFunctionsInfo[F]);
        updateInstructionDependencies(callInst, dependencies);
    }
}

llvm::Argument* DependencyAnaliser::isInput(llvm::Value* val) const
{
    for (auto& arg : m_inputs) {
        if (auto* argVal = llvm::dyn_cast<llvm::Value>(arg)) {
            if (argVal == val || m_AAR.alias(argVal, val)) {
                return arg;
            }
        }
    }
    return nullptr;
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
        // Trust argDepInfo collector. If argument is in there means it depends on input.
        // if dependent args is empty, depends on new input
        //info.dependency = DepInfo::INPUT_DEP;
        info.mergeDependencies(pos->second);
    }
    return info;
}

llvm::Value* DependencyAnaliser::getFunctionOutArgumentValue(const llvm::CallInst* callInst,
                                                             const llvm::Argument& arg)
{
    if (!arg.getType()->isPointerTy()) {
        return nullptr;
    }
    llvm::Value* val = callInst->getArgOperand(arg.getArgNo());
    if (llvm::dyn_cast<llvm::GlobalVariable>(val)) {
        return nullptr;
    }
    if (auto* argInstr = llvm::dyn_cast<llvm::Instruction>(val)) {
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


