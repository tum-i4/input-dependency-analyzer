#pragma once

#include "input-dependency/Analysis/BasicBlockAnalysisResult.h"
#include "input-dependency/Analysis/ReflectingDependencyAnaliser.h"

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

    bool isInputDependent(llvm::BasicBlock* block, const ArgumentDependenciesMap& depArgs) const override
    {
        return true;
    }
    bool isInputDependent(llvm::Instruction* instr) const override
    {
        return true;
    }

    bool isInputDependent(llvm::Instruction* instr, const ArgumentDependenciesMap& depArgs) const override
    {
        return true;
    }

    bool isInputIndependent(llvm::Instruction* instr) const override
    {
        return false;
    }

    bool isInputIndependent(llvm::Instruction* instr, const ArgumentDependenciesMap& depArgs) const override
    {
        return false;
    }

    long unsigned get_input_dep_blocks_count() const  override
    {
        return 1;
    }

    long unsigned get_input_indep_blocks_count() const  override
    {
        return 0;
    }

    long unsigned get_input_dep_count() const  override;

    long unsigned get_input_indep_count() const  override
    {
        return 0;
    }

    /// \name Implementation of DependencyAnaliser interface
    /// \{
protected:
    void processReturnInstr(llvm::ReturnInst* retInst) override;
    void processBranchInst(llvm::BranchInst* branchInst) override;
    void processStoreInst(llvm::StoreInst* storeInst) override;
    DepInfo getLoadInstrDependencies(llvm::LoadInst* instr) override;
    void addControlDependencies(ValueDepInfo& valueDepInfo) override;
    void addControlDependencies(DepInfo& depInfo) override;
    DepInfo getInstructionDependencies(llvm::Instruction* instr) override;
    ValueDepInfo getValueDependencies(llvm::Value* value) override;
    ValueDepInfo getCompositeValueDependencies(llvm::Value* value, llvm::Instruction* element_instr) override;

    void updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info) override;
    void updateValueDependencies(llvm::Value* value, const DepInfo& info, bool update_aliases, int arg_idx = -1) override;
    void updateValueDependencies(llvm::Value* value, const ValueDepInfo& info, bool update_aliases, int arg_idx = -1) override;
    void updateReturnValueDependencies(const ValueDepInfo& info) override;
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

    bool isInputDependent(llvm::BasicBlock* block, const ArgumentDependenciesMap& depArgs) const override
    {
        return true;
    }
    bool isInputDependent(llvm::Instruction* instr) const override
    {
        return true;
    }

    bool isInputDependent(llvm::Instruction* instr, const ArgumentDependenciesMap& depArgs) const override
    {
        return true;
    }

    bool isInputIndependent(llvm::Instruction* instr) const override
    {
        return false;
    }

    bool isInputIndependent(llvm::Instruction* instr, const ArgumentDependenciesMap& depArgs) const override
    {
        return false;
    }

   long unsigned get_input_dep_blocks_count() const  override
    {
        return 1;
    }

    long unsigned get_input_indep_blocks_count() const  override
    {
        return 0;
    }

    long unsigned get_input_dep_count() const  override;

    long unsigned get_input_indep_count() const  override
    {
        return 0;
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

