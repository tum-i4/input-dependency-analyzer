#pragma once

#include "input-dependency/Analysis/BasicBlockAnalysisResult.h"
#include "input-dependency/Analysis/ReflectingDependencyAnaliser.h"

namespace input_dependency {

class VirtualCallSiteAnalysisResult;
class IndirectCallSitesAnalysisResult;

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
                                 const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                 const IndirectCallSitesAnalysisResult& indirectCallsInfo,
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
    void reflect(const DependencyAnaliser::ValueDependencies& dependencies,
                 const DepInfo& mandatory_deps) override;
    bool isReflected() const override
    {
        return m_isReflected;
    }

    void addControlDependencies(ValueDepInfo& valueDepInfo) override;
    void addControlDependencies(DepInfo& depInfo) override;

    DepInfo getInstructionDependencies(llvm::Instruction* instr) const override;
    void setOutArguments(const ArgumentDependenciesMap& outArgs) override;

    /// \name Implementation of DependencyAnaliser interface
    /// \{
protected:
    DepInfo getInstructionDependencies(llvm::Instruction* instr) override;
    void updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info) override;
    void updateAliasingOutArgDependencies(llvm::Value* val, const ValueDepInfo& info, int arg_idx = -1) override;
    ValueDepInfo getCompositeValueDependencies(llvm::Value* value, llvm::Instruction* element_instr) override;

private:
    DepInfo getLoadInstrDependencies(llvm::LoadInst* instr) override;
    void updateFunctionCallSiteInfo(llvm::CallInst* callInst, llvm::Function* F) override;
    void updateFunctionInvokeSiteInfo(llvm::InvokeInst* invokeInst, llvm::Function* F) override;
    /// \}

private:
    void updateValueDependentInstructions(const DepInfo& info, llvm::Instruction* instr);
    void updateValueDependentCallArguments(llvm::CallInst* callInst, llvm::Function* F);
    void updateValueDependentInvokeArguments(llvm::InvokeInst* invokeInst, llvm::Function* F);
    void updateValueDependentCallReferencedGlobals(llvm::CallInst* callInst, llvm::Function* F);
    void updateValueDependentInvokeReferencedGlobals(llvm::InvokeInst* invokeInst, llvm::Function* F);

    void reflect(llvm::Value* value, const ValueDepInfo& deps);
    void reflectOnValues(llvm::Value* value, const DepInfo& depInfo);
    void reflectOnInstructions(llvm::Value* value, const ValueDepInfo& depInfo);
    void reflectOnOutArguments(llvm::Value* value, const DepInfo& depInfo);
    void reflectOnCalledFunctionArguments(llvm::Value* value, const DepInfo& depInfo);
    void reflectOnCalledFunctionReferencedGlobals(llvm::Value* value, const DepInfo& depInfo);
    void reflectOnInvokedFunctionArguments(llvm::Value* value, const DepInfo& depInfo);
    void reflectOnInvokedFunctionReferencedGlobals(llvm::Value* value, const DepInfo& depInfo);
    void reflectOnReturnValue(llvm::Value* value, const DepInfo& depInfo);
    void reflectOnDepInfo(llvm::Value* value,
                          DepInfo& depInfoTo,
                          const DepInfo& depInfoFrom,
                          bool eraseAfterReflection = true);
    void reflectOnDepInfo(llvm::Value* value,
                          ValueDepInfo& depInfoTo,
                          const DepInfo& depInfoFrom,
                          bool eraseAfterReflection = true);
    void resolveValueDependencies(const DependencyAnaliser::ValueDependencies& successorDependencies,
                                  const DepInfo& mandatory_deps);
    DepInfo getValueFinalDependencies(llvm::Value* value, ValueSet& processed);

private:
    std::unordered_map<llvm::Value*, InstrSet> m_valueDependentInstrs;
    std::unordered_map<llvm::Value*, ArgumentSet> m_valueDependentOutArguments; 

    using CallArgumentSet = std::unordered_map<llvm::CallInst*, ArgumentSet>;
    std::unordered_map<llvm::Value*, CallArgumentSet> m_valueDependentFunctionCallArguments;

    using InvokeArgumentSet = std::unordered_map<llvm::InvokeInst*, ArgumentSet>;
    std::unordered_map<llvm::Value*, InvokeArgumentSet> m_valueDependentFunctionInvokeArguments;

    using CallGlobalsSet = std::unordered_map<llvm::CallInst*, GlobalsSet>;
    std::unordered_map<llvm::Value*, CallGlobalsSet> m_valueDependentCallGlobals;

    using InvokeGlobalsSet = std::unordered_map<llvm::InvokeInst*, GlobalsSet>;
    std::unordered_map<llvm::Value*, InvokeGlobalsSet> m_valueDependentInvokeGlobals;

    std::unordered_map<llvm::Instruction*, DepInfo> m_instructionValueDependencies;
    bool m_isReflected;
}; // class ReflectingBasicBlockAnaliser

} // namespace input_dependency

