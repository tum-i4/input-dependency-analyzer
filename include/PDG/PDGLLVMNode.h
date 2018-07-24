#pragma once

#include "PDGNode.h"

#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"

namespace pdg {

class PDGLLVMNode : public PDGNode
{
public:
    explicit PDGLLVMNode(llvm::Value* node_value)
        : m_value(node_value)
    {
    }

    virtual ~PDGLLVMNode() = default;

    llvm::Value* getNodeValue() const
    {
        return m_value;
    }

    virtual NodeType getNodeType() const override
    {
        return NodeType::UnknownNode;
    }

public:
    static bool isLLVMNodeType(NodeType nodeType)
    {
        return nodeType == NodeType::UnknownNode
            || (nodeType >= NodeType::InstructionNode && nodeType <= NodeType::NullNode);
    }

    static bool classof(const PDGNode* node)
    {
        return isLLVMNodeType(node->getNodeType());
    }

private:
    llvm::Value* m_value;
}; // class PDGLLVMNode

class PDGLLVMInstructionNode : public PDGLLVMNode
{
public:
    explicit PDGLLVMInstructionNode(llvm::Instruction* instr)
        : PDGLLVMNode(instr)
    {
    }

    NodeType getNodeType() const override
    {
        return NodeType::InstructionNode;
    }

public:
    static bool classof(const PDGLLVMNode* node)
    {
        return node->getNodeType() == NodeType::InstructionNode;
    }

    static bool classof(const PDGNode* node)
    {
        return llvm::isa<PDGLLVMNode>(node) && classof(llvm::cast<PDGLLVMNode>(node));
    }

}; // class PDGInstructionNode

class PDGLLVMFormalArgumentNode : public PDGLLVMNode
{
public:
    explicit PDGLLVMFormalArgumentNode(llvm::Argument* arg)
        : PDGLLVMNode(arg)
    {
    }

    NodeType getNodeType() const override
    {
        return NodeType::FormalArgumentNode;
    }

public:
    static bool classof(const PDGLLVMNode* node)
    {
        return node->getNodeType() == NodeType::FormalArgumentNode;
    }

    static bool classof(const PDGNode* node)
    {
        return llvm::isa<PDGLLVMNode>(node) && classof(llvm::cast<PDGLLVMNode>(node));
    }

}; // class PDGArgumentNode

class PDGLLVMActualArgumentNode : public PDGLLVMNode
{
public:
    explicit PDGLLVMActualArgumentNode(llvm::CallSite& callSite, llvm::Value* actualArg)
        : PDGLLVMNode(actualArg)
        , m_callSite(callSite)
    {
    }

    NodeType getNodeType() const override
    {
        return NodeType::ActualArgumentNode;
    }

public:
    static bool classof(const PDGLLVMNode* node)
    {
        return node->getNodeType() == NodeType::ActualArgumentNode;
    }

    static bool classof(const PDGNode* node)
    {
        return llvm::isa<PDGLLVMNode>(node) && classof(llvm::cast<PDGLLVMNode>(node));
    }

private:
    llvm::CallSite m_callSite;
}; // class PDGArgumentNode


class PDGLLVMGlobalVariableNode : public PDGLLVMNode
{
public:
    explicit PDGLLVMGlobalVariableNode(llvm::GlobalVariable* var)
        : PDGLLVMNode(var)
    {
    }

    NodeType getNodeType() const override
    {
        return NodeType::GlobalVariableNode;
    }

public:
    static bool classof(const PDGLLVMNode* node)
    {
        return node->getNodeType() == NodeType::GlobalVariableNode;
    }

    static bool classof(const PDGNode* node)
    {
        return llvm::isa<PDGLLVMNode>(node) && classof(llvm::cast<PDGLLVMNode>(node));
    }
}; // class PDGGlobalVariableNodeNode

class PDGLLVMConstantExprNode : public PDGLLVMNode
{
public:
    explicit PDGLLVMConstantExprNode(llvm::ConstantExpr* expr)
        : PDGLLVMNode(expr)
    {
    }

    NodeType getNodeType() const override
    {
        return NodeType::ConstantExprNode;
    }

public:
    static bool classof(const PDGLLVMNode* node)
    {
        return node->getNodeType() == NodeType::ConstantExprNode;
    }

    static bool classof(const PDGNode* node)
    {
        return llvm::isa<PDGLLVMNode>(node) && classof(llvm::cast<PDGLLVMNode>(node));
    }
}; // class PDGConstantExprNode

class PDGLLVMConstantNode : public PDGLLVMNode
{
public:
    explicit PDGLLVMConstantNode(llvm::Constant* constant)
        : PDGLLVMNode(constant)
    {
    }

    NodeType getNodeType() const override
    {
        return NodeType::ConstantNode;
    }

    virtual bool addInEdge(PDGEdgeType inEdge) override
    {
        assert(false);
    }

public:
    static bool classof(const PDGLLVMNode* node)
    {
        return node->getNodeType() == NodeType::ConstantNode;
    }

    static bool classof(const PDGNode* node)
    {
        return llvm::isa<PDGLLVMNode>(node) && classof(llvm::cast<PDGLLVMNode>(node));
    }
}; // class PDGConstantNode

class PDGLLVMBasicBlockNode : public PDGLLVMNode
{
public:
    explicit PDGLLVMBasicBlockNode(llvm::BasicBlock* block)
        : PDGLLVMNode(llvm::dyn_cast<llvm::Value>(block))
    {
    }

    NodeType getNodeType() const override
    {
        return NodeType::BasicBlockNode;
    }

    virtual bool addOutEdge(PDGEdgeType outEdge) override
    {
        assert(false);
    }

public:
    static bool classof(const PDGLLVMNode* node)
    {
        return node->getNodeType() == NodeType::BasicBlockNode;
    }

    static bool classof(const PDGNode* node)
    {
        return llvm::isa<PDGLLVMNode>(node) && classof(llvm::cast<PDGLLVMNode>(node));
    }
}; // class PDGLLVMBasicBlockNode

class PDGNullNode : public PDGLLVMNode
{
public:
    PDGNullNode()
        : PDGLLVMNode(nullptr)
    {
    }

    NodeType getNodeType() const override
    {
        return NodeType::NullNode;
    }

    virtual bool addInEdge(PDGEdgeType inEdge) override
    {
        assert(false);
    }

public:
    static bool classof(const PDGLLVMNode* node)
    {
        return node->getNodeType() == NodeType::NullNode;
    }

    static bool classof(const PDGNode* node)
    {
        return llvm::isa<PDGLLVMNode>(node) && classof(llvm::cast<PDGLLVMNode>(node));
    }
}; // class PDGNullNode


// TODO: For now use PDGLLVMNode as base class, as expecting to have for MemoryPhi only
class PDGLLVMemoryAccessNode : public PDGLLVMNode
{
public:
    explicit PDGLLVMemoryAccessNode(llvm::MemoryPhi* memPhi)
        : PDGLLVMNode(memPhi)
    {
    }

    virtual NodeType getNodeType() const override
    {
        return NodeType::LLVMMemoryPhiNode;
    }

public:
    static bool classof(const PDGLLVMNode* node)
    {
        return node->getNodeType() == NodeType::LLVMMemoryPhiNode;
    }

    static bool classof(const PDGNode* node)
    {
        return llvm::isa<PDGLLVMNode>(node) && classof(llvm::cast<PDGLLVMNode>(node));
    }
}; // class PDGLLVMemoryAccessNode

} // namespace pdg

