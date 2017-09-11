#pragma once

#include "DependencyAnalysisResult.h"
#include "ReflectingBasicBlockAnaliser.h"

namespace input_dependency {

class VirtualCallSiteAnalysisResult;
class IndirectCallSitesAnalysisResult;

// TODO: can derive from NonDeterministicBasicBlockAnaliser.
class NonDeterministicReflectingBasicBlockAnaliser : public ReflectingBasicBlockAnaliser
{
public:
    NonDeterministicReflectingBasicBlockAnaliser(llvm::Function* F,
                                                llvm::AAResults& AAR,
                                                const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                                const IndirectCallSitesAnalysisResult& indirectCallsInfo,
                                                const Arguments& inputs,
                                                const FunctionAnalysisGetter& Fgetter,
                                                llvm::BasicBlock* BB,
                                                const DepInfo& nonDetDeps);

    NonDeterministicReflectingBasicBlockAnaliser(const NonDeterministicReflectingBasicBlockAnaliser&) = delete;
    NonDeterministicReflectingBasicBlockAnaliser(NonDeterministicReflectingBasicBlockAnaliser&& ) = delete;
    NonDeterministicReflectingBasicBlockAnaliser& operator =(const NonDeterministicReflectingBasicBlockAnaliser&) = delete;
    NonDeterministicReflectingBasicBlockAnaliser& operator =(NonDeterministicReflectingBasicBlockAnaliser&&) = delete;

public:
    void finalizeResults(const ArgumentDependenciesMap& dependentArgs) override;

public:
    DepInfo getInstructionDependencies(llvm::Instruction* instr) override;
    DepInfo getValueDependencies(llvm::Value* value) override;
    void updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info) override;
    void updateValueDependencies(llvm::Value* value, const DepInfo& info) override;
    void updateReturnValueDependencies(const DepInfo& info) override;
    void setInitialValueDependencies(const DependencyAnaliser::ValueDependencies& valueDependencies) override;
    DepInfo getArgumentValueDependecnies(llvm::Value* argVal) override;

private:
    DepInfo addOnDependencyInfo(const DepInfo& info);

private:
    DepInfo m_nonDeterministicDeps;
    bool m_is_final_inputDep;
}; // class NonDeterministiReflectingBasicBlockAnaliser
} // namespace input_dependency

