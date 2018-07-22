#pragma once

#include <memory>
#include <unordered_set>

namespace pdg {

class PDGEdge;

class PDGNode
{
public:
    using PDGEdgeType = std::shared_ptr<PDGEdge>;
    using PDGEdges = std::unordered_set<PDGEdgeType>;
    using iterator = PDGEdges::iterator;
    using const_iterator = PDGEdges::const_iterator;

    enum NodeType : unsigned {
        InstructionNode,
        FormalArgumentNode,
        ActualArgumentNode,
        GlobalVariableNode,
        ConstantExprNode,
        ConstantNode,
        BasicBlockNode,
        NullNode,
        PhiSvfgNode,
        MssaPhiSvfgNode,
        LLVMMemoryPhiNode,
        UnknownNode
    };

public:
    PDGNode() = default;
    virtual ~PDGNode() = default;
    PDGNode(const PDGNode&) = delete;
    PDGNode(PDGNode&&) = delete;
    PDGNode& operator =(const PDGNode&) = delete;
    PDGNode& operator =(PDGNode&&) = delete;

public:
    virtual NodeType getNodeType() const = 0;

    const PDGEdges& getInEdges() const
    {
        return m_inEdges;
    }

    const PDGEdges& getOutEdges() const
    {
        return m_outEdges;
    }

    virtual bool addInEdge(PDGEdgeType inEdge)
    {
        return m_inEdges.insert(inEdge).second;
    }

    virtual bool addOutEdge(PDGEdgeType outEdge)
    {
        return m_outEdges.insert(outEdge).second;
    }

    virtual bool removeInEdge(PDGEdgeType inEdge)
    {
        m_inEdges.erase(inEdge);
    }

    virtual bool removeOutEdge(PDGEdgeType outEdge)
    {
        m_outEdges.erase(outEdge);
    }

public:
    iterator inEdgesBegin()
    {
        return m_inEdges.begin();
    }

    iterator inEdgesEnd()
    {
        return m_inEdges.end();
    }

    iterator outEdgesBegin()
    {
        return m_outEdges.begin();
    }

    iterator outEdgesEnd()
    {
        return m_outEdges.end();
    }

    const_iterator inEdgesBegin() const
    {
        return m_inEdges.begin();
    }

    const_iterator inEdgesEnd() const
    {
        return m_inEdges.end();
    }

    const_iterator outEdgesBegin() const
    {
        return m_outEdges.begin();
    }

    const_iterator outEdgesEnd() const
    {
        return m_outEdges.end();
    }

private:
    PDGEdges m_inEdges;
    PDGEdges m_outEdges;
}; // class PDGNode

} // namespace pdg

