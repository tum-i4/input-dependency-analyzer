#pragma once

#include "input-dependency/Analysis/FunctionInputDependencyResultInterface.h"
#include "input-dependency/Analysis/DependencyAnaliser.h"

#include <memory>

namespace llvm {
class GlobalVariable;
class LoopInfo;
class PostDominatorTree;
class DominatorTree;
}

namespace input_dependency {

class IndirectCallSitesAnalysisResult;
class VirtualCallSiteAnalysisResult;

class FunctionAnaliser final : public FunctionInputDependencyResultInterface
{
public:
    FunctionAnaliser(llvm::Function* F,
                     const FunctionAnalysisGetter& getter);

public:
    void setFunction(llvm::Function* F);

    void setAAResults(llvm::AAResults* AAR);
    void setLoopInfo(llvm::LoopInfo* LI);
    void setPostDomTree(const llvm::PostDominatorTree* PDom);
    void setDomTree(const llvm::DominatorTree* dom);
    void setVirtualCallSiteAnalysisResult(const VirtualCallSiteAnalysisResult* virtualCallsInfo);
    void setIndirectCallSiteAnalysisResult(const IndirectCallSitesAnalysisResult* indirectCallsInfo);

    /// \name FunctionInputDependencyResultInterface implementation
    /// \{
public:
    llvm::Function* getFunction() override;
    const llvm::Function* getFunction() const override;
    bool isInputDepFunction() const override;
    void setIsInputDepFunction(bool isInputDep) override;
    bool isExtractedFunction() const override;
    void setIsExtractedFunction(bool isExtracted) override;

    bool areArgumentsFinalized() const;
    bool areGlobalsFinalized() const;

    bool isInputDependent(llvm::Instruction* instr) const override;
    bool isInputDependent(const llvm::Instruction* instr) const override;
    bool isInputIndependent(llvm::Instruction* instr) const override;
    bool isInputIndependent(const llvm::Instruction* instr) const override;
    bool isInputDependentBlock(llvm::BasicBlock* block) const override;
    bool isControlDependent(llvm::Instruction* I) const override;
    bool isDataDependent(llvm::Instruction* I) const override;
    bool isArgumentDependent(llvm::Instruction* I) const override;
    bool isArgumentDependent(llvm::BasicBlock* block) const override;
    bool isGlobalDependent(llvm::Instruction* I) const override;

    FunctionSet getCallSitesData() const override;
    FunctionCallDepInfo getFunctionCallDepInfo(llvm::Function* F) const override;
    bool changeFunctionCall(const llvm::Instruction* callInstr, llvm::Function* oldF, llvm::Function* newF) override;

    // caching for statistics
    long unsigned get_input_dep_blocks_count() const override;
    long unsigned get_input_indep_blocks_count() const override;
    long unsigned get_unreachable_blocks_count() const override;
    long unsigned get_unreachable_instructions_count() const override;
    long unsigned get_input_dep_count() const override;
    long unsigned get_input_indep_count() const override;
    long unsigned get_data_indep_count() const override;
    long unsigned get_input_unknowns_count() const override;

    FunctionAnaliser* toFunctionAnalysisResult() override
    {
        return this;
    }
    /// \}

    /// \name Analysis interface
    /// \{
public:
    /**
     * \brief Preliminary analyses input dependency of instructions in the function.
     * Performs context insensitive, flow sensitive input dependency analysis
     * Collects function call site dependency info.
     * \note Assumes that function arguments are user inputs.
     */
    void analyze() override;

    /**
     * \brief Refines results of the \link analyze by performing context-sensitive analysis given set of input dep arguments.
     * \param[in] inputDepArgs Arguments which are actually input dependent.
     * 
     * \note \link analyze function should be called before calling this function.
     */
    void finalizeArguments(const DependencyAnaliser::ArgumentDependenciesMap& inputDepArgs);

    /**
     * \brief Refines results of analysis given set of input dependent globals.
     * \param[in] globalsDeps global variables that are input dependent.
     * \note \link analyze function should be called before calling this function.
     */
    void finalizeGlobals(const DependencyAnaliser::GlobalVariableDependencyMap& globalsDeps);

    /// \}

    /// \name Intermediate input dep results interface
    /// \{
    const DependencyAnaliser::ArgumentDependenciesMap& getCallArgumentInfo(llvm::Function* F) const;
    DependencyAnaliser::GlobalVariableDependencyMap getCallGlobalsInfo(llvm::Function* F) const;
    bool isOutArgInputIndependent(llvm::Argument* arg) const;
    ValueDepInfo getOutArgDependencies(llvm::Argument* arg) const;
    bool isReturnValueInputIndependent() const;
    const ValueDepInfo& getRetValueDependencies() const;
    bool hasGlobalVariableDepInfo(llvm::GlobalVariable* global) const;
    ValueDepInfo getGlobalVariableDependencies(llvm::GlobalVariable* global) const;
    ValueDepInfo getDependencyInfoFromBlock(llvm::Value* val, llvm::BasicBlock* block) const;
    DepInfo getBlockDependencyInfo(llvm::BasicBlock* block) const;
    const GlobalsSet& getReferencedGlobals() const;
    const GlobalsSet& getModifiedGlobals() const;
    /// \}

    FunctionInputDependencyResultInterface* cloneForArguments(const DependencyAnaliser::ArgumentDependenciesMap& inputDepArgs);
    /// \name debug interface
    /// \{
    void dump() const;
    /// \}

private:
    class Impl;
    std::shared_ptr<Impl> m_analiser;
};

} // namespace input_dependency

