#pragma once

#include <memory>
#include "PDG.h"
#include "FunctionPDG.h"

#include "llvm/IR/InstVisitor.h"

namespace llvm {

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

public:
    PDGBuilder(llvm::Module* M);

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
    void visitInstructions(llvm::BasicBlock& B);
    void addDataEdge(FunctionPDG::LLVMNodeTy source, FunctionPDG::LLVMNodeTy dest);
    FunctionPDG::LLVMNodeTy getNodeFor(llvm::Value* value);

private:
    llvm::Module* m_module;
    PDGType m_pdg;
    FunctionPDGTy m_currentFPDG;
}; // class PDGBuilder

} // namespace pdg

