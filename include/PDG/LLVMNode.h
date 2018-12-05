#pragma once

#include "PDG/InputDependencyNode.h"
#include "PDG/PDG/PDGLLVMNode.h"

#include "llvm/IR/Argument.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"

namespace input_dependency
{

class LLVMInstructionNode : public InputDependencyNode
                          , public pdg::PDGLLVMInstructionNode
{
public:
    LLVMInstructionNode(llvm::Instruction* instr)
        : pdg::PDGLLVMInstructionNode(instr)
    {
    }
};

class LLVMFormalArgumentNode : public InputDependencyNode
                             , public pdg::PDGLLVMFormalArgumentNode
{
public:
    LLVMFormalArgumentNode(llvm::Argument* arg)
        : pdg::PDGLLVMFormalArgumentNode(arg)
    {
    }

};

class LLVMVarArgNode : public InputDependencyNode
                     , public pdg::PDGLLVMVaArgNode
{
public:
    LLVMVarArgNode(llvm::Function* function)
        : pdg::PDGLLVMVaArgNode(function)
    {
    }

};

class LLVMActualArgumentNode : public InputDependencyNode
                             , public pdg::PDGLLVMActualArgumentNode
{
public:
    LLVMActualArgumentNode(llvm::CallSite& callSite,
                           llvm::Value* actualArg,
                           unsigned argIdx)
        : pdg::PDGLLVMActualArgumentNode(callSite, actualArg, argIdx)
    {
    }
};

class LLVMGlobalVariableNode : public InputDependencyNode
                             , public pdg::PDGLLVMGlobalVariableNode
{
public:
    LLVMGlobalVariableNode(llvm::GlobalVariable* global)
        : pdg::PDGLLVMGlobalVariableNode(global)
    {
    }

};

class LLVMConstantExprNode : public InputDependencyNode
                           , public pdg::PDGLLVMConstantExprNode
{
public:
    LLVMConstantExprNode(llvm::ConstantExpr* constant)
        : pdg::PDGLLVMConstantExprNode(constant)
    {
    }

};

class LLVMConstantNode : public InputDependencyNode
                       , public pdg::PDGLLVMConstantNode
{
public:
    LLVMConstantNode(llvm::Constant* constant)
        : pdg::PDGLLVMConstantNode(constant)
    {
    }
};

class LLVMFunctionNode : public InputDependencyNode
                       , public pdg::PDGLLVMFunctionNode
{
public:
    LLVMFunctionNode(llvm::Function* function)
        : pdg::PDGLLVMFunctionNode(function)
    {
    }
};

class LLVMBasicBlockNode : public InputDependencyNode
                         , public pdg::PDGLLVMBasicBlockNode
{
public:
    LLVMBasicBlockNode(llvm::BasicBlock* block)
        : pdg::PDGLLVMBasicBlockNode(block)
    {
    }
};

class LLVMNullNode : public InputDependencyNode
                   , public pdg::PDGNullNode
{
public:
    LLVMNullNode() = default;
};

class PhiNode : public InputDependencyNode
              , public pdg::PDGPhiNode
{
public:
    PhiNode(const pdg::PDGPhiNode::Values& values,
            const pdg::PDGPhiNode::Blocks& blocks)
        : pdg::PDGPhiNode(values, blocks)
    {
    }
};

} // namespace input_dependency

