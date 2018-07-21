#pragma once

#include "PDGNode.h"
#include "SVF/MSSA/SVFGNode.h"

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
};


} // namespace pdg

