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

public:
    ReachabilityAnalysis() = default;

    ReachabilityAnalysis(const ReachabilityAnalysis& ) = delete;
    ReachabilityAnalysis(ReachabilityAnalysis&& ) = delete;
    ReachabilityAnalysis& operator =(const ReachabilityAnalysis& ) = delete;
    ReachabilityAnalysis& operator =(ReachabilityAnalysis&& ) = delete;

public:
    virtual void analyze() = 0;

    static void propagateDependencies(ReachabilityAnalysis::NodeType node1,
                                      ReachabilityAnalysis::NodeType node2);

protected:
    void analyze(NodeType node, const ReachCallback& callback);
};

} // namespace input_dependency

