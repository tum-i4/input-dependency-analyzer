#pragma once

#include "PDG/DefUseResults.h"

class SVFG;

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
    virtual PDGNodes getDefSites(llvm::Value* value) override;

private:
    SVFG* m_svfg;
}; // class SVFGDefUseAnalysisResults

} // namespace pdg

