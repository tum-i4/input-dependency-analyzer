#pragma once

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

#include <unordered_set>

namespace llvm {
class LLVMContext;
class ReturnInst;
}

namespace oh
{

/**
* \class Snippet
* \brief Base class for functions snippet
* Defines common interface for function snippets.
*/
class InstructionsSnippet;
class BasicBlocksSnippet;

class Snippet
{
public:
    using ValueSet = std::unordered_set<llvm::Value*>;
    using InstructionSet = std::unordered_set<llvm::Instruction*>;

public:
    Snippet()
        : m_instruction_number(0)
    {
    }

    virtual ~Snippet()
    {
    }

    virtual bool is_valid_snippet() const = 0;
    virtual unsigned get_instructions_number() const = 0;
    virtual bool contains_instruction(llvm::Instruction* instr) const = 0;
    virtual bool contains_block(llvm::BasicBlock* block) const = 0;
    virtual bool is_single_instr_snippet() const
    {
        return false;
    }
    virtual bool is_function() const = 0;
    virtual llvm::BasicBlock* get_begin_block() const = 0;
    virtual llvm::BasicBlock* get_end_block() const = 0;
    virtual bool intersects(const Snippet& snippet) const = 0;
    virtual InstructionSet expand() = 0;
    virtual void adjust_end() = 0;
    virtual void collect_used_values(const Snippet* parent_snippet) = 0;
    virtual bool merge(const Snippet& snippet) = 0;
    virtual llvm::Function* to_function() = 0;
    virtual void dump() const = 0;

    const ValueSet& get_used_values() const
    {
        return m_used_values;
    }

    virtual InstructionsSnippet* to_instrSnippet()
    {
        return nullptr;
    }

    virtual BasicBlocksSnippet* to_blockSnippet()
    {
        return nullptr;
    }

protected:
    ValueSet m_used_values; 
    InstructionSet m_allocas_to_extract;
    unsigned m_instruction_number;
};

class InstructionsSnippet : public Snippet
{
public:
    using iterator = llvm::BasicBlock::iterator;

public:
    InstructionsSnippet();
    InstructionsSnippet(llvm::BasicBlock* block, iterator begin, iterator end);

public:
    bool is_valid_snippet() const override;
    unsigned get_instructions_number() const override;
    bool contains_instruction(llvm::Instruction* instr) const override;
    bool contains_block(llvm::BasicBlock* block) const override;
    llvm::BasicBlock* get_begin_block() const override;
    llvm::BasicBlock* get_end_block() const override;
    bool is_single_instr_snippet() const override;
    bool is_function() const override;
    bool intersects(const Snippet& snippet) const override;
    InstructionSet expand() override;
    void adjust_end() override;
    void collect_used_values(const Snippet* parent_snippet) override;
    bool merge(const Snippet& snippet) override;
    llvm::Function* to_function() override;
    void dump() const override;
    virtual InstructionsSnippet* to_instrSnippet() override;

public:
    iterator get_begin() const;
    iterator get_end() const;
    int get_begin_index() const;
    int get_end_index() const;
    llvm::Instruction* get_begin_instr() const;
    llvm::Instruction* get_end_instr() const;
    bool is_block() const;
    llvm::BasicBlock* get_block() const;
    void compute_indices();
    void clear();

public:
    static bool is_valid_snippet(iterator begin, iterator end, llvm::BasicBlock* B);

private:
    void snippet_instructions(InstructionSet& instrs) const;
    void expand_for_instruction(llvm::Instruction* instr,
                                InstructionSet& instructions,
                                InstructionSet& new_instructions);
    void expand_for_instruction_operand(llvm::Value* val,
                                        InstructionSet& instructions,
                                        InstructionSet& new_instructions);
    bool can_erase_snippet() const;

private:
    llvm::BasicBlock* m_block;
    llvm::ReturnInst* m_returnInst;
    iterator m_begin;
    iterator m_end;
    int m_begin_idx;
    int m_end_idx;
};

class BasicBlocksSnippet : public Snippet
{
public:
    using BlockSet = std::unordered_set<llvm::BasicBlock*>;
    using iterator = llvm::Function::iterator;

public:
    BasicBlocksSnippet(llvm::Function* function,
                       iterator begin,
                       iterator end,
                       InstructionsSnippet start);
    BasicBlocksSnippet(llvm::Function* function,
                       iterator begin,
                       iterator end,
                       const BlockSet& blocks,
                       InstructionsSnippet start);

public:
    const BlockSet& get_blocks() const
    {
        return m_blocks;
    }

    bool is_valid_snippet() const override;
    unsigned get_instructions_number() const override;
    bool contains_instruction(llvm::Instruction* instr) const override;
    bool contains_block(llvm::BasicBlock* block) const override;
    bool is_single_instr_snippet() const
    {
        return false;
    }
    bool is_function() const override;
    bool intersects(const Snippet& snippet) const override;
    InstructionSet expand() override;
    void adjust_end() override;
    void collect_used_values(const Snippet* parent_snippet) override;
    bool merge(const Snippet& snippet) override;
    llvm::Function* to_function() override;
    void dump() const override;
    virtual BasicBlocksSnippet* to_blockSnippet() override;

    const InstructionsSnippet& get_start_snippet() const;
    iterator get_begin() const;
    iterator get_end() const;
    llvm::BasicBlock* get_begin_block() const override;
    llvm::BasicBlock* get_end_block() const override;

public:
    static bool is_valid_snippet(iterator begin, iterator end, llvm::Function* F);

private:
    llvm::BasicBlock* find_return_block() const;
    bool can_erase_block_snippet() const;
    bool can_erase_block(llvm::BasicBlock* block) const;
    bool can_erase_instruction_range(llvm::BasicBlock::iterator begin,
                                     llvm::BasicBlock::iterator end) const;

private:
    llvm::Function* m_function;
    iterator m_begin;
    iterator m_end;
    InstructionsSnippet m_start;
    InstructionsSnippet m_tail;
    BlockSet m_blocks;
};

} // namespace oh

