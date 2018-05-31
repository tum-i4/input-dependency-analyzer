#pragma once

#include "definitions.h"
#include "DependencyInfo.h"
#include "ValueDepInfo.h"
#include "FunctionCallDepInfo.h"

namespace llvm {
class FunctionType;
}

namespace input_dependency {

class VirtualCallSiteAnalysisResult;
class IndirectCallSitesAnalysisResult;

/**
* \class DependencyAnaliser
* Interface for providing dependency analysis information.
**/
class DependencyAnaliser
{
public:
    using ValueDependencies = std::unordered_map<llvm::Value*, ValueDepInfo>;
    using ArgumentDependenciesMap = FunctionCallDepInfo::ArgumentDependenciesMap;
    using GlobalVariableDependencyMap = FunctionCallDepInfo::GlobalVariableDependencyMap;
    using FunctionCallsArgumentDependencies = std::unordered_map<llvm::Function*, FunctionCallDepInfo>;
    using InstrDependencyMap = std::unordered_map<llvm::Instruction*, DepInfo>;
    using ValueCallbackMap = std::unordered_map<llvm::Value*, FunctionSet>;

public:
    DependencyAnaliser(llvm::Function* F,
                       llvm::AAResults& AAR,
                       const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                       const IndirectCallSitesAnalysisResult& indirectCallsInfo,
                       const Arguments& inputs,
                       const FunctionAnalysisGetter& Fgetter);

    DependencyAnaliser(const DependencyAnaliser&) = delete;
    DependencyAnaliser(DependencyAnaliser&& ) = delete;
    DependencyAnaliser& operator =(const DependencyAnaliser&) = delete;
    DependencyAnaliser& operator =(DependencyAnaliser&&) = delete;

    virtual ~DependencyAnaliser() = default;

    /// \name Interface to start analysis
    /// \{
public:
    virtual void analyze() = 0;
    virtual void finalize(const ArgumentDependenciesMap& dependentArgs);
    virtual void finalize(const GlobalVariableDependencyMap& globalDeps);
    virtual void dump() const;
    /// \}

    /// \name Abstract protected interface for porcessing instructions
    /// \{
protected:
    virtual void processInstruction(llvm::Instruction* inst);
    virtual void processPhiNode(llvm::PHINode* phi);
    virtual void processBitCast(llvm::BitCastInst* bitcast);
    virtual void processGetElementPtrInst(llvm::GetElementPtrInst* getElPtr);
    virtual void processReturnInstr(llvm::ReturnInst* retInst);
    virtual void processBranchInst(llvm::BranchInst* branchInst);
    virtual void processStoreInst(llvm::StoreInst* storeInst);
    virtual void processCallInst(llvm::CallInst* callInst);
    virtual void processInvokeInst(llvm::InvokeInst* invokeInst);
    
    virtual void addControlDependencies(ValueDepInfo& valueDepInfo) = 0;
    virtual void addControlDependencies(DepInfo& depInfo) = 0;
    virtual DepInfo getInstructionDependencies(llvm::Instruction* instr) = 0;
    virtual ValueDepInfo getValueDependencies(llvm::Value* value) = 0;
    virtual ValueDepInfo getCompositeValueDependencies(llvm::Value* value, llvm::Instruction* element_instr) = 0;
    virtual DepInfo getLoadInstrDependencies(llvm::LoadInst* instr) = 0;
    virtual DepInfo determineInstructionDependenciesFromOperands(llvm::Instruction* instr) = 0;
    virtual void updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info) = 0;
    virtual void updateValueDependencies(llvm::Value* value, const DepInfo& info, bool update_aliases, int arg_idx = -1) = 0;
    virtual void updateValueDependencies(llvm::Value* value, const ValueDepInfo& info, bool update_aliases, int arg_idx = -1) = 0;
    virtual void updateCompositeValueDependencies(llvm::Value* value, llvm::Instruction* elInstr, const ValueDepInfo& info) = 0;
    virtual void updateReturnValueDependencies(const ValueDepInfo& info) = 0;
    virtual ValueDepInfo getRefInfo(llvm::Instruction* instr) = 0;
    virtual void updateAliasesDependencies(llvm::Value* val, const ValueDepInfo& info, ValueDependencies& valueDependencies) = 0;
    virtual void updateAliasesDependencies(llvm::Value* val, llvm::Instruction* elInstr, const ValueDepInfo& info, ValueDependencies& valueDependencies) = 0;
    virtual void updateAliasingOutArgDependencies(llvm::Value* val, const ValueDepInfo& info, int arg_idx = -1) = 0;
    virtual void updateModAliasesDependencies(llvm::StoreInst* storeInst, const ValueDepInfo& info) = 0;
    virtual void updateRefAliasesDependencies(llvm::Instruction* instr, const ValueDepInfo& info) = 0;
    virtual void markCallbackFunctionsForValue(llvm::Value* value) = 0;
    virtual void removeCallbackFunctionsForValue(llvm::Value* value) = 0;

    virtual ValueDepInfo getArgumentValueDependecnies(llvm::Value* argVal);
    virtual void updateFunctionCallSiteInfo(llvm::CallInst* callInst, llvm::Function* F);
    virtual void updateFunctionInvokeSiteInfo(llvm::InvokeInst* invokeInst, llvm::Function* F);
    /// \}

protected:
    ArgumentSet isInput(llvm::Value* val) const;

