#pragma once

#include "llvm/Pass.h"
#include "analysis/IndirectCallSiteResults.h"

#include <memory>
#include <unordered_set>
#include <unordered_map>

namespace llvm {
class FunctionType;
class Function;
class CallSite;
}

namespace input_dependency {

class IndirectCallSiteAnalysisResult : public IndirectCallSiteResults
{
public:
    using FunctionSet = IndirectCallSiteResults::FunctionSet;

public:
    void addIndirectCallTarget(llvm::FunctionType* type, llvm::Function* target);
    void addIndirectCallTargets(llvm::FunctionType* type, const FunctionSet& targets);

    bool hasIndirectTargets(llvm::FunctionType* func_ty) const;
    const FunctionSet& getIndirectTargets(llvm::FunctionType* func_ty) const;

    virtual bool hasIndCSCallees(const llvm::CallSite& callSite) const override;
    virtual FunctionSet getIndCSCallees(const llvm::CallSite& callSite) override;

public:
    void dump();

private:
    std::unordered_map<llvm::FunctionType*, FunctionSet> m_indirectCallTargets;
}; // class IndirectCallSiteAnalysisResult

class IndirectCallSitesAnalysis : public llvm::ModulePass
{
public:
    using IndCSAnalysisResTy = std::shared_ptr<IndirectCallSiteAnalysisResult>;
public:
    static char ID;

    IndirectCallSitesAnalysis();

public:
    bool runOnModule(llvm::Module& M) override;

public:
    IndCSAnalysisResTy getIndirectsAnalysisResult()
    {
        return m_results;
    }

private:
    class VirtualsImpl;
    std::unique_ptr<VirtualsImpl> m_vimpl;

    class IndirectsImpl;
    std::unique_ptr<IndirectsImpl> m_iimpl;

    IndCSAnalysisResTy m_results;
}; // class VirtualCallSitesAnalysis

}

