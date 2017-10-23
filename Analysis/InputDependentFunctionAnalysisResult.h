#pragma once

#include "InputDependencyResult.h"

#include "llvm/IR/Function.h"

namespace input_dependency {

class InputDependentFunctionAnalysisResult : public InputDependencyResult
{
public:
    InputDependentFunctionAnalysisResult(llvm::Function* F)
        : m_F(F)
    {
    }

public:
    llvm::Function* getFunction() override
    {
        return m_F;
    }

    const llvm::Function* getFunction() const override
    {
        return m_F;
    }

    bool isInputDependent(llvm::Instruction* instr) const override
    {
        return true;
    }

    bool isInputDependent(const llvm::Instruction* instr) const override
    {
        return true;
    }

    bool isInputIndependent(llvm::Instruction* instr) const override
    {
        return false;
    }

    bool isInputIndependent(const llvm::Instruction* instr) const override
    {
        return false;
    }

    bool isInputDependentBlock(llvm::BasicBlock* block) const override
    {
        // TODO: what about entry and exit blocks?
        return true;
    }

    FunctionSet getCallSitesData() const override
    {
        return FunctionSet();    
    }

    FunctionCallDepInfo getFunctionCallDepInfo(llvm::Function* F) const override
    {
    }

    InputDependentFunctionAnalysisResult* toInputDependentFunctionAnalysisResult() override
    {
        return this;
    }

    // for debug only
    virtual long unsigned get_input_dep_count() const override
    {
        return m_F->getBasicBlockList().size();
    }

    virtual long unsigned get_input_indep_count() const override
    {
        return 0;
    }

    virtual long unsigned get_input_unknowns_count() const override 
    {
        return 0;
    }

private:
    llvm::Function* m_F;
}; // class InputDependentFunctionAnalysisResult

} // namespace input_dependency

