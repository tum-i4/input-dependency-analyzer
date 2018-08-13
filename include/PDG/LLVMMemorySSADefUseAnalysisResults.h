#pragma once

#include "PDG/DefUseResults.h"

#include <functional>
#include <unordered_map>
#include <vector>

namespace llvm {
class AAResults;
class BasicBlock;
class Function;
class Instruction;
class MemoryAccess;
class MemoryPhi;
class MemorySSA;
class Value;
}

namespace pdg {

class LLVMMemorySSADefUseAnalysisResults : public DefUseResults
{
public:
    using MemorySSAGetter = std::function<llvm::MemorySSA* (llvm::Function*)>;
    using AARGetter = std::function<llvm::AAResults* (llvm::Function*)>;

public:
    LLVMMemorySSADefUseAnalysisResults(const MemorySSAGetter& mssaGetter,
                                       const AARGetter& aaGetter);
    
    LLVMMemorySSADefUseAnalysisResults(const LLVMMemorySSADefUseAnalysisResults& ) = delete;
    LLVMMemorySSADefUseAnalysisResults(LLVMMemorySSADefUseAnalysisResults&& ) = delete;
    LLVMMemorySSADefUseAnalysisResults& operator =(const LLVMMemorySSADefUseAnalysisResults& ) = delete;
    LLVMMemorySSADefUseAnalysisResults& operator =(LLVMMemorySSADefUseAnalysisResults&& ) = delete;

public:
    virtual llvm::Value* getDefSite(llvm::Value* value) override;
    virtual PDGNodeTy getDefSiteNode(llvm::Value* value) override;

private:
    struct PHI {
        std::vector<llvm::BasicBlock*> blocks;
        std::vector<llvm::Value*> values;

        bool empty() const {
            return blocks.empty() && values.empty();
        }
    };

private:
    llvm::MemoryAccess* getMemoryDefAccess(llvm::Instruction* instr, llvm::MemorySSA* memorySSA);
    PHI getDefSites(llvm::Value* value,
                    llvm::MemoryAccess* access,
                    llvm::MemorySSA* memorySSA,
                    llvm::AAResults* aa);

private:
    const MemorySSAGetter& m_memorySSAGetter;
    const AARGetter& m_aarGetter;
}; // class LLVMMemorySSADefUseAnalysisResults

} // namespace pdg

