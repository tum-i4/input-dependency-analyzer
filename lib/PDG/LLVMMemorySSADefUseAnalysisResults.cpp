#include "PDG/LLVMMemorySSADefUseAnalysisResults.h"
#include "analysis/IndirectCallSitesAnalysis.h"

#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace pdg {

LLVMMemorySSADefUseAnalysisResults::LLVMMemorySSADefUseAnalysisResults(
                        llvm::MemorySSA* memSSA)
    : m_memorySSA(memSSA)
{
}

LLVMMemorySSADefUseAnalysisResults::PDGNodeTy LLVMMemorySSADefUseAnalysisResults::getDefSite(llvm::Value* value)
{
    return PDGNodeTy();
}

LLVMMemorySSADefUseAnalysisResults::PDGNodes LLVMMemorySSADefUseAnalysisResults::getDefSites(llvm::Value* value)
{
    return PDGNodes();
}

} // namespace pdg

