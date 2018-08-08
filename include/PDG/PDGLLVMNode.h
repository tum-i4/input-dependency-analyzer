#pragma once

#include "PDGNode.h"

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
    enum NodeType : unsigned {
        InstructionNode = 0,
        FormalArgumentNode,
        ActualArgumentNode,
        GlobalVariableNode,
        ConstantExprNode,
        ConstantNode,
        BasicBlockNode,
        NullNode,
        PhiNode,
        UnknownNode
    };

public:
    explicit PDGLLVMNode(llvm::Value* node_value, NodeType type = UnknownNode)
        : m_value(node_value)
        , m_type(type)
    {
    }

    virtual ~PDGLLVMNode() = default;

public:
    virtual unsigned getNodeType() const override
    {
        return m_type;
    }

    virtual std::string getNodeAsString() const override;

public:
    llvm::Value* getNodeValue() const
    {
        return m_value;
    }

public:
    static bool isLLVMNodeType(NodeType nodeType)
    {
        return nodeType == NodeType::UnknownNode
            || (nodeType >= NodeType::InstructionNode && nodeType <= NodeType::PhiNode);
    }

    static bool classof(const PDGNode* node)
    {
        return isLLVMNodeType((NodeType) node->getNodeType());
    }

private:
    llvm::Value* m_value;
    NodeType m_type;
}; // class PDGLLVMNode

class PDGLLVMInstructionNode : public PDGLLVMNode
{
public:
    explicit PDGLLVMInstructionNode(llvm::Instruction* instr)
        : PDGLLVMNode(instr, NodeType::InstructionNode)
    {
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
        : PDGLLVMNode(arg, NodeType::FormalArgumentNode)
        , m_function(arg->getParent())
    {
    }

public:
    llvm::Function* getFunction() const
    {
        return m_function;
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

private:
    llvm::Function* m_function;
}; // class PDGArgumentNode

class PDGLLVMActualArgumentNode : public PDGLLVMNode
{
public:
    explicit PDGLLVMActualArgumentNode(llvm::CallSite& callSite, llvm::Value* actualArg)
        : PDGLLVMNode(actualArg, NodeType::ActualArgumentNode)
        , m_callSite(callSite)
    {
    }

public:
    const llvm::CallSite& getCallSite() const
    {
        return m_callSite;
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
        : PDGLLVMNode(var, NodeType::GlobalVariableNode)
    {
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
        : PDGLLVMNode(expr, NodeType::ConstantExprNode)
    {
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
        : PDGLLVMNode(constant, NodeType::ConstantNode)
    {
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
        : PDGLLVMNode(llvm::dyn_cast<llvm::Value>(block), NodeType::BasicBlockNode)
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

public:
    static bool classof(const PDGLLVMNode* node)
    {
        return node->getNodeType() == NodeType::BasicBlockNode;
    }

    static bool classof(const PDGNode* node)
    {
        return llvm::isa<PDGLLVMNode>(node) && classof(llvm::cast<PDGLLVMNode>(node));
    }

private:
    llvm::BasicBlock* m_block;
}; // class PDGLLVMBasicBlockNode

class PDGNullNode : public PDGLLVMNode
{
public:
    PDGNullNode()
        : PDGLLVMNode(nullptr, NodeType::NullNode)
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


class PDGPhiNode : public PDGLLVMNode
{
public:
    using Values = std::vector<llvm::Value*>;
    using Blocks = std::vector<llvm::BasicBlock*>;

public:
    PDGPhiNode(const Values& values, const Blocks& blocks)
        : PDGLLVMNode(nullptr, NodeType::PhiNode)
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

public:
    static bool classof(const PDGLLVMNode* node)
    {
        return node->getNodeType() == NodeType::PhiNode;
    }

    static bool classof(const PDGNode* node)
    {
        return llvm::isa<PDGLLVMNode>(node) && classof(llvm::cast<PDGLLVMNode>(node));
    }

private:
    Values m_values;
    Blocks m_blocks;
}; // class PDGPhiNode

} // namespace pdg

