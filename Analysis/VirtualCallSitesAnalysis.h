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

class VirtualCallSitesAnalysis : public llvm::ModulePass
{
public:
    static char ID;

    VirtualCallSitesAnalysis();

public:
    bool runOnModule(llvm::Module& M) override;

public:
    VirtualCallSiteAnalysisResult& getAnalysisResult();
    const VirtualCallSiteAnalysisResult& getAnalysisResult() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
}; // class VirtualCallSitesAnalysis

}

