#include "analysis/SVFGIndirectCallSiteResults.h"

#include "SVF/Util/PTACallGraph.h"

namespace input_dependency {

SVFGIndirectCallSiteResults::SVFGIndirectCallSiteResults(PTACallGraph* ptaGraph)
    : m_ptaGraph(ptaGraph)
{
}

bool SVFGIndirectCallSiteResults::hasIndCSCallees(const llvm::CallSite& callSite) const
{
    return m_ptaGraph->hasIndCSCallees(callSite);
}

SVFGIndirectCallSiteResults::FunctionSet SVFGIndirectCallSiteResults::getIndCSCallees(const llvm::CallSite& callSite)
{
    FunctionSet callees;
    const auto& ptaCallees = m_ptaGraph->getIndCSCallees(callSite);
    for (auto& F : ptaCallees) {
        callees.insert(const_cast<llvm::Function*>(F));
    }
    return callees;
}

}

