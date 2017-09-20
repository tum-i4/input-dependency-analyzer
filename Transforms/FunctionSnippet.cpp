#include "FunctionSnippet.h"

#include "Utils.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <unordered_map>

namespace oh {

namespace {

class unique_name_generator
{
public:
    static unique_name_generator& get()
    {
        static unique_name_generator unique_gen;
        return unique_gen;
    }

private:
    unique_name_generator()
        : unique_id(0)
    {
    }

public:
    std::string get_unique(const std::string& name)
    {
        return name + std::to_string(unique_id++);
    }

private:
    unsigned unique_id;
};

using ValueToValueMap = std::unordered_map<llvm::Value*, llvm::Value*>;
using ArgIdxToValueMap = std::unordered_map<int, llvm::Value*>;
using ArgToValueMap = std::unordered_map<llvm::Argument*, llvm::Value*>;
using InstructionSet = std::unordered_set<llvm::Instruction*>;
using BlockSet = BasicBlocksSnippet::BlockSet;

/*
 * Returns allocated type for the value
 */
llvm::Type* get_value_type(llvm::Value* val)
{
    auto val_type = val->getType();
    if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(val)) {
        val_type = alloca->getAllocatedType();
    }
    return val_type;
}

void collect_values(llvm::Value* val,
                    Snippet::ValueSet& values)
{
    if (!val) {
        return;
    }
    auto instr = llvm::dyn_cast<llvm::Instruction>(val);
    if (!instr) {
        return;
    }
    if (llvm::dyn_cast<llvm::AllocaInst>(instr)) {
        values.insert(instr);
        return;
    }
    for (unsigned i = 0; i < instr->getNumOperands(); ++i) {
        collect_values(instr->getOperand(i), values);
    }
}

void collect_values(InstructionsSnippet::iterator begin,
                    InstructionsSnippet::iterator end,
                    Snippet::ValueSet& values)
{
    auto it = begin;
    while (it != end) {
        auto instr = &*it;
        ++it;
        collect_values(instr, values);
    }
}

/*
 * Creates value and argument maps necessary for function extraction,
 * argument adjustment and instructions remapping in extracted function.
 * This function not only collects maps, but also creates all necessary instructions for handling arguments in extracted function.
 *
 * arg_index_to_value defines the order of arguments for the function to be extracted,
 * as well as corresponding values to be passes at call site. e.g. 0 -> n, means value n will be the first argument of a call.
 * value_ptr_map defines a mapping of a value in original function, to corresponding value in extracted function.
 * e.g. for argument n, a value n.el will be created in extracted function. value_ptr_map will contain entry n -> n.el.
 * n.el is an intermediate value in extracted function. All uses of n in extracted instructions will be remapped to use n.el instead.
 * value_map defines mapping from n.ptr to n.el. n.ptr is a pointer value, that captures argument n. (all arguments are pointers).
 * This map is used at the end of extracted function, to store n.el values back to corresponding n.ptr pointers (i.e. to return from function).
 */
void setup_function_mappings(llvm::Function* new_F,
                             ArgIdxToValueMap& arg_index_to_value,
                             ValueToValueMap& value_ptr_map,
                             ValueToValueMap& value_map)
{
    // Create block for new function
    llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(new_F->getParent()->getContext(), "entry", new_F);
    llvm::IRBuilder<> builder(entry_block);

    // create mapping from used values to function arguments
    builder.SetInsertPoint(entry_block, ++builder.GetInsertPoint());
    auto arg_it = new_F->arg_begin();
    unsigned i = 0;
    while (arg_it != new_F->arg_end()) {
        const std::string arg_name = "arg" + std::to_string(i);
        arg_it->setName(arg_name);
        auto ptr_type = llvm::dyn_cast<llvm::PointerType>(arg_it->getType());
        assert(ptr_type != nullptr);
        llvm::Value* val = arg_index_to_value[i];
        auto val_type = get_value_type(val);

        auto new_ptr_val = builder.CreateAlloca(ptr_type, nullptr,  arg_name + ".ptr");
        builder.CreateStore(&*arg_it, new_ptr_val);
        auto new_val = builder.CreateAlloca(ptr_type->getElementType(), nullptr, arg_name + ".el");
        auto ptr_load = builder.CreateLoad(new_ptr_val);
        auto load = builder.CreateLoad(ptr_load);
        builder.CreateStore(load, new_val);
        value_map[new_ptr_val] = new_val;
        value_ptr_map[val] = new_val;

        ++arg_it;
        ++i;
    }
}

void create_value_to_value_map(const ValueToValueMap& value_ptr_map,
                               llvm::ValueToValueMapTy& value_to_value_map)
{
    for (auto& entry : value_ptr_map) {
        //llvm::dbgs() << "add to value-to-value map " << *entry.first << "   " << *entry.second << "\n";
        value_to_value_map.insert(std::make_pair(entry.first, llvm::WeakVH(entry.second)));
    }
}

void remap_value_in_instruction(llvm::Instruction* instr, llvm::Value* old_value, llvm::Value* new_value)
{
    for (llvm::Use& op : instr->operands()) {
        llvm::Value* val = &*op;
        if (val == old_value) {
            op = new_value;
        }
    }
}

/*
 * Clone instructions of the given snippet into new block.
 * Creates mapping from old instructions to new clones
 */
void clone_snippet_to_function(llvm::BasicBlock* block,
                               InstructionsSnippet::iterator begin,
                               InstructionsSnippet::iterator end,
                               llvm::ValueToValueMapTy& value_to_value_map)
{
    auto& new_function_instructions = block->getInstList();
    auto inst_it = begin;
    // to include end
    if (end != begin->getParent()->end()) {
        end++;
    }
    while (inst_it != end) {
        llvm::Instruction* I = &*inst_it;
        ++inst_it;
        llvm::Instruction* new_I = I->clone();
        new_function_instructions.push_back(new_I);
        value_to_value_map.insert(std::make_pair(I, llvm::WeakVH(new_I)));
    }
}

void clone_blocks_snippet_to_function(llvm::Function* new_F,
                                      const BlockSet& blocks_to_clone,
                                      BasicBlocksSnippet::iterator begin,
                                      BasicBlocksSnippet::iterator end,
                                      bool clone_begin,
                                      llvm::ValueToValueMapTy& value_to_value_map)
{
    // will clone begin, however it might be replaced later by new entry block, created for start snippet
    llvm::SmallVector<llvm::BasicBlock*, 10> blocks;
    for (const auto& block : blocks_to_clone) {
        if (block == &*begin && !clone_begin) {
            continue;
        }
        auto clone = llvm::CloneBasicBlock(block, value_to_value_map, "", new_F);
        value_to_value_map.insert(std::make_pair(block, llvm::WeakVH(clone)));
        blocks.push_back(clone);
    }
    auto exit_clone = llvm::CloneBasicBlock(&*end, value_to_value_map, "", new_F);
    value_to_value_map.insert(std::make_pair(&*end, llvm::WeakVH(exit_clone)));
    blocks.push_back(exit_clone);
    llvm::remapInstructionsInBlocks(blocks, value_to_value_map);
}

void create_new_exit_block(llvm::Function* new_F, llvm::BasicBlock* old_exit_block)
{
    std::string block_name = unique_name_generator::get().get_unique("exit");
    llvm::BasicBlock* new_exit = llvm::BasicBlock::Create(new_F->getParent()->getContext(), block_name, new_F);
    auto retInst = llvm::ReturnInst::Create(new_F->getParent()->getContext());
    new_exit->getInstList().push_back(retInst);
    auto pred = pred_begin(old_exit_block);
    while (pred != pred_end(old_exit_block)) {
        llvm::BasicBlock* pred_block = *pred;
        ++pred; // keep this here
        auto term = pred_block->getTerminator();
        remap_value_in_instruction(term, old_exit_block, new_exit);
    }
    old_exit_block->eraseFromParent();
}

void remap_instructions_in_new_function(llvm::BasicBlock* block,
                                        unsigned skip_instr_count,
                                        llvm::ValueToValueMapTy& value_to_value_map)
{
    //for (const auto& map_entry : value_to_value_map) {
    //    llvm::dbgs() << "mapped " << *map_entry.first << " to " << *map_entry.second << "\n";
    //}

    llvm::ValueMapper mapper(value_to_value_map);
    //std::vector<llvm::Instruction*> not_mapped_instrs;
    unsigned skip = 0;
    for (auto& instr : *block) {
        if (skip++ < skip_instr_count) {
            continue;
        }

        mapper.remapInstruction(instr);
        //llvm::dbgs() << "Remaped instr: " << instr << "\n";
    }
}

void create_return_stores(llvm::BasicBlock* block,
                          const ValueToValueMap& value_map)
{
    llvm::IRBuilder<> builder(block);
    builder.SetInsertPoint(block->getTerminator());
    // first - pointer, second - value
    for (auto& ret_entry : value_map) {
        auto load_ptr = builder.CreateLoad(ret_entry.first);
        auto load_val = builder.CreateLoad(ret_entry.second);
        builder.CreateStore(load_val, load_ptr);
    }
}

llvm::CallInst* create_call_to_snippet_function(llvm::Function* F,
                                                llvm::Instruction* insertion_point,
                                                bool insert_before,
                                                const ArgIdxToValueMap& arg_index_to_value,
                                                const ValueToValueMap& value_ptr_map)
{
    std::vector<llvm::Value*> arguments(arg_index_to_value.size());
    llvm::IRBuilder<> builder(insertion_point);
    if (insert_before) {
        builder.SetInsertPoint(insertion_point->getParent(), builder.GetInsertPoint());
    } else {
        builder.SetInsertPoint(insertion_point->getParent(), ++builder.GetInsertPoint());
    }
    for (auto& arg_entry : arg_index_to_value) {
        auto val_type = get_value_type(arg_entry.second);
        arguments[arg_entry.first] = arg_entry.second;
    }
    llvm::ArrayRef<llvm::Value*> args_array(arguments);
    llvm::CallInst* callInst = builder.CreateCall(F, args_array);
    return callInst;
}

void erase_snippet(llvm::BasicBlock* block,
                   llvm::BasicBlock::iterator begin,
                   llvm::BasicBlock::iterator end)
{
    assert(InstructionsSnippet::is_valid_snippet(begin, end, block));
    if (end == begin->getParent()->end()) {
        --end;
    }
    while (end != begin) {
        auto inst = &*end;
        --end;
        if (!inst->user_empty()) {
            llvm::dbgs() << "Instruction has uses: do not erase " << *inst << "\n";
            continue;
        }
        inst->eraseFromParent();
    }
    if (begin->user_empty()) {
        begin->eraseFromParent();
    }
}

// TODO: cleanup this mess
/*
 * Snippet won't be erased if any of the blocks in a snippet, except begin block, has a predecessor outside of the snippet.
 */
void get_block_phi_nodes(llvm::BasicBlock* block, InstructionSet& phi_nodes)
{
    auto non_phi = block->getFirstNonPHI();
    if (non_phi) {
        auto non_phi_pos = non_phi->getIterator();
        std::for_each(block->begin(), non_phi_pos,
                [&phi_nodes] (llvm::Instruction& instr) { phi_nodes.insert(&instr);});
    }
}

bool get_block_users(llvm::BasicBlock* block,
                     const BlockSet& blocks,
                     InstructionSet& users)
{
    for (auto user : block->users()) {
        if (auto instr = llvm::dyn_cast<llvm::Instruction>(user)) {
            if (users.find(instr) != users.end()) {
                continue;
            }
            auto user_parent = instr->getParent();
            if (blocks.find(user_parent) == blocks.end()) {
                return false;
            }
            users.insert(instr);
        }
    }
    return true;
}

void erase_snippet(llvm::Function* function,
                   bool erase_begin,
                   llvm::Function::iterator begin,
                   llvm::Function::iterator end,
                   const BasicBlocksSnippet::BlockSet& blocks)
{
    assert(BasicBlocksSnippet::is_valid_snippet(begin, end, function));

    // change all predecessors from self blocks to link dummy_block
    llvm::BasicBlock* dummy_block = llvm::BasicBlock::Create(function->getParent()->getContext(), "dummy", function);
    std::vector<llvm::BasicBlock*> blocks_to_erase;
    InstructionSet users_to_remap;
    bool erase_blocks = true;

    llvm::ValueToValueMapTy block_map;
    for (const auto& block : blocks) {
        if ((block == &*begin && !erase_begin) ||  block == &*end) {
            continue;
        }
        block_map.insert(std::make_pair(block, llvm::WeakVH(dummy_block)));
        blocks_to_erase.push_back(block);

        // add all phi nodes, as those are not reported as use :[[
        // note this is not necessarily solving the problem with other uses
        get_block_phi_nodes(block, users_to_remap);
        if (pred_empty(block) && block->user_empty()) {
            continue;
        }
        if (!get_block_users(block, blocks, users_to_remap)) {
            erase_blocks = false;
            break;
        }
    }
    if (!erase_blocks) {
        llvm::dbgs() << "Basic blocks have uses: do not erase snippet\n";
        users_to_remap.clear();
        return;
    }

    block_map.insert(std::make_pair(&*end, llvm::WeakVH(dummy_block)));

    auto user_it = users_to_remap.begin();
    while (user_it != users_to_remap.end()) {
        llvm::Instruction* term = *user_it;
        ++user_it;
        for (llvm::Use& op : term->operands()) {
            llvm::Value* val = &*op;
            block_map.insert(std::make_pair(val, llvm::WeakVH(dummy_block)));
        }
        llvm::ValueMapper mapper(block_map);
        mapper.remapInstruction(*term);
    }

    auto it = blocks_to_erase.begin();
    while (it != blocks_to_erase.end()) {
        llvm::BasicBlock* block = *it;
        ++it;
        llvm::dbgs() << "Erase block " << block->getName() << "\n";
        block->eraseFromParent();
    }
    // all predecessors of dummy_block were the ones erased from snippet
    dummy_block->eraseFromParent();
    // end is not removed
}

llvm::FunctionType* create_function_type(llvm::LLVMContext& Ctx,
                                         const Snippet::ValueSet& used_values,
                                         llvm::Type* return_type,
                                         ArgIdxToValueMap& arg_values)
{
    std::vector<llvm::Type*> arg_types;
    int i = 0;
    for (auto& val : used_values) {
        //llvm::dbgs() << "Used value " << *val << "\n";
        arg_values[i] = val;
        ++i;
        auto type = get_value_type(val);
        arg_types.push_back(type->getPointerTo());
    }
    llvm::ArrayRef<llvm::Type*> params(arg_types);
    llvm::FunctionType* f_type = llvm::FunctionType::get(return_type, params, false);
    return f_type;
}

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
    if (m_end != block->end()) {
        m_returnInst = llvm::dyn_cast<llvm::ReturnInst>(&*end);
    } else {
        m_returnInst = nullptr;
    }
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
        expand_for_instruction(instr, instructions);
        // not to decrement begin
        if (it == m_begin) {
            break;
        }
    } while (it-- != m_begin);

}

