#pragma once

#include "llvm/Pass.h"

#include <memory>
#include <unordered_set>
#include <unordered_map>

namespace llvm {
class FunctionType;
class Function;
class CallSite;
}

namespace input_dependency {

class IndirectCallSitesAnalysisResult
{
public:
    using FunctionSet = std::unordered_set<llvm::Function*>;
    void addIndirectCallTarget(llvm::FunctionType* type, llvm::Function* target);
    void addIndirectCallTargets(llvm::FunctionType* type, const FunctionSet& targets);

    bool hasIndirectTargets(llvm::FunctionType* func_ty) const;
    const FunctionSet& getIndirectTargets(llvm::FunctionType* func_ty) const;
    bool hasIndirectTargets(const llvm::CallSite& callSite) const;
    const FunctionSet& getIndirectTargets(const llvm::CallSite& callSite) const;

public:
    void dump();

private:
    std::unordered_map<llvm::FunctionType*, FunctionSet> m_indirectCallTargets;
}; // class IndirectCallSitesAnalysisResult

class IndirectCallSitesAnalysis : public llvm::ModulePass
{
public:
    static char ID;

    IndirectCallSitesAnalysis();

public:
    bool runOnModule(llvm::Module& M) override;

public:
    IndirectCallSitesAnalysisResult& getIndirectsAnalysisResult();
    const IndirectCallSitesAnalysisResult& getIndirectsAnalysisResult() const;

private:
    class VirtualsImpl;
    std::unique_ptr<VirtualsImpl> m_vimpl;

    class IndirectsImpl;
    std::unique_ptr<IndirectsImpl> m_iimpl;

    IndirectCallSitesAnalysisResult m_results;
}; // class VirtualCallSitesAnalysis

}

