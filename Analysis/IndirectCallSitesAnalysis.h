#pragma once

#include "llvm/Pass.h"

#include "definitions.h"

#include <memory>

namespace input_dependency {

class VirtualCallSiteAnalysisResult
{
public:
    void addVirtualCall(llvm::CallInst* call);
    void addVirtualCallCandidates(llvm::CallInst* call, FunctionSet&& candidates);
    void addVirtualInvoke(llvm::InvokeInst* invoke);
    void addVirtualInvokeCandidates(llvm::InvokeInst* call, FunctionSet&& candidates);

    bool hasVirtualCallCandidates(llvm::CallInst* call) const;
    const FunctionSet& getVirtualCallCandidates(llvm::CallInst* call) const;
    bool hasVirtualInvokeCandidates(llvm::InvokeInst* invoke) const;
    const FunctionSet& getVirtualInvokeCandidates(llvm::InvokeInst* invoke) const;

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
    void addIndirectCallTarget(llvm::CallInst* call, llvm::Function* target);
    void addIndirectCallTargets(llvm::CallInst* call, const FunctionSet& targets);
    void addIndirectInvokeTarget(llvm::InvokeInst* invoke, llvm::Function* target);
    void addIndirectInvokeTargets(llvm::InvokeInst* invoke, const FunctionSet& targets);

    bool hasIndirectCallTargets(llvm::CallInst* call) const;
    const FunctionSet& getIndirectCallTargets(llvm::CallInst* call) const;
    bool hasIndirectInvokeTargets(llvm::InvokeInst* invoke) const;
    const FunctionSet& getIndirectInvokeTargets(llvm::InvokeInst* invoke) const;

public:
    void dump();

private:
    const FunctionSet& getTargets(llvm::Instruction* instr) const;

private:
    std::unordered_map<llvm::Instruction*, FunctionSet> m_indirectCallTargets;
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

