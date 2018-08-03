#pragma once

#include "PDG/DefUseResults.h"

namespace llvm {
class MemorySSA;
}

namespace pdg {

class LLVMMemorySSADefUseAnalysisResults : public DefUseResults
{
public:
    explicit LLVMMemorySSADefUseAnalysisResults(llvm::MemorySSA* memSSA);
    
    LLVMMemorySSADefUseAnalysisResults(const LLVMMemorySSADefUseAnalysisResults& ) = delete;
    LLVMMemorySSADefUseAnalysisResults(LLVMMemorySSADefUseAnalysisResults&& ) = delete;
    LLVMMemorySSADefUseAnalysisResults& operator =(const LLVMMemorySSADefUseAnalysisResults& ) = delete;
    LLVMMemorySSADefUseAnalysisResults& operator =(LLVMMemorySSADefUseAnalysisResults&& ) = delete;


public:
    virtual PDGNodeTy getDefSite(llvm::Value* value) override;
    virtual PDGNodes getDefSites(llvm::Value* value) override;

private:
    llvm::MemorySSA* m_memorySSA;
}; // class LLVMMemorySSADefUseAnalysisResults

} // namespace pdg

