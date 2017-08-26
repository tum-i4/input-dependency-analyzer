#include "FunctionSnippet.h"

#include "Utils.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace oh {

namespace {

}

InstructionsSnippet::InstructionsSnippet(llvm::BasicBlock* block,
                                         iterator begin,
                                         iterator end)
    : m_block(block)
    , m_begin(begin)
    , m_end(end)
    , m_begin_idx(-1)
    , m_end_idx(-1)
{
    m_begin_idx = Utils::get_instruction_index(&*m_begin);
    m_end_idx = Utils::get_instruction_index(&*m_end);
}

bool InstructionsSnippet::is_valid_snippet() const
{
    return m_block && InstructionsSnippet::is_valid_snippet(m_begin, m_end, m_block);
}

bool InstructionsSnippet::intersects(const Snippet& snippet) const
{
    assert(snippet.is_valid_snippet());
    auto instr_snippet = const_cast<Snippet&>(snippet).to_instrSnippet();
    if (instr_snippet) {
        if (m_block != instr_snippet->get_block()) {
            return false;
        }
        return instr_snippet->get_begin_index() <= m_end_idx && m_begin_idx <= instr_snippet->get_end_index();
    }
    // redirect to block snippet function
    return snippet.intersects(*this);
}

void InstructionsSnippet::expand()
{
    InstructionSet instructions;
    snippet_instructions(instructions);
    auto it = m_end;
    do {
        llvm::Instruction* instr = &*it;
        llvm::dbgs() << "P: " << *instr << "\n";
        expand_for_instruction(instr, instructions);
    } while (it-- != m_begin);
}

void InstructionsSnippet::merge(const Snippet& snippet)
{
    // expand this to include given snippet
    auto instr_snippet = const_cast<Snippet&>(snippet).to_instrSnippet();
    if (instr_snippet) {
        if (m_begin_idx > instr_snippet->get_begin_index()) {
            m_begin = instr_snippet->get_begin();
            m_begin_idx = instr_snippet->get_begin_index();
        }
        if (m_end_idx < instr_snippet->get_end_index()) {
            m_end = instr_snippet->get_end();
            m_end_idx = instr_snippet->get_end_index();
        }
        return;
    }
    assert(false);
    // do not merge instruction snippet with block snippet,
    // as block snippet should be turn into instruction snippet
}

llvm::Function* InstructionsSnippet::toFunction()
{
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

InstructionsSnippet::iterator InstructionsSnippet::get_begin() const
{
    return m_begin;
}

InstructionsSnippet::iterator InstructionsSnippet::get_end() const
{
    return m_end;
}

llvm::Instruction* InstructionsSnippet::get_begin_instr() const
{
    return &*m_begin;
}

llvm::Instruction* InstructionsSnippet::get_end_instr() const
{
    return &*m_end;
}

int InstructionsSnippet::get_begin_index() const
{
    return m_begin_idx;
}

int InstructionsSnippet::get_end_index() const
{
    return m_end_idx;
}

bool InstructionsSnippet::is_block() const
{
    return (m_begin == m_block->begin() && m_end == --m_block->end());
}

llvm::BasicBlock* InstructionsSnippet::get_block() const
{
    return m_block;
}

void InstructionsSnippet::clear()
{
    if (!is_valid_snippet()) {
        return;
    }
    m_end = m_block->end();
    m_begin = m_end;
    m_begin_idx = -1;
    m_end_idx = -1;
    m_block = nullptr;
}

bool InstructionsSnippet::is_valid_snippet(iterator begin,
                                           iterator end,
                                           llvm::BasicBlock* block)
{
    bool valid = (begin != block->end());
    valid &= (begin->getParent() == block);
    if (end != block->end()) {
        valid &= (end->getParent() == block);
    }
    return valid;
}


void InstructionsSnippet::snippet_instructions(InstructionSet& instrs) const
{
    std::for_each(m_begin, m_end, [&instrs] (llvm::Instruction& instr) { instrs.insert(&instr); });
    instrs.insert(&*m_end);
}

void InstructionsSnippet::expand_for_instruction(llvm::Instruction* instr,
                                                 InstructionSet& instructions)
{
    if (llvm::dyn_cast<llvm::LoadInst>(instr)) {
        assert(instructions.find(instr) != instructions.end());
        return;
    }
    if (auto store = llvm::dyn_cast<llvm::StoreInst>(instr)) {
        auto value_op = store->getValueOperand();
        expand_for_instruction_operand(value_op, instructions);
    } else {
        for (unsigned i = 0; i < instr->getNumOperands(); ++i) {
            expand_for_instruction_operand(instr->getOperand(i), instructions);
        }
    }
}

void InstructionsSnippet::expand_for_instruction_operand(llvm::Value* val,
                                                         InstructionSet& instructions)
{
    auto instr = llvm::dyn_cast<llvm::Instruction>(val);
    if (!instr) {
        return;
    }
    auto res = instructions.insert(instr);
    if (!res.second) {
        return;
    }
    auto new_begin = instr->getIterator();
    auto new_begin_idx = Utils::get_instruction_index(&*new_begin);
    if (m_begin_idx > new_begin_idx) {
        m_begin = new_begin;
        m_begin_idx = new_begin_idx;
    }
}


BasicBlocksSnippet::BasicBlocksSnippet(llvm::Function* function,
                                       iterator begin,
                                       iterator end,
                                       InstructionsSnippet start)
    : m_function(function)
    , m_begin(begin)
    , m_end(end)
    , m_start(start)
{
}

bool BasicBlocksSnippet::is_valid_snippet() const
{
    return m_function && BasicBlocksSnippet::is_valid_snippet(m_begin, m_end, m_function);
}

bool BasicBlocksSnippet::intersects(const Snippet& snippet) const
{
    if (!m_start.is_valid_snippet()) {
        return false;
    }
    return m_start.intersects(snippet);
}

void BasicBlocksSnippet::expand()
{
    m_start.expand();
    // can include block in snippet
    if (m_start.is_block()) {
        //m_begin = Utils::get_block_pos(m_start.get_block());
        m_begin = m_start.get_block()->getIterator();
        m_start.clear();
    }
}

void BasicBlocksSnippet::merge(const Snippet& snippet)
{
    auto instr_snippet = const_cast<Snippet&>(snippet).to_instrSnippet();
    if (instr_snippet) {
        m_start.merge(snippet);
    }
    // do not merge block snippets for now
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

bool BasicBlocksSnippet::is_valid_snippet(iterator begin,
                                          iterator end,
                                          llvm::Function* parent)
{
    return (begin != parent->end() && begin != end);
}

}

