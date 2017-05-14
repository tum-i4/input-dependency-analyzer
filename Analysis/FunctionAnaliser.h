#pragma once

#include "DependencyAnaliser.h"
#include "definitions.h"

#include <memory>

namespace llvm {
class GlobalVariable;
class LoopInfo;
}

namespace input_dependency {

class VirtualCallSiteAnalysisResult;

class FunctionAnaliser
{
public:
    FunctionAnaliser(llvm::Function* F,
                     llvm::AAResults& AAR,
                     llvm::LoopInfo& LI,
                     const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                     const FunctionAnalysisGetter& getter);

public:
    void setFunction(llvm::Function* F);

public:
    /**
     * \brief Preliminary analyses input dependency of instructions in the function.
     * Results of this function is primary information about instructions input dependency, output arguments input dependency, and function call sites dependency info.
     * \note This information is not complete as it is based on assumption, that function input arguments are also program inputs.
     *       - Information about function calls from this function, whith additional information about called function arguments being input dependent or not.
     *
     * \note The call site information can be obtained with \link getCallSitesData function.
     * \note To make analysis results final call \link finalizeArguments and \link finalizeGlobals after calling this function.
     */
    void analize();

    /**
     * \brief Finalizes input dependency analysis by refining \link analize results with given set of input dependent arguments.
     * \param[in] inputDepArgs Arguments which are actually input dependent.
     * 
     * \note Instruction can be marked as input dependent in \a analize however if it does not depend on arguments
     *        from \a inputDepArgs, it will be unmarked.
     * \note \link analize function should be called before calling this function.
     */
    void finalizeArguments(const DependencyAnaliser::ArgumentDependenciesMap& inputDepArgs);
    void finalizeGlobals(const DependencyAnaliser::GlobalVariableDependencyMap& globalsDeps);

    /// Get call site info collected by \link analize function.
    FunctionSet getCallSitesData() const;

    DependencyAnaliser::ArgumentDependenciesMap getCallArgumentInfo(llvm::Function* F) const;

    // can't return with reference. Get with r-value if possible, to avoid copy
    FunctionCallDepInfo getFunctionCallDepInfo(llvm::Function* F) const;
    DependencyAnaliser::GlobalVariableDependencyMap getCallGlobalsInfo(llvm::Function* F) const;

    /**
     * \brief Checks if instruction is input dependent.
     * \param[in] instr Instruction to check
     *
     * \note If called after after \a analize but before \a finalize, the result may be incomplete.
     *
     * \see \link analize
     * \see \link finalize
     * \see \link isInputDependent
     */
    bool isInputDependent(llvm::Instruction* instr) const;
    bool isInputDependent(const llvm::Instruction* instr) const;
    bool isInputIndependent(llvm::Instruction* instr) const;
    bool isInputIndependent(const llvm::Instruction* instr) const;

    bool isOutArgInputIndependent(llvm::Argument* arg) const;
    DepInfo getOutArgDependencies(llvm::Argument* arg) const;
    bool isReturnValueInputIndependent() const;
    const DepInfo& getRetValueDependencies() const;
    bool hasGlobalVariableDepInfo(llvm::GlobalVariable* global) const;
    const DepInfo& getGlobalVariableDependencies(llvm::GlobalVariable* global) const;

    const GlobalsSet& getReferencedGlobals() const;
    const GlobalsSet& getModifiedGlobals() const;

    llvm::Function* getFunction();
    const llvm::Function* getFunction() const;

    void dump() const;

private:
    class Impl;
    std::shared_ptr<Impl> m_analiser;
};

} // namespace input_dependency

