#include "CachedFunctionAnalysisResult.h"

#include "constants.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

CachedFunctionAnalysisResult::CachedFunctionAnalysisResult(llvm::Function* F)
    : m_F(F)
    , m_is_inputDep(false)
    , m_is_extracted(false)
    , m_dataIndepInstrCount(0)
{
}

void CachedFunctionAnalysisResult::analyze()
{
    parse_function_input_dep_metadata();
    parse_function_extracted_metadata();
    for (auto& B : *m_F) {
        parse_block_input_dep_metadata(B);
        parse_block_instructions_input_dep_metadata(B);
    }
    m_dataIndepInstrCount += get_input_indep_count();
    for (auto& I : m_controlDepInstructions) {
        if (!isDataDependent(I)) {
            ++m_dataIndepInstrCount;
        }
    }
}

void CachedFunctionAnalysisResult::parse_function_input_dep_metadata()
{
    if (auto* input_dep_function_md = m_F->getMetadata(metadata_strings::input_dep_function)) {
        m_is_inputDep = true;
    }
    // no need to look for input indep md
}

 void CachedFunctionAnalysisResult::parse_function_extracted_metadata()
 {
   if (auto* extr_function_md = m_F->getMetadata(metadata_strings::extracted)) {
        m_is_extracted = true;
    }
 }

void CachedFunctionAnalysisResult::parse_block_input_dep_metadata(llvm::BasicBlock& B)
{
    const llvm::Instruction& first_instr = *B.begin();
    if (auto* input_dep_block = first_instr.getMetadata(metadata_strings::input_dep_block)) {
        m_inputDepBlocks.insert(&B);
    } else if (auto* input_indep_block = first_instr.getMetadata(metadata_strings::input_indep_block)) {
        m_inputInDepBlocks.insert(&B);
    } else if (auto* unreachable_block = first_instr.getMetadata(metadata_strings::unreachable)) {
        m_unreachableBlocks.insert(&B);
    } else {
        llvm::dbgs() << "No input dependency metadata for block "
                     << B.getName() << " in function " << B.getParent()->getName() << "\n";
        llvm::dbgs() << "Mark input dependent\n";
        m_inputDepBlocks.insert(&B);
    }
}

void CachedFunctionAnalysisResult::parse_block_instructions_input_dep_metadata(llvm::BasicBlock& B)
{
    if (m_unreachableBlocks.find(&B) != m_unreachableBlocks.end()) {
        add_all_instructions_to(B, m_unreachableInstructions);
        return;
    }
    bool is_block_input_dep = (m_inputDepBlocks.find(&B) != m_inputDepBlocks.end());
    for (auto& I : B) {
        parse_instruction_input_dep_metadata(I, is_block_input_dep);
    }
}

void CachedFunctionAnalysisResult::add_all_instructions_to(llvm::BasicBlock& B, Instructions& instructions)
{
    for (auto& I : B) {
        instructions.insert(&I);
    }
}

void CachedFunctionAnalysisResult::parse_instruction_input_dep_metadata(llvm::Instruction& I, bool is_block_input_dep)
{
    if (is_block_input_dep) {
        m_inputDepInstructions.insert(&I);
    }
    if (I.getMetadata(metadata_strings::input_dep_instr)) {
        m_inputDepInstructions.insert(&I);
    }
    if (I.getMetadata(metadata_strings::control_dep_instr)) {
        m_controlDepInstructions.insert(&I);
    }
    if (I.getMetadata(metadata_strings::data_dep_instr)) {
        m_dataDepInstructions.insert(&I);
    } else if (I.getMetadata(metadata_strings::input_indep_instr)) {
        m_inputIndepInstructions.insert(&I);
    } else if (I.getMetadata(metadata_strings::unknown)) {
        m_unknownInstructions.insert(&I);
    }
    if (I.getMetadata(metadata_strings::global_dep_instr)) {
        m_globalDepInstructions.insert(&I);
    }
    if (I.getMetadata(metadata_strings::argument_dep_instr)) {
        m_argumentDepInstructions.insert(&I);
    }
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

bool CachedFunctionAnalysisResult::isExtractedFunction() const
{
    return m_is_extracted;
}

void CachedFunctionAnalysisResult::setIsExtractedFunction(bool isExtracted)
{
    m_is_extracted = isExtracted;
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

bool CachedFunctionAnalysisResult::isControlDependent(llvm::Instruction* I) const
{
    return m_controlDepInstructions.find(I) != m_controlDepInstructions.end();
}

bool CachedFunctionAnalysisResult::isDataDependent(llvm::Instruction* I) const
{
    return m_dataDepInstructions.find(I) != m_dataDepInstructions.end();
}

bool CachedFunctionAnalysisResult::isArgumentDependent(llvm::Instruction* I) const
{
    return m_argumentDepInstructions.find(I) != m_argumentDepInstructions.end();
}

bool CachedFunctionAnalysisResult::isArgumentDependent(llvm::BasicBlock* block) const
{
    return false;
}

bool CachedFunctionAnalysisResult::isGlobalDependent(llvm::Instruction* I) const
{
    return m_globalDepInstructions.find(I) != m_globalDepInstructions.end();
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

long unsigned CachedFunctionAnalysisResult::get_data_indep_count() const
{
    return m_dataIndepInstrCount;
}

long unsigned CachedFunctionAnalysisResult::get_input_unknowns_count() const
{
    return m_unknownInstructions.size();
}

}

