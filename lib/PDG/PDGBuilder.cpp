#include "PDG/PDGBuilder.h"

#include "PDG/PDGEdge.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace pdg {

PDGBuilder::PDGBuilder(llvm::Module* M)
    : m_module(M)
{
}

void PDGBuilder::build()
{
    m_pdg.reset(new PDG(m_module));
    visitGlobals();

    for (auto& F : *m_module) {
        buildFunctionPDG(&F);
        m_pdg->addFunctionPDG(&F, m_currentFPDG);
        m_currentFPDG.reset();
    }
}

void PDGBuilder::visitGlobals()
{
    for (auto glob_it = m_module->global_begin();
            glob_it != m_module->global_end();
            ++glob_it) {
        m_pdg->addGlobalVariableNode(&*glob_it);
    }
}

void PDGBuilder::buildFunctionPDG(llvm::Function* F)
{
    m_currentFPDG.reset(new FunctionPDG(F));
    visitFormalArguments(F);
    for (auto& B : *F) {
        visitBlock(B);
        visitInstructions(B);
    }
}

void PDGBuilder::visitFormalArguments(llvm::Function* F)
{
    for (auto arg_it = F->arg_begin();
            arg_it != F->arg_end();
            ++arg_it) {
        m_currentFPDG->addFormalArgNode(&*arg_it);
    }
}

void PDGBuilder::visitBlock(llvm::BasicBlock& B)
{
    m_currentFPDG->addNode(llvm::dyn_cast<llvm::Value>(&B),
            FunctionPDG::LLVMNodeTy(new PDGLLVMBasicBlockNode(&B)));
}

void PDGBuilder::visitInstructions(llvm::BasicBlock& B)
{
    for (auto& I : B) {
        visit(I);
    }
}

void PDGBuilder::visitBranchInst(llvm::BranchInst& I)
{
    // TODO: output this for debug mode only
    llvm::dbgs() << "Branch Inst: " << I << "\n";
}

void PDGBuilder::visitLoadInst(llvm::LoadInst& I)
{
    // TODO: output this for debug mode only
    llvm::dbgs() << "Load Inst: " << I << "\n";
    // TODO: should connect to last definition
    // try to take from SVFG if does not have take from llvm SSA
}

void PDGBuilder::visitStoreInst(llvm::StoreInst& I)
{
    // TODO: output this for debug mode only
    llvm::dbgs() << "Store Inst: " << I << "\n";
    auto* valueOp = I.getValueOperand();
    auto sourceNode = getNodeFor(valueOp);
    if (!sourceNode) {
        return;
    }
    auto destNode = FunctionPDG::LLVMNodeTy(new PDGLLVMInstructionNode(&I));
    addDataEdge(sourceNode, destNode);
    m_currentFPDG->addNode(&I, destNode);
}

void PDGBuilder::visitGetElementPtrInst(llvm::GetElementPtrInst& I)
{
    llvm::dbgs() << "GetElementPtr Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitPhiNode(llvm::PHINode& I)
{
    llvm::dbgs() << "Phi Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemSetInst(llvm::MemSetInst& I)
{
    llvm::dbgs() << "MemSet Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemCpyInst(llvm::MemCpyInst& I)
{
    llvm::dbgs() << "MemCpy Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemMoveInst(llvm::MemMoveInst &I)
{
    llvm::dbgs() << "MemMove Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemTransferInst(llvm::MemTransferInst &I)
{
    llvm::dbgs() << "MemTransfer Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemIntrinsic(llvm::MemIntrinsic &I)
{
    llvm::dbgs() << "MemInstrinsic Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitCallInst(llvm::CallInst& I)
{
}

void PDGBuilder::visitInvokeInst(llvm::InvokeInst& I)
{
}

void PDGBuilder::visitTerminatorInst(llvm::TerminatorInst& I)
{
}

void PDGBuilder::visitInstruction(llvm::Instruction& I)
{
    auto destNode = getNodeFor(&I);
    for (auto op_it = I.op_begin(); op_it != I.op_end(); ++op_it) {
        auto sourceNode = getNodeFor(op_it->get());
        addDataEdge(sourceNode, destNode);
    }
}

void PDGBuilder::addDataEdge(FunctionPDG::LLVMNodeTy source, FunctionPDG::LLVMNodeTy dest)
{
    PDGNode::PDGEdgeType outEdge = PDGNode::PDGEdgeType(new PDGEdge(source, dest));
    PDGNode::PDGEdgeType inEdge = PDGNode::PDGEdgeType(new PDGEdge(dest, source));
    source->addOutDataEdge(outEdge);
    dest->addInDataEdge(inEdge);
}

FunctionPDG::LLVMNodeTy PDGBuilder::getNodeFor(llvm::Value* value)
{
    if (m_currentFPDG->hasNode(value)) {
        return m_currentFPDG->getNode(value);
    }
    if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
        if (!m_pdg->hasGlobalVariableNode(global)) {
            m_pdg->getGlobalVariableNode(global);
        }
        return m_pdg->getGlobalVariableNode(global);
    }
    if (auto* argument = llvm::dyn_cast<llvm::Argument>(value)) {
        assert(m_currentFPDG->hasFormalArgNode(argument));
        return m_currentFPDG->getFormalArgNode(argument);
    }

    if (auto* constant = llvm::dyn_cast<llvm::Constant>(value)) {
        m_currentFPDG->addNode(value, FunctionPDG::LLVMNodeTy(new PDGLLVMConstantNode(constant)));
    } else if (auto* instr = llvm::dyn_cast<llvm::Instruction>(value)) {
        m_currentFPDG->addNode(value, FunctionPDG::LLVMNodeTy(new PDGLLVMInstructionNode(instr)));
    } else {
        // do not assert here for now to keep track of possible values to be handled here
        llvm::dbgs() << "Unhandled value " << *value << "\n";
        return FunctionPDG::LLVMNodeTy();
    }
    return m_currentFPDG->getNode(value);
}

} // namespace pdg

