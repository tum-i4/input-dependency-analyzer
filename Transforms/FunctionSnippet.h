#pragma once

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

#include <unordered_set>

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
    virtual bool is_valid_snippet() const = 0;
    virtual bool intersects(const Snippet& snippet) const = 0;
    virtual void expand() = 0;
    virtual void merge(const Snippet& snippet) = 0;
    virtual llvm::Function* toFunction() = 0;
    virtual void dump() const = 0;

    virtual InstructionsSnippet* to_instrSnippet()
    {
        return nullptr;
    }

    virtual BasicBlocksSnippet* to_blockSnippet()
    {
        return nullptr;
    }
};

class InstructionsSnippet : public Snippet
{
public:
    using iterator = llvm::BasicBlock::iterator;

public:
    InstructionsSnippet(llvm::BasicBlock* block, iterator begin, iterator end);

public:
    bool is_valid_snippet() const override;
    bool intersects(const Snippet& snippet) const override;
    void expand() override;
    void merge(const Snippet& snippet) override;
    llvm::Function* toFunction() override;
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
    void clear();

public:
    static bool is_valid_snippet(iterator begin, iterator end, llvm::BasicBlock* B);

private:
    using InstructionSet = std::unordered_set<llvm::Instruction*>;
    void snippet_instructions(InstructionSet& instrs) const;
    void expand_for_instruction(llvm::Instruction* instr,
                                InstructionSet& instructions);
    void expand_for_instruction_operand(llvm::Value* val,
                                        InstructionSet& instructions);

private:
    llvm::BasicBlock* m_block;
    iterator m_begin;
    iterator m_end;
    int m_begin_idx;
    int m_end_idx;
};

class BasicBlocksSnippet : public Snippet
{
public:
    using iterator = llvm::Function::iterator;

public:
    BasicBlocksSnippet(llvm::Function* function,
                       iterator begin,
                       iterator end,
                       InstructionsSnippet start);

public:
    bool is_valid_snippet() const override;
    bool intersects(const Snippet& snippet) const override;
    void expand() override;
    void merge(const Snippet& snippet) override;
    llvm::Function* toFunction() override;
    void dump() const override;
    virtual BasicBlocksSnippet* to_blockSnippet() override;

    iterator get_begin() const;
    iterator get_end() const;

public:
    static bool is_valid_snippet(iterator begin, iterator end, llvm::Function* F);

private:
    llvm::Function* m_function;
    iterator m_begin;
    iterator m_end;
    InstructionsSnippet m_start;
};

} // namespace oh

