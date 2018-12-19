#pragma once

#include <memory>

#include "analysis/ReachabilityAnalysis.h"

namespace pdg {

class FunctionPDG;
} // namespace pdg

namespace input_dependency {

class ArgumentReachabilityAnalysis : public ReachabilityAnalysis
{
public:
    using FunctionPDGType = std::shared_ptr<pdg::FunctionPDG>;

public:
    ArgumentReachabilityAnalysis(FunctionPDGType functionPDG);

    ArgumentReachabilityAnalysis(const ArgumentReachabilityAnalysis& ) = delete;
    ArgumentReachabilityAnalysis(ArgumentReachabilityAnalysis&& ) = delete;
    ArgumentReachabilityAnalysis& operator =(const ArgumentReachabilityAnalysis& ) = delete;
    ArgumentReachabilityAnalysis& operator =(ArgumentReachabilityAnalysis&& ) = delete;

public:
    void analyze() override;

private:
   FunctionPDGType m_functionPDG; 
}; // class ArgumentReachabilityAnalysis

} // namespace input_dependency

