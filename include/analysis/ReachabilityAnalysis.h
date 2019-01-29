#pragma once

#include <memory>
#include <functional>

namespace pdg {

class PDG;
class PDGNode;

} // namespace pdg

namespace input_dependency {

class ReachabilityAnalysis
{
public:
    using NodeType = std::shared_ptr<pdg::PDGNode>;
    using ReachCallback = std::function<void (NodeType source, NodeType dest)>;
    using NodeProcessor = std::function<void (NodeType node)>;

public:
    ReachabilityAnalysis();

    ReachabilityAnalysis(const ReachabilityAnalysis& ) = delete;
    ReachabilityAnalysis(ReachabilityAnalysis&& ) = delete;
    ReachabilityAnalysis& operator =(const ReachabilityAnalysis& ) = delete;
    ReachabilityAnalysis& operator =(ReachabilityAnalysis&& ) = delete;

public:
    void setNodeProcessor(const NodeProcessor& nodeProcessor);
    virtual void analyze() = 0;

public:
    static void propagateDependencies(ReachabilityAnalysis::NodeType node1,
                                      ReachabilityAnalysis::NodeType node2);

protected:
    void analyze(NodeType node, const ReachCallback& callback);

protected:
    NodeProcessor m_nodeProcessor;
};

} // namespace input_dependency

