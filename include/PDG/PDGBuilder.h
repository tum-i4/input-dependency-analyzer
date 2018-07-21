#pragma once

#include <memory>
#include "PDG.h"
#include "FunctionPDG.h"

#include "llvm/IR/InstVisitor.h"

class SVFG;
class SVFGNode;

namespace llvm {

class MemorySSA;
class Module;
class Function;
class Value;

}

namespace pdg {

class PDGBuilder : public llvm::InstVisitor<PDGBuilder>
{
public:
    using PDGType = std::shared_ptr<PDG>;
    using FunctionPDGTy = PDG::FunctionPDGTy;
    using PDGNodeTy = FunctionPDG::PDGNodeTy;

public:
    PDGBuilder(llvm::Module* M, SVFG* svfg, llvm::MemorySSA& ssa);

    virtual ~PDGBuilder() = default;
    PDGBuilder(const PDGBuilder& ) = delete;
    PDGBuilder(PDGBuilder&& ) = delete;
    PDGBuilder& operator =(const PDGBuilder& ) = delete;
    PDGBuilder& operator =(PDGBuilder&& ) = delete;
 
    void build();

public:
    PDGType getPDG()
    {
        return std::move(m_pdg);
    }

public:
    /// visit overrides
    // TODO: those are instructions that seems to be interesting to handle separately. 
    // Add or remove is necessary
    void visitBranchInst(llvm::BranchInst& I);
    void visitLoadInst(llvm::LoadInst& I);
    void visitStoreInst(llvm::StoreInst& I);
    void visitGetElementPtrInst(llvm::GetElementPtrInst& I);
    void visitPhiNode(llvm::PHINode& I);
    void visitMemSetInst(llvm::MemSetInst& I);
    void visitMemCpyInst(llvm::MemCpyInst& I);
    void visitMemMoveInst(llvm::MemMoveInst &I);
    void visitMemTransferInst(llvm::MemTransferInst &I);
    void visitMemIntrinsic(llvm::MemIntrinsic &I);
    void visitCallInst(llvm::CallInst& I);
    void visitInvokeInst(llvm::InvokeInst& I);
    void visitTerminatorInst(llvm::TerminatorInst& I);

    // all instructions not handled individually will get here
    void visitInstruction(llvm::Instruction& I);

private:
    void buildFunctionPDG(llvm::Function* F);
    void visitGlobals();
    void visitFormalArguments(llvm::Function* F);
    void visitBlock(llvm::BasicBlock& B);
    void visitBlockInstructions(llvm::BasicBlock& B);
    void addDataEdge(PDGNodeTy source, PDGNodeTy dest);
    void addControlEdge(PDGNodeTy source, PDGNodeTy dest);
    PDGNodeTy processLLVMSSADef(llvm::Instruction& I);
    PDGNodeTy processSVFGDef(llvm::Instruction& I);
    PDGNodeTy getNodeFor(llvm::Value* value);
    PDGNodeTy getNodeFor(llvm::BasicBlock* block);
    PDGNodeTy getNodeFor(llvm::MemoryPhi* memPhi);
    PDGNodeTy getNodeFor(SVFGNode* svfgNode);

private:
    llvm::Module* m_module;
    SVFG* m_svfg;
    llvm::MemorySSA& m_memorySSA;
    PDGType m_pdg;
    FunctionPDGTy m_currentFPDG;
}; // class PDGBuilder

} // namespace pdg

