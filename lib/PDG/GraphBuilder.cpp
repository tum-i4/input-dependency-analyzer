#include "PDG/GraphBuilder.h"

#include "PDG/LLVMNode.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

GraphBuilder::PDGNodeTy GraphBuilder::createInstructionNodeFor(llvm::Instruction* instr)
{
    return std::make_shared<LLVMInstructionNode>(instr);
}

GraphBuilder::PDGNodeTy GraphBuilder::createBasicBlockNodeFor(llvm::BasicBlock* block)
{
    return std::make_shared<LLVMBasicBlockNode>(block);
}

GraphBuilder::PDGNodeTy GraphBuilder::createFunctionNodeFor(llvm::Function* F)
{
    return std::make_shared<LLVMFunctionNode>(F);
}

GraphBuilder::PDGNodeTy GraphBuilder::createGlobalNodeFor(llvm::GlobalVariable* global)
{
    return std::make_shared<LLVMGlobalVariableNode>(global);
}

GraphBuilder::PDGNodeTy GraphBuilder::createFormalArgNodeFor(llvm::Argument* arg)
{
    return std::make_shared<LLVMFormalArgumentNode>(arg);
}

 GraphBuilder::PDGNodeTy GraphBuilder::createActualArgumentNode(llvm::CallSite& callSite,
                                                                llvm::Value* arg,
                                                                unsigned idx)
{
    return std::make_shared<LLVMActualArgumentNode>(callSite, arg, idx);
}

GraphBuilder::PDGNodeTy GraphBuilder::createNullNode()
{
    return std::make_shared<LLVMNullNode>();
}

GraphBuilder::PDGNodeTy GraphBuilder::createConstantNodeFor(llvm::Constant* constant)
{
    return std::make_shared<LLVMConstantNode>(constant);
}

GraphBuilder::PDGNodeTy GraphBuilder::createVaArgNodeFor(llvm::Function* F)
{
    return std::make_shared<LLVMVarArgNode>(F);
}

GraphBuilder::PDGNodeTy GraphBuilder::createPhiNode(const std::vector<llvm::Value*>& values,
                                                    const std::vector<llvm::BasicBlock*>& blocks)
{
    return std::make_shared<LLVMPhiNode>(values, blocks);
}


} // namespace input_dependency

