#pragma once

#include "PDG/PDG/PDGBuilder.h"

namespace input_dependency {

class GraphBuilder : public pdg::PDGBuilder
{
public:
    GraphBuilder(llvm::Module* M)
        : pdg::PDGBuilder(M)
    {
    }

protected:
    virtual PDGNodeTy createInstructionNodeFor(llvm::Instruction* instr) override;
    virtual PDGNodeTy createBasicBlockNodeFor(llvm::BasicBlock* block) override;
    virtual PDGNodeTy createGlobalNodeFor(llvm::GlobalVariable* global) override;
    virtual PDGNodeTy createFormalArgNodeFor(llvm::Argument* arg) override;
    virtual PDGNodeTy createNullNode() override;
    virtual PDGNodeTy createConstantNodeFor(llvm::Constant* constant) override;

}; // class GraphBuilder

} // namespace input_dependency