void InstructionsSnippet::collect_used_values()
{
    if (!m_used_values.empty()) {
        // already collected
        return;
    }
    collect_values(m_begin, m_end++, m_used_values);
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
        const auto& used_values = instr_snippet->get_used_values();
        m_used_values.insert(used_values.begin(), used_values.end());
        return;
    }
    assert(false);
    // do not merge instruction snippet with block snippet,
    // as block snippet should be turn into instruction snippet
}

llvm::Function* InstructionsSnippet::to_function()
{
    //llvm::dbgs() << "used values\n";
    //for (const auto& val : m_used_values) {
    //    llvm::dbgs() << *val << "\n";
    //}
    
    // create function type
    llvm::LLVMContext& Ctx = m_block->getModule()->getContext();
    // maps argument index to corresponding value
    ArgIdxToValueMap arg_index_to_value;
    llvm::Type* return_type = m_returnInst ? m_block->getParent()->getReturnType() : llvm::Type::getVoidTy(Ctx);
    llvm::FunctionType* type = create_function_type(Ctx, m_used_values, return_type, arg_index_to_value);
    //llvm::dbgs() << "Function type " << *type << "\n";

    std::string f_name = unique_name_generator::get().get_unique(m_block->getParent()->getName());
    llvm::Function* new_F = llvm::Function::Create(type,
                                                   llvm::GlobalValue::LinkageTypes::ExternalLinkage,
                                                   f_name,
                                                   m_block->getModule());
    // Maps the values in original function to local values in extracted function.
    // Further adds instruction mapping as well
    ValueToValueMap value_ptr_map;

    // maps element type value to corresponding pointer value. all operations will be done over this value.
    // Maps local values mapped in previous map, to pointers passed as argument. This is done for convenience,
    // not to use pointers in operations.
    // e.g. if c = a + b is extracted, this instruction will be replaced by
    // c_local = a_local + b_local, where c_local, a_local and b_local are of the same type as a and b. 
    // Further c_local will be stored into corresponding c_ptr pointer value to be returned from function.
    // this map contains mapping from c_ptr to c_local
    ValueToValueMap value_map;

    setup_function_mappings(new_F, arg_index_to_value, value_ptr_map, value_map);
    llvm::BasicBlock* entry_block = &new_F->getEntryBlock();

    unsigned setup_size = entry_block->size();
    llvm::ValueToValueMapTy value_to_value_map;
    create_value_to_value_map(value_ptr_map, value_to_value_map);
    clone_snippet_to_function(entry_block, m_begin, m_end, value_to_value_map);
    remap_instructions_in_new_function(entry_block, setup_size, value_to_value_map);
    if (!new_F->getReturnType() || new_F->getReturnType()->isVoidTy()) {
        auto retInst = llvm::ReturnInst::Create(new_F->getParent()->getContext());
        entry_block->getInstList().push_back(retInst);
    }
    create_return_stores(entry_block, value_map);

    auto insert_before = m_begin;
    auto callInst = create_call_to_snippet_function(new_F, &*insert_before, true, arg_index_to_value, value_ptr_map);
    if (m_returnInst) {
        auto ret = llvm::ReturnInst::Create(Ctx, callInst, m_block);
    }
    erase_snippet(m_block, m_begin, m_end);
    return new_F;
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

bool InstructionsSnippet::is_single_instr_snippet() const
{
    return m_begin == m_end;
}

void InstructionsSnippet::snippet_instructions(InstructionSet& instrs) const
{
    std::for_each(m_begin, m_end, [&instrs] (llvm::Instruction& instr) { instrs.insert(&instr); });
    instrs.insert(&*m_end);
}

void InstructionsSnippet::expand_for_instruction(llvm::Instruction* instr,
                                                 InstructionSet& instructions)
{
    //llvm::dbgs() << "expand for instr " << *instr << "\n";
    if (auto load = llvm::dyn_cast<llvm::LoadInst>(instr)) {
        assert(instructions.find(instr) != instructions.end());
        if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(load->getPointerOperand())) {
            m_used_values.insert(alloca);
        } else if (auto loaded_inst = llvm::dyn_cast<llvm::Instruction>(load->getPointerOperand())) {
            expand_for_instruction_operand(loaded_inst, instructions);
        }
        return;
    }
    if (auto store = llvm::dyn_cast<llvm::StoreInst>(instr)) {
        auto value_op = store->getValueOperand();
        if (llvm::dyn_cast<llvm::AllocaInst>(value_op)) {
            m_used_values.insert(value_op);
            return;
        }
        expand_for_instruction_operand(value_op, instructions);
        auto storeTo = store->getPointerOperand();
        // e.g. for pointer loadInst will be pointer operand, but it should not be used as value
        if (llvm::dyn_cast<llvm::AllocaInst>(storeTo)) {
            m_used_values.insert(storeTo);
        } else {
            expand_for_instruction_operand(storeTo, instructions);
        }
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
    if (llvm::dyn_cast<llvm::AllocaInst>(val)) {
        m_used_values.insert(val);
        return;
    }
    auto res = instructions.insert(instr);
    if (!res.second) {
        return;
    }
    //llvm::dbgs() << "Expand: add " << *instr << "\n";
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
    m_blocks = Utils::get_blocks_in_range(m_begin, m_end);
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

void BasicBlocksSnippet::collect_used_values()
{
    if (!m_used_values.empty()) {
        return;
    }
    m_start.collect_used_values();
    auto used_in_start = m_start.get_used_values();
    m_used_values.insert(used_in_start.begin(), used_in_start.end());
    for (const auto& block : m_blocks) {
        collect_values(block->begin(), block->end(), m_used_values);
    }
    if (m_blocks.find(&*m_begin) == m_blocks.end()) {
        collect_values(m_begin->begin(), m_begin->end(), m_used_values);
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

// create function type and corresponding function
// need to create entry block for start snippet in new function,
// clone all blocks in snippet to new function
// create new exit block in new function
// replace all edges to the end block to new exit block.
// Remove end block from new function
// Remove [m_begin, m_end) blocks from original functions
// Remove start snippet from old function.
// create call instruction
// create unconditional jump to m_end after call instruction
// TODO: extract the common code with instruction snippet
llvm::Function* BasicBlocksSnippet::to_function()
{

    m_blocks = Utils::get_blocks_in_range(m_begin, m_end);
    collect_used_values();

    llvm::LLVMContext& Ctx = m_function->getParent()->getContext();
    // maps argument index to corresponding value
    ArgIdxToValueMap arg_index_to_value;
    llvm::FunctionType* type = create_function_type(Ctx, m_used_values, llvm::Type::getVoidTy(Ctx), arg_index_to_value);
    //llvm::dbgs() << "Function type " << *type << "\n";
    std::string f_name = unique_name_generator::get().get_unique(m_begin->getParent()->getName());
    llvm::Function* new_F = llvm::Function::Create(type,
                                                   llvm::GlobalValue::LinkageTypes::ExternalLinkage,
                                                   f_name,
                                                   m_function->getParent());
    ValueToValueMap value_ptr_map;
    ValueToValueMap value_map;
    setup_function_mappings(new_F, arg_index_to_value, value_ptr_map, value_map);
    llvm::BasicBlock* entry_block = &new_F->getEntryBlock();

    const bool has_start_snippet = m_start.is_valid_snippet();
    llvm::ValueToValueMapTy value_to_value_map;
    create_value_to_value_map(value_ptr_map, value_to_value_map);

    // this function will also create new exit block
    clone_blocks_snippet_to_function(new_F, m_blocks, m_begin, m_end, !has_start_snippet, value_to_value_map);

    unsigned setup_size = entry_block->size();
    if (has_start_snippet) {
        // begining will go to entry block
        clone_snippet_to_function(entry_block, m_start.get_begin(), m_start.get_end(), value_to_value_map);
        remap_instructions_in_new_function(entry_block, setup_size, value_to_value_map);
    } else {
        // if has start snippet it will have terminator instr, which will be added to new entry
        // in case if there is no start snippet, entry block is the one we created, hence needs a terminator
        auto begin_block_pos = value_to_value_map.find(&*m_begin);
        assert(begin_block_pos != value_to_value_map.end());
        llvm::BasicBlock* begin_block = llvm::dyn_cast<llvm::BasicBlock>(&*begin_block_pos->second);
        auto entry_terminator = llvm::BranchInst::Create(begin_block);
        entry_block->getInstList().push_back(entry_terminator);
    }

    // create new exit block
    auto exit_block_entry = value_to_value_map.find(&*m_end);
    assert(exit_block_entry != value_to_value_map.end());
    llvm::BasicBlock* exit_block = llvm::dyn_cast<llvm::BasicBlock>(&*exit_block_entry->second);
    assert(exit_block);
    create_new_exit_block(new_F, exit_block);
    create_return_stores(&new_F->back(), value_map);

    if (has_start_snippet) {
        auto insert_before = m_start.get_begin();
        create_call_to_snippet_function(new_F, &*insert_before, true, arg_index_to_value, value_ptr_map);
        erase_snippet(m_start.get_block(), m_start.get_begin(), m_start.get_end());
        auto branch_inst = llvm::BranchInst::Create(&*m_end);
        m_start.get_block()->getInstList().push_back(branch_inst);
    } else {
        // insert at the end of each predecessor
        // Is it safe to assume there is only one predecessor?
        std::string block_name = unique_name_generator::get().get_unique("call_block");
        llvm::BasicBlock* call_block = llvm::BasicBlock::Create(m_function->getParent()->getContext(), block_name, m_function);
        auto pred_it = pred_begin(&*m_begin);
        while (pred_it != pred_end(&*m_begin)) {
            auto pred = *pred_it;
            ++pred_it;
            if (m_blocks.find(pred) != m_blocks.end()) {
                continue;
            }
            auto pred_term = pred->getTerminator();
            for (llvm::Use& op : pred_term->operands()) {
                llvm::Value* val = &*op;
                if (auto b = llvm::dyn_cast<llvm::BasicBlock>(val)) {
                    if (b == &*m_begin) {
                        op = call_block;
                    }
                }
            }
        }
        auto call_term = llvm::BranchInst::Create(&*m_end);
        call_block->getInstList().push_back(call_term);
        auto insert_before = call_block->end();
        --insert_before;
        create_call_to_snippet_function(new_F, &*insert_before, true, arg_index_to_value, value_ptr_map);
    }

    erase_snippet(m_function, !has_start_snippet, m_begin, m_end, m_blocks);
    return new_F;
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
    if (m_start.is_valid_snippet()) {
        m_start.dump();
    }
    for (const auto& b : m_blocks) {
        llvm::dbgs() << b->getName() << "\n";
    }
    if (m_end != m_begin->getParent()->end()) {
        llvm::dbgs() << m_end->getName() << "\n";
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

