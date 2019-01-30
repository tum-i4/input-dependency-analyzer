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
    virtual PDGNodeTy createFunctionNodeFor(llvm::Function* F) override;
    virtual PDGNodeTy createGlobalNodeFor(llvm::GlobalVariable* global) override;
    virtual PDGNodeTy createFormalArgNodeFor(llvm::Argument* arg) override;
    virtual PDGNodeTy createActualArgumentNode(llvm::CallSite& callSite,
                                               llvm::Value* arg,
                                               unsigned idx) override;
    virtual PDGNodeTy createNullNode() override;
    virtual PDGNodeTy createConstantNodeFor(llvm::Constant* constant) override;
    virtual PDGNodeTy createVaArgNodeFor(llvm::Function* F) override;

}; // class GraphBuilder

} // namespace input_dependency

