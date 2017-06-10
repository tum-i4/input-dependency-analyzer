#pragma once

#include "BasicBlockAnalysisResult.h"
#include "ReflectingDependencyAnaliser.h"

namespace input_dependency {

class VirtualCallSiteAnalysisResult;

class InputDependentBasicBlockAnaliser : public BasicBlockAnalysisResult
{
public:
    InputDependentBasicBlockAnaliser(llvm::Function* F,
                                       llvm::AAResults& AAR,
                                       const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                       const Arguments& inputs,
                                       const FunctionAnalysisGetter& Fgetter,
                                       llvm::BasicBlock* BB);

    InputDependentBasicBlockAnaliser(const InputDependentBasicBlockAnaliser&) = delete;
    InputDependentBasicBlockAnaliser(InputDependentBasicBlockAnaliser&& ) = delete;
    InputDependentBasicBlockAnaliser& operator =(const InputDependentBasicBlockAnaliser&) = delete;
    InputDependentBasicBlockAnaliser& operator =(InputDependentBasicBlockAnaliser&&) = delete;

    /// \name Implementation of DependencyAnaliser interface
    /// \{
protected:
    virtual void processReturnInstr(llvm::ReturnInst* retInst) override;
    virtual void processBranchInst(llvm::BranchInst* branchInst) override;
    virtual void processStoreInst(llvm::StoreInst* storeInst) override;
    virtual DepInfo getLoadInstrDependencies(llvm::LoadInst* instr) override;
    virtual DepInfo getInstructionDependencies(llvm::Instruction* instr) override;
    virtual DepInfo getValueDependencies(llvm::Value* value) override;
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
                                               const Arguments& inputs,
                                               const FunctionAnalysisGetter& Fgetter,
                                               llvm::BasicBlock* BB);
public:
    void reflect(const DependencyAnaliser::ValueDependencies&, const DepInfo&) override
    {
    }

    bool isReflected() const override
    {
        return true;
    }
};

} // namespace input_dependency

