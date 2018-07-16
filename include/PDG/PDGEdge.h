#pragma once

#include <memory>

#include "PDGNode.h"

namespace pdg {

class PDGEdge {
public:
    using PDGNodeTy = std::shared_ptr<PDGNode>;

public:
    PDGEdge(PDGNodeTy sourceNode, PDGNodeTy destNode)
        : m_source(sourceNode)
        , m_dest(destNode)
    {
    }

    virtual ~PDGEdge() = default;
    PDGEdge(const PDGEdge&) = delete;
    PDGEdge(PDGEdge&&) = delete;
    PDGEdge& operator =(const PDGEdge&) = delete;
    PDGEdge& operator =(PDGEdge&&) = delete;

public:
    const PDGNodeTy getSource() const
    {
        return m_source;
    }

    PDGNodeTy getSource()
    {
        return m_source;
    }

    const PDGNodeTy getDestination() const
    {
        return m_dest;
    }

    PDGNodeTy getDestination()
    {
        return m_dest;
    }

private:
    PDGNodeTy m_source;
    PDGNodeTy m_dest;
}; // class PDGEdge

} // namespace pdg

