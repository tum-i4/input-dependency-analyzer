#pragma once

#include <memory>

#include "analysis/ReachabilityAnalysis.h"

namespace pdg {

class FunctionPDG;
} // namespace pdg

namespace input_dependency {

class InputDependencyReachabilityAnalysis : public ReachabilityAnalysis
{
public:
    using GraphType = std::shared_ptr<pdg::PDG>;

public:
    InputDependencyReachabilityAnalysis(GraphType pdg);

    InputDependencyReachabilityAnalysis(const InputDependencyReachabilityAnalysis& ) = delete;
    InputDependencyReachabilityAnalysis(InputDependencyReachabilityAnalysis&& ) = delete;
    InputDependencyReachabilityAnalysis& operator =(const InputDependencyReachabilityAnalysis& ) = delete;
    InputDependencyReachabilityAnalysis& operator =(InputDependencyReachabilityAnalysis&& ) = delete;

public:
    void analyze() override;

private:
   GraphType m_pdg; 
}; // class InputDependencyReachabilityAnalysis

} // namespace input_dependency

