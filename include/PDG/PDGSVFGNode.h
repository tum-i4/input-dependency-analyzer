#pragma once

#include "PDGNode.h"
#include "SVF/MSSA/SVFGNode.h"
#include "llvm/Support/Casting.h"

namespace pdg {

class PDGSVFGNode : public PDGNode
{
public:
    explicit PDGSVFGNode(SVFGNode* node)
        : m_svfgNode(node)
    {
    }
    
    NodeType getNodeType() const override
    {
        return NodeType::UnknownNode;
    }
    
    SVFGNode* getSVFGNode() const
    {
        return m_svfgNode;
    }

public:
    static bool isSVFGNodeType(NodeType nodeType)
    {
        return nodeType == NodeType::UnknownNode
            || (nodeType >= NodeType::PhiSvfgNode && nodeType <= NodeType::MssaPhiSvfgNode);
    }

    static bool classof(const PDGNode* node)
    {
        return isSVFGNodeType(node->getNodeType());
    }

private:
    SVFGNode* m_svfgNode;
};

class PDGPHISVFGNode : public PDGSVFGNode
{
public:
    explicit PDGPHISVFGNode(PHISVFGNode* node)
        : PDGSVFGNode(node)
    {
    }

    NodeType getNodeType() const override
    {
        return NodeType::PhiSvfgNode;
    }

public:
    static bool classof(const PDGSVFGNode* node)
    {
        return node->getNodeType() == NodeType::PhiSvfgNode;
    }

    static bool classof(const PDGNode* node)
    {
        return llvm::isa<PDGSVFGNode>(node) && classof(llvm::cast<PDGSVFGNode>(node));
    }
};

class PDGMSSAPHISVFGNode : public PDGSVFGNode
{
public:
    explicit PDGMSSAPHISVFGNode(MSSAPHISVFGNode* node)
        : PDGSVFGNode(node)
    {
    }

    NodeType getNodeType() const override
    {
        return NodeType::MssaPhiSvfgNode;
    }

public:
    static bool classof(const PDGSVFGNode* node)
    {
        return node->getNodeType() == NodeType::MssaPhiSvfgNode;
    }

    static bool classof(const PDGNode* node)
    {
        return llvm::isa<PDGSVFGNode>(node) && classof(llvm::cast<PDGSVFGNode>(node));
    }
};


} // namespace pdg

