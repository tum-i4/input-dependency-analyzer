#pragma once

#include "definitions.h"
#include "DependencyInfo.h"

namespace input_dependency {

/**
* \class DependencyAnaliser
* Interface for providing dependency analysis information.
**/
class DependencyAnaliser
{
public:
    typedef std::unordered_map<llvm::Value*, DepInfo> ValueDependencies;
    typedef std::unordered_map<llvm::Argument*, DepInfo> ArgumentDependenciesMap;
    typedef std::unordered_map<llvm::Function*, ArgumentDependenciesMap> FunctionArgumentsDependencies;

public:
    DependencyAnaliser(llvm::Function* F,
                       llvm::AAResults& AAR,
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
    virtual void analize() = 0;
    virtual void finalize(const ArgumentDependenciesMap& dependentArgs);
    virtual void dump() const;
    /// \}

    /// \name Abstract protected interface for porcessing instructions
    /// \{
protected:
    virtual void processInstruction(llvm::Instruction* inst);
    virtual void processReturnInstr(llvm::ReturnInst* retInst);
    virtual void processBranchInst(llvm::BranchInst* branchInst);
    virtual void processStoreInst(llvm::StoreInst* storeInst);
    virtual void processCallInst(llvm::CallInst* callInst);
    virtual void processInstrForOutputArgs(llvm::Instruction* I);
    
    virtual DepInfo getInstructionDependencies(llvm::Instruction* instr) = 0;
    virtual DepInfo getValueDependencies(llvm::Value* value) = 0;
    virtual DepInfo getLoadInstrDependencies(llvm::LoadInst* instr) = 0;
    virtual DepInfo determineInstructionDependenciesFromOperands(llvm::Instruction* instr) = 0;
    virtual void updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info) = 0;
    virtual void updateValueDependencies(llvm::Value* value, const DepInfo& info) = 0;
    virtual void updateReturnValueDependencies(const DepInfo& info) = 0;

    virtual void updateFunctionCallInfo(llvm::CallInst* callInst, bool isExternalF);
    /// \}

protected:
    llvm::Argument* isInput(llvm::Value* val) const;
    // TODO: see if need to be virtual. looks like this might need to be virtual
    void updateCallOutArgDependencies(llvm::CallInst* callInst, bool isExternalF);
    void updateCallInstructionDependencies(llvm::CallInst* callInst);

protected:
    // TODO: see if no derived is using this, move to source file
    static DepInfo getArgumentActualDependencies(const ArgumentSet& dependencies,
                                                 const ArgumentDependenciesMap& argDepInfo);
    static llvm::Value* getFunctionOutArgumentValue(const llvm::CallInst* callInst,
                                                    const llvm::Argument& arg);
    static llvm::Value* getMemoryValue(llvm::Value* instrOp);

protected:
    llvm::Function* m_F;
    const Arguments& m_inputs;
    const FunctionAnalysisGetter& m_FAG;
    llvm::AAResults& m_AAR;
    bool m_finalized;

    ArgumentDependenciesMap m_outArgDependencies;
    DepInfo m_returnValueDependencies;
    FunctionArgumentsDependencies m_calledFunctionsInfo;
    InstrSet m_inputIndependentInstrs; // for debug purposes only
    InstrDependencyMap m_inputDependentInstrs;
    InstrSet m_finalInputDependentInstrs;
    ValueDependencies m_valueDependencies;
}; // class DependencyAnaliser

} // namespace input_dependency

