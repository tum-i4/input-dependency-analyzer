#pragma once

#include <memory>
#include "PDG.h"

namespace pdg {

class FunctionPDG;

class PDGBuilder
{
public:
    using PDGType = std::unique_ptr<PDG>;
    using FunctionPDGTy = PDG::FunctionPDGTy;

public:
    PDGBuilder() = default;

    ~PDGBuilder() = default;
    PDGBuilder(const PDGBuilder& ) = delete;
    PDGBuilder(PDGBuilder&& ) = delete;
    PDGBuilder& operator =(const PDGBuilder& ) = delete;
    PDGBuilder& operator =(PDGBuilder&& ) = delete;
 
    void build(llvm::Module* M);

public:
    PDGType getPDG()
    {
        return std::move(m_pdg);
    }

private:
    FunctionPDGTy buildFunctionPDG(llvm::Function* F);
    
private:
    PDGType m_pdg;
}; // class PDGBuilder

} // namespace pdg

