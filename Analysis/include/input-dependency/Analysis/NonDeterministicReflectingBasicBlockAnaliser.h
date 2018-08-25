#pragma once

#include "input-dependency/Analysis/DependencyAnalysisResult.h"
#include "input-dependency/Analysis/ReflectingBasicBlockAnaliser.h"

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
    DepInfo getBlockDependencies() const override;
    void finalizeResults(const ArgumentDependenciesMap& dependentArgs) override;
    void finalizeGlobals(const GlobalVariableDependencyMap& globalsDeps) override;
    bool isInputDependent(llvm::BasicBlock* block, const DependencyAnaliser::ArgumentDependenciesMap& depArgs) const override;
    bool isDataDependent(llvm::Instruction* I) const override;
    bool isDataDependent(llvm::Instruction* I, const ArgumentDependenciesMap& depArgs) const override;
    bool isArgumentDependent(llvm::BasicBlock* block) const override;

    void reflect(const DependencyAnaliser::ValueDependencies& dependencies,
                 const DepInfo& mandatory_deps) override;
public:
    void addControlDependencies(ValueDepInfo& valueDepInfo) override;
    void addControlDependencies(DepInfo& depInfo) override;
    DepInfo getInstructionDependencies(llvm::Instruction* instr) override;
    ValueDepInfo getValueDependencies(llvm::Value* value) override;
    void updateValueDependencies(llvm::Value* value, const DepInfo& info, bool update_aliases, int arg_idx = -1) override;
    void updateValueDependencies(llvm::Value* value, const ValueDepInfo& info, bool update_aliases, int arg_idx = -1) override;
    void updateCompositeValueDependencies(llvm::Value* value,
                                          llvm::Instruction* elInstr,
                                          const ValueDepInfo& info) override;
    void updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info) override;
    void updateReturnValueDependencies(const ValueDepInfo& info) override;
    ValueDepInfo getArgumentValueDependecnies(llvm::Value* argVal) override;

private:
    DepInfo addOnDependencyInfo(const DepInfo& info);
    ValueDepInfo addOnDependencyInfo(const ValueDepInfo& info);

private:
    DepInfo m_nonDeterministicDeps;
    InstrDependencyMap m_instructions;
    InstrSet m_dataDependentInstrs;
    ValueDependencies m_valueDataDependencies;
}; // class NonDeterministiReflectingBasicBlockAnaliser
} // namespace input_dependency

