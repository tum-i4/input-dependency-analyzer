#pragma once

#include "BasicBlockAnalysisResult.h"

namespace input_dependency {

class VirtualCallSiteAnalysisResult;
class IndirectCallSitesAnalysisResult;

class NonDeterministicBasicBlockAnaliser : public BasicBlockAnalysisResult
{
public:
    NonDeterministicBasicBlockAnaliser(llvm::Function* F,
                                       llvm::AAResults& AAR,
                                       const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                       const IndirectCallSitesAnalysisResult& indirectCallsInfo,
                                       const Arguments& inputs,
                                       const FunctionAnalysisGetter& Fgetter,
                                       llvm::BasicBlock* BB,
                                       const DepInfo& nonDetDeps);

    NonDeterministicBasicBlockAnaliser(const NonDeterministicBasicBlockAnaliser&) = delete;
    NonDeterministicBasicBlockAnaliser(NonDeterministicBasicBlockAnaliser&& ) = delete;
    NonDeterministicBasicBlockAnaliser& operator =(const NonDeterministicBasicBlockAnaliser&) = delete;
    NonDeterministicBasicBlockAnaliser& operator =(NonDeterministicBasicBlockAnaliser&&) = delete;

public:
    DepInfo getBlockDependencies() const override;

    void finalizeResults(const ArgumentDependenciesMap& dependentArgs) override;
    bool isInputDependent(llvm::BasicBlock* block, const DependencyAnaliser::ArgumentDependenciesMap& depArgs) const override;

    /// \name Implementation of DependencyAnaliser interface
    /// \{
protected:
    DepInfo getInstructionDependencies(llvm::Instruction* instr) override;
    ValueDepInfo getValueDependencies(llvm::Value* value) override;
    ValueDepInfo getCompositeValueDependencies(llvm::Value* value, llvm::Instruction* element_instr) override;
    void updateValueDependencies(llvm::Value* value, const DepInfo& info, bool update_aliases) override;
    void updateValueDependencies(llvm::Value* value, const ValueDepInfo& info, bool update_aliases) override;
    void updateCompositeValueDependencies(llvm::Value* value,
                                          llvm::Instruction* elInstr,
                                          const ValueDepInfo& info) override;
    void updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info) override;
    void updateReturnValueDependencies(const ValueDepInfo& info) override;
    void setInitialValueDependencies(const DependencyAnaliser::ValueDependencies& valueDependencies) override;
    ValueDepInfo getArgumentValueDependecnies(llvm::Value* argVal) override;
    /// \}

private:
    DepInfo addOnDependencyInfo(const DepInfo& info);
    ValueDepInfo addOnDependencyInfo(const ValueDepInfo& info);

private:
    DepInfo m_nonDetDeps;
};

} // namespace input_dependency

