#pragma once

#include "PDG/DefUseResults.h"

namespace llvm {
class MemorySSA;
}

namespace pdg {

class LLVMMemorySSADefUseAnalysisResults : public DefUseResults
{
public:
    LLVMMemorySSADefUseAnalysisResults(llvm::MemorySSA* memSSA, /*IndirectCallSiteAnalysisResult */);
    
    LLVMMemorySSADefUseAnalysisResults(const LLVMMemorySSADefUseAnalysisResults& ) = delete;
    LLVMMemorySSADefUseAnalysisResults(LLVMMemorySSADefUseAnalysisResults&& ) = delete;
    LLVMMemorySSADefUseAnalysisResults& operator =(const LLVMMemorySSADefUseAnalysisResults& ) = delete;
    LLVMMemorySSADefUseAnalysisResults& operator =(LLVMMemorySSADefUseAnalysisResults&& ) = delete;


public:
    virtual PDGNodeTy getDefSite(llvm::Value* value) override;
    virtual PDGNodes getDefSites(llvm::Value* value) override;
    virtual bool hasIndCSCallees(const llvm::CallSite& callSite) const override;
    virtual FunctionSet getIndCSCallees(const llvm::CallSite& callSite) override;

private:
    llvm::MemorySSA* m_memorySSA;
    // IndirectCallSiteAnalysisResult m_indCSInfo;
}; // class LLVMMemorySSADefUseAnalysisResults

} // namespace pdg

