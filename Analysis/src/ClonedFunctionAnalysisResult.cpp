#include "input-dependency/Analysis/ClonedFunctionAnalysisResult.h"

#include "input-dependency/Analysis/BasicBlocksUtils.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

ClonedFunctionAnalysisResult::ClonedFunctionAnalysisResult(llvm::Function* F)
    : m_F(F)
    , m_is_inputDep(false)
    , m_is_extracted(false)
    , m_instructionsCount(0)
    , m_dataIndepInstrsCount(0)
{
    for (auto& B : *m_F) {
        m_instructionsCount += B.getInstList().size();
        for (auto& I : B) {
            if (!isDataDependent(&I)) {
                ++m_dataIndepInstrsCount;
            }
        }
    }
}

void ClonedFunctionAnalysisResult::setInputDepInstrs(InstrSet&& inputDeps)
{
    m_inputDependentInstrs = std::move(inputDeps);
}

void ClonedFunctionAnalysisResult::setInputIndepInstrs(InstrSet&& inputIndeps)
{
    m_inputIndependentInstrs = std::move(inputIndeps);
}

void ClonedFunctionAnalysisResult::setDataDependentInstrs(InstrSet&& dataDeps)
{
    m_dataDependentInstrs = std::move(dataDeps);
}

void ClonedFunctionAnalysisResult::setArgumentDependentInstrs(InstrSet&& argumentDeps)
{
    m_argumentDependentInstrs = std::move(argumentDeps);
}

void ClonedFunctionAnalysisResult::setGlobalDependentInstrs(InstrSet&& globalDeps)
{
    m_globalDependentInstrs = std::move(globalDeps);
}

void ClonedFunctionAnalysisResult::setInputDependentBasicBlocks(std::unordered_set<llvm::BasicBlock*>&& inputDeps)
{
    m_inputDependentBasicBlocks = std::move(inputDeps);
}

void ClonedFunctionAnalysisResult::setArgumentDependentBasicBlocks(std::unordered_set<llvm::BasicBlock*>&& argDeps)
{
    m_argumentDependentBasicBlocks = std::move(argDeps);
}

void ClonedFunctionAnalysisResult::setCalledFunctions(const FunctionSet& calledFunctions)
{
    m_calledFunctions = calledFunctions;
}

void ClonedFunctionAnalysisResult::setFunctionCallDepInfo(std::unordered_map<llvm::Function*, FunctionCallDepInfo>&& callDepInfo)
{
    m_functionCallDepInfo = std::move(callDepInfo);
}

llvm::Function* ClonedFunctionAnalysisResult::getFunction()
{
    return m_F;
}

const llvm::Function* ClonedFunctionAnalysisResult::getFunction() const
{
    return m_F;
}

bool ClonedFunctionAnalysisResult::isInputDepFunction() const
{
    return m_is_inputDep;
}

void ClonedFunctionAnalysisResult::setIsInputDepFunction(bool isInputDep)
{
    m_is_inputDep = isInputDep;
}

bool ClonedFunctionAnalysisResult::isExtractedFunction() const
{
    return m_is_extracted;
}

void ClonedFunctionAnalysisResult::setIsExtractedFunction(bool isExtracted)
{
    m_is_extracted = isExtracted;
}

bool ClonedFunctionAnalysisResult::isInputDependent(llvm::Instruction* instr) const
{
    return m_inputDependentInstrs.find(instr) != m_inputDependentInstrs.end();
}

bool ClonedFunctionAnalysisResult::isInputDependent(const llvm::Instruction* instr) const
{
    return m_inputDependentInstrs.find(const_cast<llvm::Instruction*>(instr)) != m_inputDependentInstrs.end();
}

bool ClonedFunctionAnalysisResult::isInputIndependent(llvm::Instruction* instr) const
{
    return m_inputIndependentInstrs.find(instr) != m_inputIndependentInstrs.end();
}

bool ClonedFunctionAnalysisResult::isInputIndependent(const llvm::Instruction* instr) const
{
    return m_inputIndependentInstrs.find(const_cast<llvm::Instruction*>(instr)) != m_inputIndependentInstrs.end();
}

bool ClonedFunctionAnalysisResult::isInputDependentBlock(llvm::BasicBlock* block) const
{
    return m_inputDependentBasicBlocks.find(block) != m_inputDependentBasicBlocks.end();
}

bool ClonedFunctionAnalysisResult::isControlDependent(llvm::Instruction* I) const
{
    return isInputDependentBlock(I->getParent());
}

