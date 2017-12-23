#include "CachedFunctionAnalysisResult.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

CachedFunctionAnalysisResult::CachedFunctionAnalysisResult(llvm::Function* F)
    : m_F(F)
    , m_is_inputDep(false)
{
}

void CachedFunctionAnalysisResult::analize()
{
    // TODO: implement
}

llvm::Function* CachedFunctionAnalysisResult::getFunction()
{
    return m_F;
}

const llvm::Function* CachedFunctionAnalysisResult::getFunction() const
{
    return m_F;
}

bool CachedFunctionAnalysisResult::isInputDepFunction() const
{
    return m_is_inputDep;
}

void CachedFunctionAnalysisResult::setIsInputDepFunction(bool isInputDep)
{
    m_is_inputDep = isInputDep;
}

bool CachedFunctionAnalysisResult::isInputDependent(llvm::Instruction* instr) const
{
    return m_inputDepInstructions.find(instr) != m_inputDepInstructions.end();
}

bool CachedFunctionAnalysisResult::isInputDependent(const llvm::Instruction* instr) const
{
    return m_inputDepInstructions.find(const_cast<llvm::Instruction*>(instr)) != m_inputDepInstructions.end();
}

bool CachedFunctionAnalysisResult::isInputIndependent(llvm::Instruction* instr) const
{
    return m_inputIndepInstructions.find(instr) != m_inputIndepInstructions.end();
}

bool CachedFunctionAnalysisResult::isInputIndependent(const llvm::Instruction* instr) const
{
    return m_inputIndepInstructions.find(const_cast<llvm::Instruction*>(instr)) != m_inputIndepInstructions.end();
}

bool CachedFunctionAnalysisResult::isInputDependentBlock(llvm::BasicBlock* block) const
{
    return m_inputDepBlocks.find(block) != m_inputDepBlocks.end();
}

FunctionSet CachedFunctionAnalysisResult::getCallSitesData() const
{
    llvm::dbgs() << "CachedFunctionAnalysisResult has no information about call site data\n";
    return FunctionSet();
}

FunctionCallDepInfo CachedFunctionAnalysisResult::getFunctionCallDepInfo(llvm::Function* F) const
{
    llvm::dbgs() << "CachedFunctionAnalysisResult has no information about call dep info\n";
    return FunctionCallDepInfo();
}

long unsigned CachedFunctionAnalysisResult::get_input_dep_blocks_count() const
{
    return m_inputDepBlocks.size();
}

long unsigned CachedFunctionAnalysisResult::get_input_indep_blocks_count() const
{
    return m_inputInDepBlocks.size();
}

long unsigned CachedFunctionAnalysisResult::get_unreachable_blocks_count() const
{
    return m_unreachableBlocks.size();
}

long unsigned CachedFunctionAnalysisResult::get_unreachable_instructions_count() const
{
    return m_unreachableInstructions.size();
}

long unsigned CachedFunctionAnalysisResult::get_input_dep_count() const
{
    return m_inputDepInstructions.size();
}

long unsigned CachedFunctionAnalysisResult::get_input_indep_count() const
{
    return m_inputIndepInstructions.size();
}

long unsigned CachedFunctionAnalysisResult::get_input_unknowns_count() const
{
    return m_unknownInstructions.size();
}

}

