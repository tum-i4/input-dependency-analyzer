#pragma once

#include "DependencyAnaliser.h"

#include <memory>

namespace llvm {
class LoopInfo;
}

namespace input_dependency {

class FunctionAnaliser
{
public:
    FunctionAnaliser(llvm::Function* F, llvm::AAResults& AAR, llvm::LoopInfo& LI, const FunctionAnalysisGetter& getter);

public:
    /**
     * \brief Preliminary analyses input dependency of instructions in the function.
     * Results of this function are:
     *      - Primary information about instruction input dependency.
            \note This information is not complete as it is based on assumption, that function input arguments are also program inputs.
            - Information about function calls from this function, whith additional information about called function arguments being input dependent or not.
     *
     * \note The call site information can be obtained with \link getCallSitesData function.
     * \note To make analysis results final call \link finalize after calling this function.
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
    void finalize(const DependencyAnaliser::ArgumentDependenciesMap& inputDepArgs);

    /// Get call site info collected by \link analize function.
    DependencyAnaliser::FunctionArgumentsDependencies getCallSitesData() const;

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

    /**
     * \brief Checks if the given instruction is input dependent if input dependent arguments of the function are the given ones.
     * \param[in] instr Instruction to check
     * \param[in] inputDepArgs positions of input dependent arguments of the function.
     * 
     * \a instr will be input dependent if it depends on argument from \a inputDepArgs
     * \note \link finalize function should have been called before calling this function.
     *
     * \see \link analize
     * \see \link finalize
     * \see \link isInputDependent
     */
    //bool isInputDependent(llvm::Instruction* instr, const ArgNos& inputDepArgs) const;

    bool isOutArgInputDependent(llvm::Argument* arg) const;
    ArgumentSet getOutArgDependencies(llvm::Argument* arg) const;
    bool isReturnValueInputDependent() const;
    ArgumentSet getRetValueDependencies() const;

    void dump() const;

    llvm::Function* getFunction();
    const llvm::Function* getFunction() const;

private:
    class Impl;
    std::shared_ptr<Impl> m_analiser;
};

} // namespace input_dependency

