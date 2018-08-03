#pragma once

#include "PDG.h"
#include "FunctionPDG.h"

#include "llvm/IR/InstVisitor.h"

#include <memory>
#include <unordered_set>
#include <functional>

namespace llvm {

class CallSite;
class MemorySSA;
class Module;
class Function;
class Value;

}

namespace input_dependency {

class IndirectCallSiteResults;
}

namespace pdg {

class DefUseResults;

class PDGBuilder : public llvm::InstVisitor<PDGBuilder>
{
public:
    using PDGType = std::shared_ptr<PDG>;
    using FunctionPDGTy = PDG::FunctionPDGTy;
    using DefUseResultsTy = std::shared_ptr<DefUseResults>;
    using IndCSResultsTy = std::shared_ptr<input_dependency::IndirectCallSiteResults>;
    using PDGNodeTy = FunctionPDG::PDGNodeTy;
    using FunctionSet = std::unordered_set<llvm::Function*>;

public:
    PDGBuilder(llvm::Module* M,
               DefUseResultsTy pointerDefUse,
               DefUseResultsTy scalarDefUse,
               IndCSResultsTy indCSResults);

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
    FunctionPDGTy buildFunctionDefinition(llvm::Function* F);
    void visitGlobals();
    void visitFormalArguments(FunctionPDGTy functionPDG, llvm::Function* F);
    void visitBlock(llvm::BasicBlock& B);
    void visitBlockInstructions(llvm::BasicBlock& B);
    void visitCallSite(llvm::CallSite& callSite);
    void addDataEdge(PDGNodeTy source, PDGNodeTy dest);
    void addControlEdge(PDGNodeTy source, PDGNodeTy dest);
    PDGNodeTy getInstructionNodeFor(llvm::Instruction* instr);
    PDGNodeTy getNodeFor(llvm::Value* value);
    PDGNodeTy getNodeFor(llvm::BasicBlock* block);
    void addActualArgumentNodeConnections(PDGNodeTy actualArgNode,
                                          unsigned argIdx,
                                          const FunctionSet& callees);

private:
    llvm::Module* m_module;
    DefUseResultsTy m_ptDefUse;
    DefUseResultsTy m_scalarDefUse;
    IndCSResultsTy m_indCSResults;
    PDGType m_pdg;
    FunctionPDGTy m_currentFPDG;
}; // class PDGBuilder

} // namespace pdg

