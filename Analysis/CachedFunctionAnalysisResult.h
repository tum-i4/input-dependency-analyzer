#pragma once

#include "FunctionInputDependencyResultInterface.h"

#include <unordered_set>

namespace llvm {
class Function;
class BasicBlock;
class Instruction;
}

namespace input_dependency {

class CachedFunctionAnalysisResult final : public FunctionInputDependencyResultInterface
{
public:
    using BasicBlocks = std::unordered_set<llvm::BasicBlock*>;
    using Instructions = std::unordered_set<llvm::Instruction*>;

public:
    CachedFunctionAnalysisResult(llvm::Function* F);

    void analyze() override;
    
public:
    llvm::Function* getFunction() override;
    const llvm::Function* getFunction() const override;
    bool isInputDepFunction() const override;
    void setIsInputDepFunction(bool isInputDep) override;
    bool isExtractedFunction() const override;
    void setIsExtractedFunction(bool isExtracted) override;
    bool isInputDependent(llvm::Instruction* instr) const override;
    bool isInputDependent(const llvm::Instruction* instr) const override;
    bool isInputIndependent(llvm::Instruction* instr) const override;
    bool isInputIndependent(const llvm::Instruction* instr) const override;
    bool isInputDependentBlock(llvm::BasicBlock* block) const override;
    bool isControlDependent(llvm::Instruction* I) const override;
    bool isDataDependent(llvm::Instruction* I) const override;
    bool isArgumentDependent(llvm::Instruction* I) const override;
    bool isArgumentDependent(llvm::BasicBlock* block) const override;
    bool isGlobalDependent(llvm::Instruction* I) const override;

    FunctionSet getCallSitesData() const override;
    FunctionCallDepInfo getFunctionCallDepInfo(llvm::Function* F) const override;

    long unsigned get_input_dep_blocks_count() const override;
    long unsigned get_input_indep_blocks_count() const override;
    long unsigned get_unreachable_blocks_count() const override;
    long unsigned get_unreachable_instructions_count() const override;
    long unsigned get_input_dep_count() const override;
    long unsigned get_input_indep_count() const override;
    long unsigned get_data_indep_count() const override;
    long unsigned get_input_unknowns_count() const override;

private:
    void parse_function_input_dep_metadata();
    void parse_function_extracted_metadata();
    void parse_block_input_dep_metadata(llvm::BasicBlock& B);
    void parse_block_instructions_input_dep_metadata(llvm::BasicBlock& B);
    void add_all_instructions_to(llvm::BasicBlock& B, Instructions& instructions);
    void parse_instruction_input_dep_metadata(llvm::Instruction& I);

private:
    llvm::Function* m_F;
    bool m_is_inputDep;
    bool m_is_extracted;
    BasicBlocks m_inputDepBlocks;
    BasicBlocks m_inputInDepBlocks;
    BasicBlocks m_unreachableBlocks;
    Instructions m_inputDepInstructions;
    Instructions m_inputIndepInstructions;
    Instructions m_controlDepInstructions;
    Instructions m_dataDepInstructions;
    Instructions m_globalDepInstructions;
    Instructions m_argumentDepInstructions;
    Instructions m_unknownInstructions;
    Instructions m_unreachableInstructions;
    long unsigned m_dataIndepInstrCount;
}; // class CachedFunctionAnalysisResult

} // namespace input_dependency

