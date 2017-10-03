#pragma once

#include "BasicBlockAnalysisResult.h"
#include "ReflectingDependencyAnaliser.h"

namespace input_dependency {

class VirtualCallSiteAnalysisResult;
class IndirectCallSitesAnalysisResult;

class InputDependentBasicBlockAnaliser : public BasicBlockAnalysisResult
{
public:
    InputDependentBasicBlockAnaliser(llvm::Function* F,
                                       llvm::AAResults& AAR,
                                       const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                       const IndirectCallSitesAnalysisResult& indirectCallsInfo,
                                       const Arguments& inputs,
                                       const FunctionAnalysisGetter& Fgetter,
                                       llvm::BasicBlock* BB);

    InputDependentBasicBlockAnaliser(const InputDependentBasicBlockAnaliser&) = delete;
    InputDependentBasicBlockAnaliser(InputDependentBasicBlockAnaliser&& ) = delete;
    InputDependentBasicBlockAnaliser& operator =(const InputDependentBasicBlockAnaliser&) = delete;
    InputDependentBasicBlockAnaliser& operator =(InputDependentBasicBlockAnaliser&&) = delete;

public:
    bool isInputDependent(llvm::BasicBlock* block) const override
    {
        return true;
    }

    /// \name Implementation of DependencyAnaliser interface
    /// \{
protected:
    virtual void processReturnInstr(llvm::ReturnInst* retInst) override;
    virtual void processBranchInst(llvm::BranchInst* branchInst) override;
    virtual void processStoreInst(llvm::StoreInst* storeInst) override;
    virtual DepInfo getLoadInstrDependencies(llvm::LoadInst* instr) override;
    virtual DepInfo getInstructionDependencies(llvm::Instruction* instr) override;
    virtual ValueDepInfo getValueDependencies(llvm::Value* value) override;

    void updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info) override;
    void updateValueDependencies(llvm::Value* value, const DepInfo& info) override;
    void updateReturnValueDependencies(const DepInfo& info) override;
    /// \}
};

// dummy class just to have inherited from ReflectingDependencyAnaliser
class ReflectingInputDependentBasicBlockAnaliser : public InputDependentBasicBlockAnaliser
                                                 , public ReflectingDependencyAnaliser
{
public:
    ReflectingInputDependentBasicBlockAnaliser(llvm::Function* F,
                                               llvm::AAResults& AAR,
                                               const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                               const IndirectCallSitesAnalysisResult& indirectCallsInfo,
                                               const Arguments& inputs,
                                               const FunctionAnalysisGetter& Fgetter,
                                               llvm::BasicBlock* BB);
public:
    bool isInputDependent(llvm::BasicBlock* block) const override
    {
        return true;
    }

    void reflect(const DependencyAnaliser::ValueDependencies&, const DepInfo&) override
    {
    }

    bool isReflected() const override
    {
        return true;
    }
};

} // namespace input_dependency

