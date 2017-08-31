#pragma once

#include "llvm/Pass.h"

#include <unordered_set>

namespace oh {

/**
* \class FunctionExtractionPass
* \brief Transformation pass to extract input dependent snippets of a function into separate function.
* Runs only for functions that are not input dependent, saying all call sites are from deterministic locations.
* Collects all functions that have been extracted as a result of the pass
*/
class FunctionExtractionPass : public llvm::ModulePass
{
public:
    static char ID;

    FunctionExtractionPass()
        : llvm::ModulePass(ID)
    {
    }

public:
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;

private:
    std::unordered_set<llvm::Function*> m_extracted_functions;
};

} // namespace oh

