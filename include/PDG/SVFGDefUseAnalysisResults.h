#pragma once

#include "PDG/DefUseResults.h"

class SVFG;
class SVFGNode;

namespace pdg {

class SVFGDefUseAnalysisResults : public DefUseResults
{
public:
    explicit SVFGDefUseAnalysisResults(SVFG* svfg);
    
    SVFGDefUseAnalysisResults(const SVFGDefUseAnalysisResults& ) = delete;
    SVFGDefUseAnalysisResults(SVFGDefUseAnalysisResults&& ) = delete;
    SVFGDefUseAnalysisResults& operator =(const SVFGDefUseAnalysisResults& ) = delete;
    SVFGDefUseAnalysisResults& operator =(SVFGDefUseAnalysisResults&& ) = delete;

public:
    virtual PDGNodeTy getDefSite(llvm::Value* value) override;

private:
    PDGNodeTy getNode(const SVFGNode* svfgNode);

private:
    SVFG* m_svfg;
}; // class SVFGDefUseAnalysisResults

} // namespace pdg

