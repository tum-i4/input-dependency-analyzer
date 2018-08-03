#pragma once

#include "analysis/IndirectCallSiteResults.h"

class PTACallGraph;

namespace input_dependency {

class SVFGIndirectCallSiteResults : public IndirectCallSiteResults
{
public:
    using FunctionSet = IndirectCallSiteResults::FunctionSet;

public:
    explicit SVFGIndirectCallSiteResults(PTACallGraph* ptaGraph);

    virtual bool hasIndCSCallees(const llvm::CallSite& callSite) const override;
    virtual FunctionSet getIndCSCallees(const llvm::CallSite& callSite) override;

private:
    PTACallGraph* m_ptaGraph;
}; // class SVFGIndirectCallSiteResults

} // namespace input_dependency

