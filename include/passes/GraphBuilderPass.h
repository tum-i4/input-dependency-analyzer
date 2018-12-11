#pragma once

#include "llvm/Pass.h"

#include <memory>

namespace pdg {
class PDG;
}

namespace input_dependency {

class GraphBuilderPass : public llvm::ModulePass
{
public:
    static char ID;

    GraphBuilderPass()
        : llvm::ModulePass(ID)
    {
    }
    
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;

public:
    using PDGTy = std::shared_ptr<pdg::PDG>;
    PDGTy getPDG() const
    {
        return m_pdg;
    }

private:
    PDGTy m_pdg;
}; // class GraphBuilderPass

} // namespace input_dependency

