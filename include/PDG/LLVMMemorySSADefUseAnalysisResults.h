#pragma once

#include "PDG/DefUseResults.h"

#include <functional>
#include <unordered_map>

namespace llvm {
class MemorySSA;
class MemoryAccess;
class MemoryPhi;
class Value;
class BasicBlock;
class Function;
}

namespace pdg {

class LLVMMemorySSADefUseAnalysisResults : public DefUseResults
{
public:
    using MemorySSAGetter = std::function<llvm::MemorySSA* (llvm::Function*)>;

public:
    explicit LLVMMemorySSADefUseAnalysisResults(const MemorySSAGetter& mssaGetter);
    
    LLVMMemorySSADefUseAnalysisResults(const LLVMMemorySSADefUseAnalysisResults& ) = delete;
    LLVMMemorySSADefUseAnalysisResults(LLVMMemorySSADefUseAnalysisResults&& ) = delete;
    LLVMMemorySSADefUseAnalysisResults& operator =(const LLVMMemorySSADefUseAnalysisResults& ) = delete;
    LLVMMemorySSADefUseAnalysisResults& operator =(LLVMMemorySSADefUseAnalysisResults&& ) = delete;


public:
    virtual llvm::Value* getDefSite(llvm::Value* value) override;
    virtual PDGNodeTy getDefSiteNode(llvm::Value* value) override;

private:
    llvm::MemoryAccess* getMemoryDefAccess(llvm::Value* value);
    void getPhiValueAndBlocks(llvm::MemoryPhi* memPhi,
                              std::vector<llvm::Value*>& values,
                              std::vector<llvm::BasicBlock*>& blocks);
private:
    const MemorySSAGetter& m_memorySSAGetter;
    llvm::MemorySSA* m_memorySSA;
    std::unordered_map<unsigned, PDGNodeTy> m_phiNodes;
}; // class LLVMMemorySSADefUseAnalysisResults

} // namespace pdg

