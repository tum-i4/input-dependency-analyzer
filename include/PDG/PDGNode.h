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

    const PDGEdges& getInDataEdges() const
    {
        return m_inDataEdges;
    }

    const PDGEdges& getOutDataEdges() const
    {
        return m_outDataEdges;
    }

    const PDGEdges& getInControlEdges() const
    {
        return m_inControlEdges;
    }

    const PDGEdges& getOutControlEdges() const
    {
        return m_outControlEdges;
    }

    virtual bool addInDataEdge(PDGEdgeType inEdge)
    {
        return m_inDataEdges.insert(inEdge).second;
    }

    virtual bool addOutDataEdge(PDGEdgeType outEdge)
    {
        return m_outDataEdges.insert(outEdge).second;
    }

    virtual bool addInControlEdge(PDGEdgeType inEdge)
    {
        return m_inControlEdges.insert(inEdge).second;
    }

    virtual bool addOutControlEdge(PDGEdgeType outEdge)
    {
        return m_outControlEdges.insert(outEdge).second;
    }

    virtual bool removeInDataEdge(PDGEdgeType inEdge)
    {
        m_inDataEdges.erase(inEdge);
    }

    virtual bool removeOutDataEdge(PDGEdgeType outEdge)
    {
        m_outDataEdges.erase(outEdge);
    }

    virtual bool removeInControlEdge(PDGEdgeType inEdge)
    {
        m_inControlEdges.erase(inEdge);
    }

    virtual bool removeOutControlEdge(PDGEdgeType outEdge)
    {
        m_outControlEdges.erase(outEdge);
    }

public:
    iterator inDataEdgesBegin()
    {
        return m_inDataEdges.begin();
    }

    iterator inDataEdgesEnd()
    {
        return m_inDataEdges.end();
    }

    iterator outDataEdgesBegin()
    {
        return m_outDataEdges.begin();
    }

    iterator outDataEdgesEnd()
    {
        return m_outDataEdges.end();
    }

    iterator inControlEdgesBegin()
    {
        return m_inControlEdges.begin();
    }

    iterator inControlEdgesEnd()
    {
        return m_inControlEdges.end();
    }

    iterator outControlEdgesBegin()
    {
        return m_outControlEdges.begin();
    }

    iterator outControlEdgesEnd()
    {
        return m_outControlEdges.end();
    }

    const_iterator inDataEdgesBegin() const
    {
        return m_inDataEdges.begin();
    }

    const_iterator inDataEdgesEnd() const
    {
        return m_inDataEdges.end();
    }

    const_iterator outDataEdgesBegin() const
    {
        return m_outDataEdges.begin();
    }

    const_iterator outDataEdgesEnd() const
    {
        return m_outDataEdges.end();
    }

    const_iterator inControlEdgesBegin() const
    {
        return m_inControlEdges.begin();
    }

    const_iterator inControlEdgesEnd() const
    {
        return m_inControlEdges.end();
    }

    const_iterator outControlEdgesBegin() const
    {
        return m_outControlEdges.begin();
    }

    const_iterator outControlEdgesEnd() const
    {
        return m_outControlEdges.end();
    }

private:
    PDGEdges m_inDataEdges;
    PDGEdges m_outDataEdges;
    PDGEdges m_inControlEdges;
    PDGEdges m_outControlEdges;
}; // class PDGNode

} // namespace pdg

