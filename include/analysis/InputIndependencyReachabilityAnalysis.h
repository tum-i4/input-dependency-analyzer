#pragma once

namespace pdg {
class PDG;
}

namespace input_dependency {

class InputIndependencyReachabilityAnalysis
{
public:
    InputIndependencyReachabilityAnalysis(pdg::PDG* pdg);

    InputIndependencyReachabilityAnalysis(const InputIndependencyReachabilityAnalysis& ) = delete;
    InputIndependencyReachabilityAnalysis(InputIndependencyReachabilityAnalysis&& ) = delete;
    InputIndependencyReachabilityAnalysis& operator =(const InputIndependencyReachabilityAnalysis& ) = delete;
    InputIndependencyReachabilityAnalysis& operator =(InputIndependencyReachabilityAnalysis&& ) = delete;

public:
    void analyze();

private:
    pdg::PDG* m_pdg;
}; // class InputIndependencyReachabilityAnalysis

}

