#pragma once

#include "PDG/DefUseResults.h"

#include <unordered_map>
#include <unordered_set>

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
    virtual llvm::Value* getDefSite(llvm::Value* value) override;
    virtual PDGNodeTy getDefSiteNode(llvm::Value* value) override;

private:
    SVFGNode* getSVFGNode(llvm::Value* value);
    std::unordered_set<SVFGNode*> getSVFGDefNodes(SVFGNode* svfgNode);
    llvm::Value* getSVFGNodeValue(SVFGNode* svfgNode);
    PDGNodeTy getNode(const std::unordered_set<SVFGNode*>& svfgNodes);
    PDGNodeTy getNode(SVFGNode* svfgNode);

private:
    SVFG* m_svfg;
    std::unordered_map<unsigned, PDGNodeTy> m_phiNodes;
}; // class SVFGDefUseAnalysisResults

} // namespace pdg

