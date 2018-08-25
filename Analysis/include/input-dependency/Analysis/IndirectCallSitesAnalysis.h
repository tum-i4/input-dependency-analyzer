#pragma once

#include "llvm/Pass.h"

#include "input-dependency/Analysis/definitions.h"

#include <memory>

namespace llvm {
class FunctionType;
}

namespace input_dependency {

class VirtualCallSiteAnalysisResult
{
public:
    void addVirtualCall(llvm::CallInst* call);
    void addVirtualCallCandidates(llvm::CallInst* call, FunctionSet&& candidates);
    void addVirtualInvoke(llvm::InvokeInst* invoke);
    void addVirtualInvokeCandidates(llvm::InvokeInst* call, FunctionSet&& candidates);

    bool hasVirtualCallCandidates(llvm::Instruction* instr) const;
    const FunctionSet& getVirtualCallCandidates(llvm::Instruction* instr) const;

public:
    void dump();

private:
    void addInstr(llvm::Instruction* instr);
    void addCandidates(llvm::Instruction* instr, FunctionSet&& candidates);
    bool hasCandidates(llvm::Instruction* instr) const;
    const FunctionSet& getCandidates(llvm::Instruction* instr) const;

private:
    std::unordered_map<llvm::Instruction*, FunctionSet> m_virtualCallCandidates;
}; // class VirtualCallSiteAnalysisResult

class IndirectCallSitesAnalysisResult
{
public:
    void addIndirectCallTarget(llvm::FunctionType* type, llvm::Function* target);
    void addIndirectCallTargets(llvm::FunctionType* type, const FunctionSet& targets);

    bool hasIndirectTargets(llvm::FunctionType* func_ty) const;
    const FunctionSet& getIndirectTargets(llvm::FunctionType* func_ty) const;
    template <class CallInstTy>
    bool hasIndirectTargets(CallInstTy* instr) const;
    template <class CallInstTy>
    const FunctionSet& getIndirectTargets(CallInstTy* instr) const;

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
    VirtualCallSiteAnalysisResult& getVirtualsAnalysisResult();
    const VirtualCallSiteAnalysisResult& getVirtualsAnalysisResult() const;

    IndirectCallSitesAnalysisResult& getIndirectsAnalysisResult();
    const IndirectCallSitesAnalysisResult& getIndirectsAnalysisResult() const;

private:
    class VirtualsImpl;
    std::unique_ptr<VirtualsImpl> m_vimpl;

    class IndirectsImpl;
    std::unique_ptr<IndirectsImpl> m_iimpl;
}; // class VirtualCallSitesAnalysis

}

