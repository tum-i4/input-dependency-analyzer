#include "PDG/PDGBuilder.h"

#include "PDG/FunctionPDG.h"
#include "PDG/PDGEdge.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"

namespace pdg {

void PDGBuilder::build(llvm::Module* M)
{
    // TODO: implement
}

PDGBuilder::FunctionPDGTy PDGBuilder::buildFunctionPDG(llvm::Function* F)
{
    // TODO: implement
}

} // namespace pdg

