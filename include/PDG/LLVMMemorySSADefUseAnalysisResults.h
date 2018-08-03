#pragma once

#include "PDG/DefUseResults.h"

namespace llvm {
class MemorySSA;
class MemoryPhi;
class Value;
class BasicBlock;
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

private:
    void getPhiValueAndBlocks(llvm::MemoryPhi* memPhi,
                              std::vector<llvm::Value*>& values,
                              std::vector<llvm::BasicBlock*>& blocks);
private:
    llvm::MemorySSA* m_memorySSA;
}; // class LLVMMemorySSADefUseAnalysisResults

} // namespace pdg

