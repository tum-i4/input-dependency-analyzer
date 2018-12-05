#include "PDG/GraphBuilder.h"

#include "PDG/LLVMNode.h"

#include "llvm/IR/Instructions.h"
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

GraphBuilder::PDGGlobalNodeTy GraphBuilder::createGlobalNodeFor(llvm::GlobalVariable* global)
{
    return std::make_shared<LLVMGlobalVariableNode>(global);
}

GraphBuilder::ArgNodeTy GraphBuilder::createFormalArgNodeFor(llvm::Argument* arg)
{
    return std::make_shared<LLVMFormalArgumentNode>(arg);
}

GraphBuilder::PDGNodeTy GraphBuilder::createNullNode()
{
    return std::make_shared<LLVMNullNode>();
}

GraphBuilder::PDGNodeTy GraphBuilder::createConstantNodeFor(llvm::Constant* constant)
{
    return std::make_shared<LLVMConstantNode>(constant);
}

} // namespace input_dependency

