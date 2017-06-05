#include "InputDependentBasicBlockAnaliser.h"

#include "VirtualCallSitesAnalysis.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

namespace input_dependency {

InputDependentBasicBlockAnaliser::InputDependentBasicBlockAnaliser(llvm::Function* F,
                                                                   llvm::AAResults& AAR,
                                                                   const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                                                   const Arguments& inputs,
                                                                   const FunctionAnalysisGetter& Fgetter,
                                                                   llvm::BasicBlock* BB)
                    : BasicBlockAnalysisResult(F, AAR, virtualCallsInfo, inputs, Fgetter, BB)
{
}

DepInfo InputDependentBasicBlockAnaliser::getInstructionDependencies(llvm::Instruction* instr)
{
    return DepInfo(DepInfo::INPUT_DEP);
}

DepInfo InputDependentBasicBlockAnaliser::getValueDependencies(llvm::Value* value)
{
    return DepInfo(DepInfo::INPUT_DEP);
}

ReflectingInputDependentBasicBlockAnaliser::ReflectingInputDependentBasicBlockAnaliser(llvm::Function* F,
                                                                   llvm::AAResults& AAR,
                                                                   const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                                                   const Arguments& inputs,
                                                                   const FunctionAnalysisGetter& Fgetter,
                                                                   llvm::BasicBlock* BB)
                    : InputDependentBasicBlockAnaliser(F, AAR, virtualCallsInfo, inputs, Fgetter, BB)
{
}


}