bool ClonedFunctionAnalysisResult::isDataDependent(llvm::Instruction* I) const
{
    return m_dataDependentInstrs.find(I) != m_dataDependentInstrs.end();
}

bool ClonedFunctionAnalysisResult::isArgumentDependent(llvm::Instruction* I) const
{
    return m_argumentDependentInstrs.find(I) != m_argumentDependentInstrs.end();
}

bool ClonedFunctionAnalysisResult::isArgumentDependent(llvm::BasicBlock* block) const
{
    return m_argumentDependentBasicBlocks.find(block) != m_argumentDependentBasicBlocks.end();
}
   
bool ClonedFunctionAnalysisResult::isGlobalDependent(llvm::Instruction* I) const
{
    return m_globalDependentInstrs.find(I) != m_globalDependentInstrs.end();
}

FunctionSet ClonedFunctionAnalysisResult::getCallSitesData() const
{
    return m_calledFunctions;
}

FunctionCallDepInfo ClonedFunctionAnalysisResult::getFunctionCallDepInfo(llvm::Function* F) const
{
    auto pos = m_functionCallDepInfo.find(F);
    if (pos == m_functionCallDepInfo.end()) {
        return FunctionCallDepInfo();
    }
    return pos->second;
}

bool ClonedFunctionAnalysisResult::changeFunctionCall(const llvm::Instruction* callInstr, llvm::Function* oldF, llvm::Function* newF)
{
    llvm::Instruction* instr = const_cast<llvm::Instruction*>(callInstr);
    if (auto call = llvm::dyn_cast<llvm::CallInst>(instr)) {
        call->setCalledFunction(newF);
    } else if (auto invoke = llvm::dyn_cast<llvm::InvokeInst>(instr)) {
        invoke->setCalledFunction(newF);
    } else {
        assert(false);
    }
    auto callDepInfo_pos = m_functionCallDepInfo.find(oldF);
    if (callDepInfo_pos == m_functionCallDepInfo.end()) {
        //llvm::dbgs() << "No call of function " << oldF->getName() << " in function " << m_F->getName() << "\n";
        return false;
    }
    auto& callDepInfo = callDepInfo_pos->second;
    const FunctionCallDepInfo::ArgumentDependenciesMap&  calledArgDepMap = callDepInfo.getArgumentsDependencies(instr);
    const FunctionCallDepInfo::GlobalVariableDependencyMap& globalsDeps = callDepInfo.getGlobalsDependencies(instr);
    FunctionCallDepInfo newCallDepInfo(*newF);
    newCallDepInfo.addCall(instr, calledArgDepMap);
    newCallDepInfo.addCall(instr, globalsDeps);
    m_functionCallDepInfo.insert(std::make_pair(newF, newCallDepInfo));
    callDepInfo.removeCall(instr);
    if (callDepInfo.empty()) {
        m_functionCallDepInfo.erase(callDepInfo_pos);
    }
    m_calledFunctions.insert(newF);
    if (m_functionCallDepInfo.find(oldF) == m_functionCallDepInfo.end()) {
        m_calledFunctions.erase(oldF);
    }
    // do we need to remove old F? would need to remove from all blocks. why don't keep info here in this class
    return true;
}

long unsigned ClonedFunctionAnalysisResult::get_input_dep_blocks_count() const
{
    return m_inputDependentBasicBlocks.size();
}

long unsigned ClonedFunctionAnalysisResult::get_input_indep_blocks_count() const
{
    return m_F->getBasicBlockList().size() - get_input_dep_blocks_count();
}

long unsigned ClonedFunctionAnalysisResult::get_unreachable_blocks_count() const
{
    return BasicBlocksUtils::get().getFunctionUnreachableBlocksCount(m_F);
}

long unsigned ClonedFunctionAnalysisResult::get_unreachable_instructions_count() const
{
    return BasicBlocksUtils::get().getFunctionUnreachableInstructionsCount(m_F);
}

long unsigned ClonedFunctionAnalysisResult::get_input_dep_count() const
{
    return m_inputDependentInstrs.size();
}

long unsigned ClonedFunctionAnalysisResult::get_input_indep_count() const
{
    return m_inputIndependentInstrs.size();
}

long unsigned ClonedFunctionAnalysisResult::get_data_indep_count() const
{
    return m_dataIndepInstrsCount;
}

long unsigned ClonedFunctionAnalysisResult::get_input_unknowns_count() const
{
    return m_instructionsCount - get_input_dep_count() - get_input_indep_count();
}


} // namespace input_dependency

