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
    using GraphType = std::shared_ptr<pdg::PDG>;
    using NodeType = std::shared_ptr<pdg::PDGNode>;
    using ReachCallback = std::function<void (NodeType source, NodeType dest)>;

public:
    ReachabilityAnalysis() = default;
    ReachabilityAnalysis(const GraphType& graph);

    ReachabilityAnalysis(const ReachabilityAnalysis& ) = delete;
    ReachabilityAnalysis(ReachabilityAnalysis&& ) = delete;
    ReachabilityAnalysis& operator =(const ReachabilityAnalysis& ) = delete;
    ReachabilityAnalysis& operator =(ReachabilityAnalysis&& ) = delete;

public:
    void analyze(NodeType node, const ReachCallback& callback);
    
private:
    // TODO: remove if not used
    GraphType m_graph;
};

} // namespace input_dependency

