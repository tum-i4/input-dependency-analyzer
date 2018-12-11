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

class LLVMNode : public InputDependencyNode
               , public pdg::PDGLLVMNode
{
public:
    LLVMNode(llvm::Value* value, pdg::PDGLLVMNode::NodeType nodeType)
        : pdg::PDGLLVMNode(value, nodeType)
    {
    }
};

class LLVMInstructionNode : public LLVMNode
{
public:
    explicit LLVMInstructionNode(llvm::Instruction* instr)
        : LLVMNode(instr, pdg::PDGLLVMNode::InstructionNode)
    {
    }
};

class LLVMFormalArgumentNode : public LLVMNode
{
public:
    explicit LLVMFormalArgumentNode(llvm::Argument* arg)
        : LLVMNode(arg, pdg::PDGLLVMNode::FormalArgumentNode)
    {
    }

};

class LLVMVarArgNode : public LLVMNode
{
public:
    LLVMVarArgNode(llvm::Function* function)
        : LLVMNode(function, pdg::PDGLLVMNode::VaArgumentNode)
    {
    }

public:
    virtual std::string getNodeAsString() const override;

};

class LLVMActualArgumentNode : public LLVMNode
{
public:
    LLVMActualArgumentNode(llvm::CallSite& callSite,
                           llvm::Value* actualArg,
                           unsigned argIdx)
        : LLVMNode(actualArg, pdg::PDGLLVMNode::ActualArgumentNode)
        , m_callSite(callSite)
        , m_argIdx(argIdx)
    {
    }

public:
    const llvm::CallSite& getCallSite() const
    {
        return m_callSite;
    }

    const unsigned getArgIndex() const
    {
        return m_argIdx;
    }

private:
    llvm::CallSite m_callSite;
    unsigned m_argIdx;
};

class LLVMGlobalVariableNode : public LLVMNode
{
public:
    LLVMGlobalVariableNode(llvm::GlobalVariable* global)
        : LLVMNode(global, pdg::PDGLLVMNode::GlobalVariableNode)
    {
    }

};

class LLVMConstantExprNode : public LLVMNode
{
public:
    LLVMConstantExprNode(llvm::ConstantExpr* constant)
        : LLVMNode(constant, pdg::PDGLLVMNode::ConstantExprNode)
    {
    }

};

class LLVMConstantNode : public LLVMNode
{
public:
    LLVMConstantNode(llvm::Constant* constant)
        : LLVMNode(constant, pdg::PDGLLVMNode::ConstantNode)
    {
    }
};

class LLVMFunctionNode : public LLVMNode
{
public:
    LLVMFunctionNode(llvm::Function* function)
        : LLVMNode(function, pdg::PDGLLVMNode::FunctionNode)
        , m_function(function)
    {
    }

public:
    virtual std::string getNodeAsString() const override
    {
        return m_function->getName();
    }

    llvm::Function* getFunction() const
    {
        return m_function;
    }

private:
    llvm::Function* m_function;
};

class LLVMBasicBlockNode : public LLVMNode
{
public:
    LLVMBasicBlockNode(llvm::BasicBlock* block)
        : LLVMNode(block, pdg::PDGLLVMNode::BasicBlockNode)
            , m_block(block)
    {
    }

public:
    virtual std::string getNodeAsString() const override
    {
        return m_block->getName();
    }

    llvm::BasicBlock* getBlock() const
    {
        return m_block;
    }

private:
    llvm::BasicBlock* m_block;

};

class LLVMNullNode : public LLVMNode
{
public:
    LLVMNullNode()
        : LLVMNode(nullptr, pdg::PDGLLVMNode::NullNode)
    {
    }
public:
    virtual bool addInEdge(PDGEdgeType inEdge) override
    {
        assert(false);
    }

    virtual std::string getNodeAsString() const override
    {
        return "Null";
    }
};

class PhiNode : public LLVMNode
{
public:
    using Values = std::vector<llvm::Value*>;
    using Blocks = std::vector<llvm::BasicBlock*>;

public:
    PhiNode(const Values& values, const Blocks& blocks)
        : LLVMNode(nullptr, NodeType::PhiNode)
        , m_values(values)
        , m_blocks(blocks)
    {
    }

public:
    virtual std::string getNodeAsString() const override;

public:
    unsigned getNumValues() const
    {
        return m_values.size();
    }

    llvm::Value* getValue(unsigned i) const
    {
        return m_values[i];
    }

    llvm::BasicBlock* getBlock(unsigned i) const
    {
        return m_blocks[i];
    }

private:
    Values m_values;
    Blocks m_blocks;
};

} // namespace input_dependency

