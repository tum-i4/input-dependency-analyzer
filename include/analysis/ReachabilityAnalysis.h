#pragma once

#include <memory>
#include <functional>
#include <unordered_set>

namespace pdg {

class PDG;
class PDGNode;

} // namespace pdg

namespace input_dependency {

class ReachabilityAnalysis
{
public:
    using NodeType = std::shared_ptr<pdg::PDGNode>;
    using NodeSet = std::unordered_set<NodeType>;
    using ReachCallback = std::function<void (NodeType source, NodeType dest, bool isDataDep)>;
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
                                      ReachabilityAnalysis::NodeType node2,
                                      bool isDataDep);

protected:
    void analyze(NodeType node, const ReachCallback& callback, NodeSet& processedNodes);

protected:
    NodeProcessor m_nodeProcessor;
};

} // namespace input_dependency

