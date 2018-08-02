#pragma once

#include "PDG/DefUseResults.h"

class SVFG;
class PointerAnalysis;

namespace pdg {

class SVFGDefUseAnalysisResults : public DefUseResults
{
public:
    SVFGDefUseAnalysisResults(SVFG* svfg, PointerAnalysis* pta);
    
    SVFGDefUseAnalysisResults(const SVFGDefUseAnalysisResults& ) = delete;
    SVFGDefUseAnalysisResults(SVFGDefUseAnalysisResults&& ) = delete;
    SVFGDefUseAnalysisResults& operator =(const SVFGDefUseAnalysisResults& ) = delete;
    SVFGDefUseAnalysisResults& operator =(SVFGDefUseAnalysisResults&& ) = delete;

public:
    virtual PDGNodeTy getDefSite(llvm::Value* value) override;
    virtual PDGNodes getDefSites(llvm::Value* value) override;
    virtual bool hasIndCSCallees(const llvm::CallSite& callSite) const override;
    virtual FunctionSet getIndCSCallees(const llvm::CallSite& callSite) override;

private:
    SVFG* m_svfg;
    PointerAnalysis* m_pta;
}; // class SVFGDefUseAnalysisResults

} // namespace pdg

