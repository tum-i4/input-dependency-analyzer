#include "FunctionSnippet.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace oh {

namespace {

InstructionsSnippet::iterator get_instruction_pos(llvm::Instruction* I)
{
    InstructionsSnippet::iterator it = I->getParent()->begin();
    while (&*it != I && it != I->getParent()->end()) {
        ++it;
    }
    assert(it != I->getParent()->end());
    return it;
}

}

InstructionsSnippet::InstructionsSnippet(iterator begin, iterator end)
    : m_begin(begin)
    , m_end(end)
{
}

llvm::Function* InstructionsSnippet::toFunction()
{
    InstructionSet instructions;
    snippet_instructions(instructions);
    auto it = m_end;
    while (it != m_begin) {
    }
    // TODO
    return nullptr;
}

void InstructionsSnippet::dump() const
{
    llvm::dbgs() << "****Instructions snippet****\n";
    auto it = m_begin;
    while (it != m_end) {
        llvm::dbgs() << *it << "\n";
        ++it;
    }
    if (m_end != m_begin->getParent()->end()) {
        llvm::dbgs() << *it << "\n";
    }
    llvm::dbgs() << "*********\n";
}

InstructionsSnippet* InstructionsSnippet::to_instrSnippet()
{
    return this;
}

void InstructionsSnippet::snippet_instructions(InstructionSet& instrs) const
{
    std::for_each(m_begin, m_end, [&instrs] (llvm::Instruction& instr) { instrs.insert(&instr); });
}

BasicBlocksSnippet::BasicBlocksSnippet(iterator begin,
                                       iterator end,
                                       InstructionsSnippet start)
    : m_begin(begin)
    , m_end(end)
    , m_start(start)
{
}

llvm::Function* BasicBlocksSnippet::toFunction()
{
    // TODO
    return nullptr;
}

BasicBlocksSnippet::iterator BasicBlocksSnippet::get_begin() const
{
    return m_begin;
}

BasicBlocksSnippet::iterator BasicBlocksSnippet::get_end() const
{
    return m_end;
}

BasicBlocksSnippet* BasicBlocksSnippet::to_blockSnippet()
{
    return this;
}

void BasicBlocksSnippet::dump() const
{
    llvm::dbgs() << "****Block snippet*****\n";
    m_start.dump();
    auto it = m_begin;
    while (it != m_end) {
        llvm::dbgs() << it->getName() << "\n";
        ++it;
    }
    if (m_end != m_begin->getParent()->end()) {
        llvm::dbgs() << it->getName() << "\n";
    }
    llvm::dbgs() << "*********\n";
}
}

