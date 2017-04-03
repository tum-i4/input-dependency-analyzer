#pragma once

#include "definitions.h"
#include "BasicBlockAnalysisResult.h"
#include "ReflectingDependencyAnaliser.h"

namespace input_dependency {

/**
* \class ReflectingBasicBlockAnaliser
* \brief Implements Reflection interface and dependency results reporting interface
**/
class ReflectingBasicBlockAnaliser : public BasicBlockAnalysisResult
                                   , public ReflectingDependencyAnaliser
{
public:
    ReflectingBasicBlockAnaliser(llvm::Function* F,
                                 llvm::AAResults& AAR,
                                 const Arguments& inputs,
                                 const FunctionAnalysisGetter& Fgetter,
                                 llvm::BasicBlock* BB);

    ReflectingBasicBlockAnaliser(const ReflectingBasicBlockAnaliser&) = delete;
    ReflectingBasicBlockAnaliser(ReflectingBasicBlockAnaliser&& ) = delete;
    ReflectingBasicBlockAnaliser& operator =(const ReflectingBasicBlockAnaliser&) = delete;
    ReflectingBasicBlockAnaliser& operator =(ReflectingBasicBlockAnaliser&&) = delete;

    virtual ~ReflectingBasicBlockAnaliser() = default;

public:
    //void dumpResults() const override; // delete later, will use parent's
    void reflect(const DependencyAnaliser::ValueDependencies& dependencies) override;
    bool isReflected() const override
    {
        return m_isReflected;
    }

    void setInitialValueDependencies(const DependencyAnalysisResult::InitialValueDpendencies& valueDependencies) override;
    DepInfo getInstructionDependencies(llvm::Instruction* instr) const override;


    /// \name Implementation of DependencyAnaliser interface
    /// \{
protected:
    DepInfo getInstructionDependencies(llvm::Instruction* instr) override;
    void updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info) override;
    void updateValueDependencies(llvm::Value* value, const DepInfo& info) override;
    void updateReturnValueDependencies(const DepInfo& info) override;

private:
    void processInstrForOutputArgs(llvm::Instruction* I) override;
    DepInfo getLoadInstrDependencies(llvm::LoadInst* instr) override;
    DepInfo determineInstructionDependenciesFromOperands(llvm::Instruction* instr) override;
    void updateFunctionCallInfo(llvm::CallInst* callInst) override;
    /// \}

private:
    void updateValueDependentInstructions(const DepInfo& info,
                                          llvm::Instruction* instr);

    void reflect(llvm::Value* value, const DepInfo& deps);
    void reflectOnValues(llvm::Value* value, const DepInfo& depInfo);
    void reflectOnInstructions(llvm::Value* value, const DepInfo& depInfo);
    void reflectOnOutArguments(llvm::Value* value, const DepInfo& depInfo);
    void reflectOnCalledFunctionArguments(llvm::Value* value, const DepInfo& depInfo);
    void reflectOnReturnValue(llvm::Value* value, const DepInfo& depInfo);
    bool reflectOnSingleValue(llvm::Value* value, DepInfo& valueDep);
    void reflectOnDepInfo(llvm::Value* value,
                          DepInfo& depInfoTo,
                          const DepInfo& depInfoFrom,
                          bool eraseAfterReflection = true);
    void resolveValueDependencies(const DependencyAnaliser::ValueDependencies& successorDependencies);
    DepInfo getValueFinalDependencies(llvm::Value* value);

private:
    std::unordered_map<llvm::Value*, InstrSet> m_valueDependentInstrs;
    std::unordered_map<llvm::Value*, ArgumentSet> m_valueDependentOutArguments; 
    using CallArgumentSet = std::unordered_map<llvm::CallInst*, ArgumentSet>;
    std::unordered_map<llvm::Value*, CallArgumentSet> m_valueDependentFunctionArguments;

    std::unordered_map<llvm::Instruction*, DepInfo> m_instructionValueDependencies;
    bool m_isReflected;
}; // class ReflectingBasicBlockAnaliser

} // namespace input_dependency