    void processCallSiteWithMultipleTargets(llvm::CallInst* callInst, const FunctionSet& targets);
    void processInvokeSiteWithMultipleTargets(llvm::InvokeInst* invokeInst, const FunctionSet& targets);
    void updateCallSiteOutArgDependencies(llvm::CallInst* callInst, llvm::Function* F);
    void updateInvokeSiteOutArgDependencies(llvm::InvokeInst* invokeInst, llvm::Function* F);
    void updateCallInstructionDependencies(llvm::CallInst* callInst, llvm::Function* F);
    void updateInvokeInstructionDependencies(llvm::InvokeInst* invokeInst, llvm::Function* F);
    void updateGlobalsAfterFunctionCall(llvm::CallInst* callInst, llvm::Function* F);
    void updateGlobalsAfterFunctionInvoke(llvm::InvokeInst* invokeInst, llvm::Function* F);
    void updateGlobalsAfterFunctionExecution(llvm::Function* F,
                                             const ArgumentDependenciesMap& functionArgDeps,
                                             bool is_recursive);
    void updateCallInputDependentOutArgDependencies(llvm::CallInst* callInst);
    void updateInvokeInputDependentOutArgDependencies(llvm::InvokeInst* invokeInst);

    void updateLibFunctionCallInstOutArgDependencies(llvm::CallInst* callInst, const ArgumentDependenciesMap& argDepMap);
    void updateLibFunctionInvokeInstOutArgDependencies(llvm::InvokeInst* callInst, const ArgumentDependenciesMap& argDepMap);
    void updateLibFunctionCallInstructionDependencies(llvm::CallInst* callInst, const ArgumentDependenciesMap& argDepMap);
    void updateLibFunctionInvokeInstructionDependencies(llvm::InvokeInst* invokeInst, const ArgumentDependenciesMap& argDepMap);

private:
    void updateDependencyForGetElementPtr(llvm::GetElementPtrInst* getElPtr, const ValueDepInfo& info);

    //TODO: make const
    ArgumentDependenciesMap gatherFunctionCallSiteInfo(llvm::CallInst* callInst, llvm::Function* F);
    ArgumentDependenciesMap gatherFunctionInvokeSiteInfo(llvm::InvokeInst* invokeInst, llvm::Function* F);
    GlobalVariableDependencyMap gatherGlobalsForFunctionCall(llvm::Function* F);

    using ArgumentValueGetter = std::function<llvm::Value* (unsigned formalArgNo)>;
    void updateCallOutArgDependencies(llvm::Function* F,
                                      const ArgumentDependenciesMap& callArgDeps,
                                      const ArgumentValueGetter& actualArgumentGetter);
    void updateLibFunctionCallOutArgDependencies(llvm::Function* F,
                                                 const ArgumentDependenciesMap& callArgDeps,
                                                 const ArgumentValueGetter& actualArgumentGetter);
    void updateInputDepLibFunctionCallOutArgDependencies(llvm::Function* F,
                                                         const DependencyAnaliser::ArgumentValueGetter& actualArgumentGetter);
    using ArgumentValueGetterByIndex = std::function<llvm::Value* (unsigned index)>;
    void updateFunctionInputDepOutArgDependencies(llvm::FunctionType* FType,
                                                  const ArgumentValueGetterByIndex& actualArgumentGetter);
    void updateOutArgumentDependencies(llvm::Value* val, const ValueDepInfo& depInfo);

    ValueDepInfo getArgumentActualValueDependencies(const ValueSet& valueDeps);
    void resolveReturnedValueDependencies(ValueDepInfo& valueDeps, const ArgumentDependenciesMap& argDepInfo);

    void finalizeValues(const GlobalVariableDependencyMap& globalDeps);
    void finalizeInstructions(const GlobalVariableDependencyMap& globalDeps);

protected:
    static ValueDepInfo getArgumentActualDependencies(const ArgumentSet& dependencies,
                                                 const ArgumentDependenciesMap& argDepInfo);
    static llvm::Value* getFunctionOutArgumentValue(llvm::Value* actualArg);
    static llvm::Value* getMemoryValue(llvm::Value* instrOp);

    void finalizeInstructions(const GlobalVariableDependencyMap& globalDeps, InstrDependencyMap& instructions);
    void finalizeValueDependencies(const GlobalVariableDependencyMap& globalDeps, DepInfo& toFinalize);
    DepInfo getFinalizedDepInfo(const ValueSet& values, const GlobalVariableDependencyMap& globalDeps);

protected:
    llvm::Function* m_F;
    const Arguments& m_inputs;
    const FunctionAnalysisGetter& m_FAG;
    llvm::AAResults& m_AAR;
    const VirtualCallSiteAnalysisResult& m_virtualCallsInfo;
    const IndirectCallSitesAnalysisResult& m_indirectCallsInfo;
    bool m_finalized;
    bool m_globalsFinalized;

    ValueCallbackMap m_functionValues;
    ArgumentDependenciesMap m_outArgDependencies;
    ValueDepInfo m_returnValueDependencies;
    FunctionSet m_calledFunctions;
    FunctionCallsArgumentDependencies m_functionCallInfo;
    InstrSet m_inputIndependentInstrs;
    InstrDependencyMap m_inputDependentInstrs;
    InstrSet m_finalInputDependentInstrs;
    ValueDependencies m_valueDependencies;
    ValueDependencies m_initialDependencies;
    GlobalsSet m_referencedGlobals;
    GlobalsSet m_modifiedGlobals;
}; // class DependencyAnaliser

} // namespace input_dependency

