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
    InstructionsSnippet(iterator begin, iterator end);

public:
    llvm::Function* toFunction() override;
    void dump() const override;
    virtual InstructionsSnippet* to_instrSnippet() override;

private:
    using InstructionSet = std::unordered_set<llvm::Instruction*>;
    void snippet_instructions(InstructionSet& instrs) const;

private:
    iterator m_begin;
    iterator m_end;
};


class BasicBlocksSnippet : public Snippet
{
public:
    using iterator = llvm::Function::iterator;

public:
    BasicBlocksSnippet(iterator begin, iterator end, InstructionsSnippet start);

public:
    llvm::Function* toFunction() override;

    iterator get_begin() const;
    iterator get_end() const;
    void dump() const override;
    virtual BasicBlocksSnippet* to_blockSnippet() override;

private:
    iterator m_begin;
    iterator m_end;
    InstructionsSnippet m_start;
};

} // namespace oh

