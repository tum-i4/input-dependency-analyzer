#pragma once

#include "BasicBlockAnalysisResult.h"

namespace input_dependency {

class VirtualCallSiteAnalysisResult;

class NonDeterministicBasicBlockAnaliser : public BasicBlockAnalysisResult
{
public:
    NonDeterministicBasicBlockAnaliser(llvm::Function* F,
                                       llvm::AAResults& AAR,
                                       const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                       const Arguments& inputs,
                                       const FunctionAnalysisGetter& Fgetter,
                                       llvm::BasicBlock* BB,
                                       const DepInfo& nonDetDeps);

    NonDeterministicBasicBlockAnaliser(const NonDeterministicBasicBlockAnaliser&) = delete;
    NonDeterministicBasicBlockAnaliser(NonDeterministicBasicBlockAnaliser&& ) = delete;
    NonDeterministicBasicBlockAnaliser& operator =(const NonDeterministicBasicBlockAnaliser&) = delete;
    NonDeterministicBasicBlockAnaliser& operator =(NonDeterministicBasicBlockAnaliser&&) = delete;

    /// \name Implementation of DependencyAnaliser interface
    /// \{
protected:
    DepInfo getInstructionDependencies(llvm::Instruction* instr) override;
    DepInfo getValueDependencies(llvm::Value* value) override;
    void updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info) override;
    void updateValueDependencies(llvm::Value* value, const DepInfo& info) override;
    void updateReturnValueDependencies(const DepInfo& info) override;
    /// \}

private:
    DepInfo addOnDependencyInfo(const DepInfo& info);

private:
    DepInfo m_nonDetDeps;
};

} // namespace input_dependency

