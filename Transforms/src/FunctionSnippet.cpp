#include "input-dependency/Transforms/FunctionSnippet.h"

#include "input-dependency/Transforms/Utils.h"
#include "input-dependency/Analysis/BasicBlocksUtils.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
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
    {
    }

public:
    std::string get_unique(const std::string& name)
    {
        const std::string unique_name = name + std::to_string(name_numbering[name]);
        ++name_numbering[name];
        return unique_name;
    }

private:
    std::unordered_map<std::string, unsigned> name_numbering;
};

// checks if snippet1 is predecessor snippet for snippet2,
//meaning begin block of snippet2 is successor node for end block of snippet1
bool is_predecessing_block_snippet(const Snippet& snippet1, const Snippet& snippet2)
{
    auto snippet_end = snippet1.get_end_block();
    if (snippet_end == &*snippet1.get_begin_block()->getParent()->end()) {
        // if snippet1 ends with the function, it can not be predecessor of any other snippet.
        // note however it may contain snippet2, but this function does not check that.
        return false;
    }
    if (snippet_end == snippet2.get_begin_block()) {
        return true;
    }
    auto succ_it = succ_begin(snippet_end);
    while (succ_it != succ_end(snippet_end)) {
        if (*succ_it == snippet2.get_begin_block()) {
            return true;
        }
        ++succ_it;
    }
    return false;
}

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

void collect_locally_used_allocas(const Snippet& snippet,
                                  Snippet::ValueSet& used_values,
                                  Snippet::InstructionSet& allocas)
{
    Snippet::ValueSet values_to_erase;
    for (auto used_value : used_values) {
        auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(used_value);
        if (!alloca) {
            continue;
        }
        bool is_used_locally = true;
        for (auto user : alloca->users()) {
            if (auto* user_instr = llvm::dyn_cast<llvm::Instruction>(user)) {
                if (!snippet.contains_instruction(user_instr)) {
                    is_used_locally = false;
                    break;
                }
            } else {
                is_used_locally = false;
                break;
            }
        }
        if (is_used_locally) {
            values_to_erase.insert(used_value);
            allocas.insert(alloca);
        }
    }
    for (auto val : values_to_erase) {
        used_values.erase(val);
    }
}

void collect_values(llvm::Value* val,
                    const Snippet& snippet,
                    Snippet::ValueSet& values)
{
    if (!val) {
        return;
    }
    if (auto* arg = llvm::dyn_cast<llvm::Argument>(val)) {
        values.insert(val);
        return;
    }
    auto instr = llvm::dyn_cast<llvm::Instruction>(val);
    if (!instr) {
        return;
    }

    if (llvm::dyn_cast<llvm::AllocaInst>(instr)) {
        if (!snippet.contains_block(instr->getParent())) {
            values.insert(instr);
        } else {
            llvm::dbgs() << "**** Skip value as alloca is inside snippet " << *instr << "\n";
        }
        return;
    } else if (!snippet.contains_instruction(instr) && !llvm::dyn_cast<llvm::BranchInst>(instr)) {
        values.insert(instr);
        return;
    }

    for (unsigned i = 0; i < instr->getNumOperands(); ++i) {
        collect_values(instr->getOperand(i), snippet, values);
    }
}

void collect_values(InstructionsSnippet::iterator begin,
                    InstructionsSnippet::iterator end,
                    const Snippet& snippet,
                    Snippet::ValueSet& values)
{
    auto it = begin;
    while (it != end) {
        auto instr = &*it;
        ++it;
        collect_values(instr, snippet, values);
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
                             ValueToValueMap& value_map,
                             std::unordered_map<llvm::Value*, int>& operand_indices)
{
    // Create block for new function
    llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(new_F->getParent()->getContext(), "entry", new_F);
    llvm::IRBuilder<> builder(entry_block);

    // create mapping from used values to function arguments
    builder.SetInsertPoint(entry_block, ++builder.GetInsertPoint());
    auto arg_it = new_F->arg_begin();
    unsigned i = 0;
    while (arg_it != new_F->arg_end()) {
        auto ptr_type = llvm::dyn_cast<llvm::PointerType>(arg_it->getType());
        //assert(ptr_type != nullptr);
        llvm::Value* val = arg_index_to_value[i];
        arg_it->setName(val->getName());
        auto val_type = get_value_type(val);

        const std::string& arg_name = arg_it->getName();
        if (ptr_type) {
            auto new_ptr_val = builder.CreateAlloca(ptr_type, nullptr,  arg_name + ".ptr");
            builder.CreateStore(&*arg_it, new_ptr_val);
            auto new_val = builder.CreateAlloca(ptr_type->getElementType(), nullptr, arg_name + ".el");
            auto ptr_load = builder.CreateLoad(new_ptr_val);
            auto load = builder.CreateLoad(ptr_load);
            builder.CreateStore(load, new_val);
            value_map[new_ptr_val] = new_val;
            operand_indices[new_ptr_val] = i;
            value_ptr_map[val] = new_val;
        } else {
            auto new_val = builder.CreateAlloca(arg_it->getType(), nullptr, arg_name + ".el");
            builder.CreateStore(&*arg_it, new_val);
            auto load = builder.CreateLoad(new_val);
            value_ptr_map[val] = load;
        }
        ++arg_it;
        ++i;
    }
}

void create_value_to_value_map(const ValueToValueMap& value_ptr_map,
                               llvm::ValueToValueMapTy& value_to_value_map)
{
    for (auto& entry : value_ptr_map) {
        //llvm::dbgs() << "add to value-to-value map " << *entry.first << "   " << *entry.second << "\n";
        value_to_value_map.insert(std::make_pair(entry.first, llvm::WeakTrackingVH(entry.second)));
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
        value_to_value_map.insert(std::make_pair(I, llvm::WeakTrackingVH(new_I)));
    }
}

void clone_allocas(llvm::BasicBlock* block,
                   Snippet::InstructionSet& allocas,
                   llvm::ValueToValueMapTy& value_to_value_map)
{
    auto& new_function_instructions = block->getInstList();
    for (auto alloca : allocas) {
        llvm::Instruction* new_I = alloca->clone();
        new_function_instructions.push_back(new_I);
        value_to_value_map.insert(std::make_pair(alloca, llvm::WeakTrackingVH(new_I)));
    }
}


llvm::SmallVector<llvm::BasicBlock*, 10> clone_blocks_snippet_to_function(llvm::Function* new_F,
                                      const BlockSet& blocks_to_clone,
                                      BasicBlocksSnippet::iterator begin,
                                      BasicBlocksSnippet::iterator end,
                                      bool clone_begin,
                                      bool clone_end,
                                      llvm::ValueToValueMapTy& value_to_value_map)
{
    // will clone begin, however it might be replaced later by new entry block, created for start snippet
    llvm::SmallVector<llvm::BasicBlock*, 10> blocks;
    for (const auto& block : blocks_to_clone) {
        if (block == &*begin && !clone_begin) {
            continue;
        }
        if (block == &*end && !clone_end) {
            continue;
        }
        //llvm::dbgs() << "Clone block " << block->getName() << "\n";
        auto clone = llvm::CloneBasicBlock(block, value_to_value_map, "", new_F);
        if (input_dependency::BasicBlocksUtils::get().isBlockUnreachable(block)) {
            input_dependency::BasicBlocksUtils::get().addUnreachableBlock(clone);
        }
        value_to_value_map.insert(std::make_pair(block, llvm::WeakTrackingVH(clone)));
        blocks.push_back(clone);
    }
    if (blocks_to_clone.find(&*end) == blocks_to_clone.end() && clone_end) {
        auto exit_clone = llvm::CloneBasicBlock(&*end, value_to_value_map, "", new_F);
        value_to_value_map.insert(std::make_pair(&*end, llvm::WeakTrackingVH(exit_clone)));
        blocks.push_back(exit_clone);
    }
    return blocks;
    //llvm::remapInstructionsInBlocks(blocks, value_to_value_map);
}

void create_new_exit_block(llvm::Function* new_F, llvm::BasicBlock* old_exit_block)
{
    std::string block_name = unique_name_generator::get().get_unique("exit");
    llvm::BasicBlock* new_exit = llvm::BasicBlock::Create(new_F->getParent()->getContext(), block_name, new_F);
    auto old_term = old_exit_block->getTerminator();
    if (old_term && !llvm::dyn_cast<llvm::ReturnInst>(old_term)) {
        auto retInst = llvm::ReturnInst::Create(new_F->getParent()->getContext());
        new_exit->getInstList().push_back(retInst);
    }
    auto pred = pred_begin(old_exit_block);
    while (pred != pred_end(old_exit_block)) {
        llvm::BasicBlock* pred_block = *pred;
        ++pred; // keep this here
        auto term = pred_block->getTerminator();
        remap_value_in_instruction(term, old_exit_block, new_exit);
    }
    old_exit_block->eraseFromParent();
    if (!new_exit->getTerminator()) {
        auto retInst = llvm::ReturnInst::Create(new_F->getParent()->getContext());
        new_exit->getInstList().push_back(retInst);
    }
}

void remap_instructions_in_new_function(llvm::BasicBlock* block,
                                        unsigned skip_instr_count,
                                        llvm::ValueToValueMapTy& value_to_value_map)
{
    llvm::ValueMapper mapper(value_to_value_map);
    //std::vector<llvm::Instruction*> not_mapped_instrs;
    unsigned skip = 0;
    for (auto& instr : *block) {
        if (skip++ < skip_instr_count) {
            continue;
        }

        //llvm::dbgs() << "Remap instr: " << instr << "\n";
        mapper.remapInstruction(instr);
        //llvm::dbgs() << "Remaped instr: " << instr << "\n";
    }
}

void create_return_stores(llvm::BasicBlock* block,
                          const ValueToValueMap& value_map,
                          const std::unordered_map<llvm::Value*, int>& operand_indices)
{
    //llvm::dbgs() << "  Create return stores\n";
    llvm::IRBuilder<> builder(block);
    builder.SetInsertPoint(block->getTerminator());
    // first - pointer, second - value
    for (auto& ret_entry : value_map) {
        auto load_ptr = builder.CreateLoad(ret_entry.first);
        auto load_val = builder.CreateLoad(ret_entry.second);
        auto* storeInst = builder.CreateStore(load_val, load_ptr);
        auto index = operand_indices.find(ret_entry.first);
        assert(index != operand_indices.end());
        llvm::LLVMContext& Ctx = block->getParent()->getContext();
        auto* idx_md = llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx),
                                                              index->second));
        llvm::MDNode* idx_md_node = llvm::MDNode::get(Ctx, idx_md);
        storeInst->setMetadata("extraction_store", idx_md_node);
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

void erase_instruction_snippet(llvm::BasicBlock* block,
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
        bool erase = true;
        for (const auto& user : inst->users()) {
            if (auto* user_inst = llvm::dyn_cast<llvm::Instruction>(user)) {
                if (user_inst->getParent()->getParent() != block->getParent()) {
                    continue;
                }
                // assume that user is inside the full snippet which is being erased.
                // otherwise the extraction wouldn't even start
                user_inst->dropAllReferences();
                //erase = false;
            }
        }
        if (erase) { 
            inst->dropAllReferences();
            inst->eraseFromParent();
        }
    }
    bool erase = true;
    for (const auto& user : begin->users()) {
        if (auto* user_inst = llvm::dyn_cast<llvm::Instruction>(user)) {
            if (user_inst->getParent()->getParent() != block->getParent()) {
                continue;
            }
            user_inst->dropAllReferences();
            //erase = false;
        }
    }
    if (erase) {
        begin->dropAllReferences();
        begin->eraseFromParent();
    }
}

void erase_instructions(Snippet::InstructionSet& instructions)
{
    auto it = instructions.begin();
    while (it != instructions.end()) {
        auto inst = *it;
        ++it;
        for (const auto& user : inst->users()) {
            user->dropAllReferences();
        }
        inst->dropAllReferences();
        inst->eraseFromParent();
    }
}

// TODO: cleanup this mess
/*
 * Snippet won't be erased if any of the blocks in a snippet, except begin block, has a predecessor outside of the snippet.
 */
void get_block_phi_nodes(llvm::BasicBlock* block, Snippet::InstructionSet& phi_nodes)
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
                     Snippet::InstructionSet& users)
{
    for (auto user : block->users()) {
        if (auto instr = llvm::dyn_cast<llvm::Instruction>(user)) {
            if (users.find(instr) != users.end()) {
                continue;
            }
            auto user_parent = instr->getParent();
            if (blocks.find(user_parent) != blocks.end()) {
                users.insert(instr);
            } else if (input_dependency::BasicBlocksUtils::get().isBlockUnreachable(user_parent)) {
                llvm::dbgs() << "Erasing unreachable basic block " << *user_parent << "\n";
                user_parent->eraseFromParent();
            } else if (pred_empty(user_parent) && user_parent != &block->getParent()->getEntryBlock()) {
                llvm::dbgs() << "Erasing unreachable basic block " << *user_parent << "\n";
                user_parent->eraseFromParent();
            }else {
                return false;
            }
        }
    }
    return true;
}

void erase_block_snippet(llvm::Function* function,
                         bool erase_begin,
                         bool erase_end,
                         llvm::Function::iterator begin,
                         llvm::Function::iterator end,
                         const BasicBlocksSnippet::BlockSet& blocks,
                         const std::vector<llvm::BasicBlock*>& blocks_in_erase_order)
{
    assert(BasicBlocksSnippet::is_valid_snippet(begin, end, function));

    // change all predecessors from self blocks to link dummy_block
    llvm::BasicBlock* dummy_block = llvm::BasicBlock::Create(function->getParent()->getContext(), "dummy", function);
    Snippet::InstructionSet users_to_remap;
    bool erase_blocks = true;

    llvm::ValueToValueMapTy block_map;
    for (const auto& block : blocks_in_erase_order) {
        if (block == &*begin && !erase_begin) {
            continue;
        }
        if (block == &*end && !erase_end) {
            continue;
        }
        block_map.insert(std::make_pair(block, llvm::WeakTrackingVH(dummy_block)));

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
        dummy_block->eraseFromParent();
        users_to_remap.clear();
        return;
    }

    block_map.insert(std::make_pair(&*end, llvm::WeakTrackingVH(dummy_block)));
    block_map.insert(std::make_pair(&*begin, llvm::WeakTrackingVH(dummy_block)));
    auto user_it = users_to_remap.begin();
    while (user_it != users_to_remap.end()) {
        llvm::Instruction* term = *user_it;
        ++user_it;
        for (llvm::Use& op : term->operands()) {
            llvm::Value* val = &*op;
            block_map.insert(std::make_pair(val, llvm::WeakTrackingVH(dummy_block)));
        }
        llvm::ValueMapper mapper(block_map);
        mapper.remapInstruction(*term);
    }

    auto it = blocks_in_erase_order.begin();
    while (it != blocks_in_erase_order.end()) {
        llvm::BasicBlock* block = *it;
        ++it;
        if (block == &*begin && !erase_begin) {
            continue;
        }
        llvm::dbgs() << "Erase block " << block->getName() << "\n";
        block->eraseFromParent();
    }
    // all predecessors of dummy_block were the ones erased from snippet
    for (auto dummy_use : dummy_block->users()) {
        dummy_use->dropAllReferences();
    }
    assert(dummy_block->user_empty());
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
        arg_values[i] = val;
        ++i;
        auto type = get_value_type(val);
        if (!llvm::dyn_cast<llvm::AllocaInst>(val)) {
            arg_types.push_back(type);
        } else if (type->isArrayTy()) {
            llvm::dbgs() << "Do not extract function with array type\n";
            return nullptr;
        } else {
            arg_types.push_back(type->getPointerTo());
        }
    }
    llvm::ArrayRef<llvm::Type*> params(arg_types);
    llvm::FunctionType* f_type = llvm::FunctionType::get(return_type, params, false);
    return f_type;
}

}

InstructionsSnippet::InstructionsSnippet()
    : m_block(nullptr)
    , m_begin_idx(-1)
    , m_end_idx(-1)
{
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
    compute_indices();
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

unsigned InstructionsSnippet::get_instructions_number() const
{
    return m_instruction_number;
}

bool InstructionsSnippet::contains_instruction(llvm::Instruction* instr) const
{
    if (instr->getParent() != m_block) {
        return false;
    }
    int instr_idx = Utils::get_instruction_index(instr);
    return m_begin_idx <= instr_idx  && instr_idx <= m_end_idx;
}

bool InstructionsSnippet::contains_block(llvm::BasicBlock* block) const
{
    return false;
}

bool InstructionsSnippet::intersects(const Snippet& snippet) const
{
    assert(snippet.is_valid_snippet());
    //llvm::dbgs() << "Intersects " << m_block->getName() << " \n";
    auto instr_snippet = const_cast<Snippet&>(snippet).to_instrSnippet();
    if (instr_snippet) {
        if (m_block != instr_snippet->get_block()) {
            return false;
        }
        int self_end_indx = (m_end == m_block->end()) ? m_end_idx : m_end_idx + 1;
        int snippet_end_indx = (instr_snippet->get_end() == m_block->end()) ? instr_snippet->get_end_index()
                                                                            : instr_snippet->get_end_index() + 1;
        bool result = instr_snippet->get_begin_index() <= self_end_indx && m_begin_idx <= snippet_end_indx;
        return result;
    }
    // redirect to block snippet function
    return snippet.intersects(*this);
}

Snippet::InstructionSet InstructionsSnippet::expand()
{
    InstructionSet instructions;
    snippet_instructions(instructions);
    auto it = m_end;
    InstructionSet new_instructions;
    do {
        llvm::Instruction* instr = &*it;
        expand_for_instruction(instr, instructions, new_instructions);
        // not to decrement begin
        if (it == m_begin) {
            break;
        }
    } while (it-- != m_begin);
    return new_instructions;
}

void InstructionsSnippet::adjust_end()
{
    // do not end instruction snippet with branch instruction, if the blocks of branch are not included in a snippet
    if (m_end != m_block->end()) {
        llvm::Instruction* end_instr = &*m_end;
        if (llvm::dyn_cast<llvm::BranchInst>(end_instr)) {
            auto prev_node = m_block->getInstList().getPrevNode(*m_end);
            if (prev_node) {
                m_end = prev_node->getIterator();
                compute_indices();
            }
        }
    }
}

void InstructionsSnippet::collect_used_values(const Snippet* parent_snippet)
{
    m_used_values.clear();
    if (m_end == m_block->end()) {
        collect_values(m_begin, m_end, !parent_snippet ? *this : *parent_snippet, m_used_values);
    } else {
        // not to increment actual end
        auto end = m_end;
        collect_values(m_begin, ++end, !parent_snippet ? *this : *parent_snippet, m_used_values);
    }
}

bool InstructionsSnippet::merge(const Snippet& snippet)
{
    if (!intersects(snippet)) {
        llvm::dbgs() << "Snippets do not intersect. Will not merge\n";
        return false;
    }
    //if (m_block->getParent()->getName() == "") {
    //    llvm::dbgs() << "Merging\n";
    //    dump();
    //    llvm::dbgs() << "WITH\n";
    //    snippet.dump();
    //}
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
        return true;
    }
    return false;
    // do not merge instruction snippet with block snippet,
    // as block snippet should be turn into instruction snippet
}

bool InstructionsSnippet::can_erase_snippet() const
{
    auto it = m_begin;
    auto end = m_end;
    if (m_end != m_block->end()) {
        ++end;
    }
    while (it != end) {
        for (const auto& user : it->users()) {
            if (auto* instr = llvm::dyn_cast<llvm::Instruction>(user)) {
                if (!contains_instruction(instr)) {
                    llvm::dbgs() << "does not contain use of instruction " << *it << "  " << *instr << "\n";
                    return false;
                }
            }
        }
        for (const auto& op : it->operands()) {
            if (auto* instr = llvm::dyn_cast<llvm::Instruction>(op)) {
                if (!contains_instruction(instr)
                        && m_used_values.find(instr) == m_used_values.end()
                        && m_allocas_to_extract.find(instr) == m_allocas_to_extract.end()) {
                    llvm::dbgs() << "does not contain operand of " << *it << "  " << *instr << "\n";
                    return false;
                }
                if (auto* phi = llvm::dyn_cast<llvm::PHINode>(instr)) {
                    // phi node incomming blocks can not be m_block anyway
                    llvm::dbgs() << "Phi node, can not extract " << *it << "\n";
                    return false;
                }
            } else if (auto* bb = llvm::dyn_cast<llvm::BasicBlock>(op)) {
                if (m_block != bb) {
                    llvm::dbgs() << "does not contain block operand of " << *it << "  " << bb->getName() << "\n";
                    return false;
                }
            }
        }
        ++it;
    }
    return true;
}

llvm::Function* InstructionsSnippet::to_function()
{
    // update begin and end indices
    compute_indices();
    m_used_values.clear();
    collect_used_values(nullptr);
    collect_locally_used_allocas(*this, m_used_values, m_allocas_to_extract);
    if (!can_erase_snippet()) {
        llvm::dbgs() << "Can not extract snippet as a function\n";
        return nullptr;
    }
    // create function type
    llvm::LLVMContext& Ctx = m_block->getModule()->getContext();
    // maps argument index to corresponding value
    ArgIdxToValueMap arg_index_to_value;
    llvm::Type* return_type = m_returnInst ? m_block->getParent()->getReturnType() : llvm::Type::getVoidTy(Ctx);
    llvm::FunctionType* type = create_function_type(Ctx, m_used_values, return_type, arg_index_to_value);
    if (!type) {
        return nullptr;
    }

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

    std::unordered_map<llvm::Value*, int> operand_indices;
    setup_function_mappings(new_F, arg_index_to_value, value_ptr_map, value_map, operand_indices);
    llvm::BasicBlock* entry_block = &new_F->getEntryBlock();

    unsigned setup_size = entry_block->size();
    llvm::ValueToValueMapTy value_to_value_map;
    create_value_to_value_map(value_ptr_map, value_to_value_map);
    clone_allocas(entry_block, m_allocas_to_extract, value_to_value_map);
    clone_snippet_to_function(entry_block, m_begin, m_end, value_to_value_map);
    remap_instructions_in_new_function(entry_block, setup_size, value_to_value_map);
    if ((!new_F->getReturnType() || new_F->getReturnType()->isVoidTy()) && !entry_block->getTerminator()) {
        auto retInst = llvm::ReturnInst::Create(new_F->getParent()->getContext());
        entry_block->getInstList().push_back(retInst);
    }
    create_return_stores(entry_block, value_map, operand_indices);

    auto insert_before = m_begin;
    auto callInst = create_call_to_snippet_function(new_F, &*insert_before, true, arg_index_to_value, value_ptr_map);
    if (m_returnInst) {
        if (!return_type->isVoidTy()) {
            auto ret = llvm::ReturnInst::Create(Ctx, callInst, m_block);
        }   else {
            auto ret = llvm::ReturnInst::Create(Ctx, m_block);
        }
    }
    erase_instruction_snippet(m_block, m_begin, m_end);
    erase_instructions(m_allocas_to_extract);
    // **** DEBUG
    //if (m_block->getParent()->getName() == "") {
    //    llvm::dbgs() << "After extraction: \n";
    //    llvm::dbgs() << *m_block->getParent() << "\n\n";

    //    llvm::dbgs() << "Extracted function\n";
    //    llvm::dbgs() << *new_F << "\n\n";
    //}
    // **** DEBUG END
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
    if (!valid) {
        return valid;
    }
    valid &= (begin->getParent() == block);
    if (end != block->end()) {
        valid &= (end->getParent() == block);
    }
    return valid;
}

llvm::BasicBlock* InstructionsSnippet::get_begin_block() const
{
    return m_block;
}

llvm::BasicBlock* InstructionsSnippet::get_end_block() const
{
    return m_block;
}

bool InstructionsSnippet::is_single_instr_snippet() const
{
    return m_begin == m_end;
}

bool InstructionsSnippet::is_function() const
{
    if (!is_valid_snippet()) {
        return false;
    }
    llvm::Function* F = m_block->getParent();
    if (F->getBasicBlockList().size() > 1) {
        return false;
    }
    if (is_block()) {
        return m_block->getInstList().size() == F->getEntryBlock().getInstList().size();
    }
    return false;
}

void InstructionsSnippet::compute_indices()
{
    m_begin_idx = Utils::get_instruction_index(&*m_begin);
    m_end_idx = Utils::get_instruction_index(&*m_end);
    m_instruction_number = m_end_idx - m_begin_idx;
}

void InstructionsSnippet::snippet_instructions(InstructionSet& instrs) const
{
    std::for_each(m_begin, m_end, [&instrs] (llvm::Instruction& instr) { instrs.insert(&instr); });
    instrs.insert(&*m_end);
}

void InstructionsSnippet::expand_for_instruction(llvm::Instruction* instr,
                                                 InstructionSet& instructions,
                                                 InstructionSet& new_instructions)
{
    //llvm::dbgs() << "expand for instr " << *instr << "\n";
    if (auto load = llvm::dyn_cast<llvm::LoadInst>(instr)) {
        assert(instructions.find(instr) != instructions.end());
        if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(load->getPointerOperand())) {
        } else if (auto loaded_inst = llvm::dyn_cast<llvm::Instruction>(load->getPointerOperand())) {
            expand_for_instruction_operand(loaded_inst, instructions, new_instructions);
        }
        return;
    }
    if (auto store = llvm::dyn_cast<llvm::StoreInst>(instr)) {
        auto value_op = store->getValueOperand();
        if (llvm::dyn_cast<llvm::AllocaInst>(value_op)) {
            return;
        }
        expand_for_instruction_operand(value_op, instructions, new_instructions);
        auto storeTo = store->getPointerOperand();
        // e.g. for pointer loadInst will be pointer operand, but it should not be used as value
        if (llvm::dyn_cast<llvm::AllocaInst>(storeTo)) {
        } else {
            expand_for_instruction_operand(storeTo, instructions, new_instructions);
        }
    } else if (auto* callInst = llvm::dyn_cast<llvm::CallInst>(instr)) {
        for (int i = callInst->getNumArgOperands() -1; i >= 0; --i) {
            expand_for_instruction_operand(callInst->getArgOperand(i), instructions, new_instructions);
        }
    } else {
        for (unsigned i = 0; i < instr->getNumOperands(); ++i) {
            expand_for_instruction_operand(instr->getOperand(i), instructions, new_instructions);
        }
    }
}

void InstructionsSnippet::expand_for_instruction_operand(llvm::Value* val,
                                                         InstructionSet& instructions,
                                                         InstructionSet& new_instructions)
{
    auto instr = llvm::dyn_cast<llvm::Instruction>(val);
    if (!instr) {
        return;
    }
    if (llvm::dyn_cast<llvm::AllocaInst>(val)) {
        return;
    }
    if (instr->getParent() != m_block) {
        llvm::dbgs() << "Operand is in a different block " << *val << ". Do not expand\n";
    }
    auto new_begin = instr->getIterator();
    auto new_begin_idx = Utils::get_instruction_index(&*new_begin);
    if (m_begin_idx > new_begin_idx + 1) {
        //llvm::dbgs() << "More than one instruction to expand to " << *val << ". Do not expand\n";
        return;
    }
    auto res = instructions.insert(instr);
    if (!res.second) {
        return;
    }
    new_instructions.insert(instr);
    //llvm::dbgs() << "Expand: add " << *instr << "\n";
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
    if (m_start.is_valid_snippet() && !m_start.is_block()) {
        m_blocks.erase(m_start.get_block());
    }
    if (m_start.is_valid_snippet()) {
        m_instruction_number += m_start.get_instructions_number();
    }
    for (const auto& block : m_blocks) {
        m_instruction_number += block->getInstList().size();
    }
}

BasicBlocksSnippet::BasicBlocksSnippet(llvm::Function* function,
                                       iterator begin,
                                       iterator end,
                                       const BlockSet& blocks,
                                       InstructionsSnippet start)
    : m_function(function)
    , m_begin(begin)
    , m_end(end)
    , m_blocks(blocks)
    , m_start(start)

{
    if (m_start.is_valid_snippet() && !m_start.is_block()) {
        m_blocks.erase(m_start.get_block());
    }
    if (m_start.is_valid_snippet()) {
        m_instruction_number += m_start.get_instructions_number();
    }
    for (const auto& block : m_blocks) {
        m_instruction_number += block->getInstList().size();
    }
}

bool BasicBlocksSnippet::is_valid_snippet() const
{
    return m_function && BasicBlocksSnippet::is_valid_snippet(m_begin, m_end, m_function);
}

unsigned BasicBlocksSnippet::get_instructions_number() const
{
    return m_instruction_number;
}

bool BasicBlocksSnippet::contains_instruction(llvm::Instruction* instr) const
{
    if (m_start.is_valid_snippet() && m_start.contains_instruction(instr)) {
        return true;
    }
    if (m_tail.is_valid_snippet() && m_tail.contains_instruction(instr)) {
        return true;
    }
    llvm::BasicBlock* instr_parent = instr->getParent();
    if (m_blocks.find(instr_parent) == m_blocks.end()) {
        return false;
    }
    if (m_start.is_valid_snippet() && instr_parent == m_start.get_block()) {
        return false;
    }
    if (m_tail.is_valid_snippet() && instr_parent == m_tail.get_block()) {
        return false;
    }
    return true;
}

bool BasicBlocksSnippet::contains_block(llvm::BasicBlock* block) const
{
    bool contains = m_blocks.find(block) != m_blocks.end();
    if (m_start.is_valid_snippet()) {
        contains &= (block != m_start.get_block());
    }
    return contains;
}

bool BasicBlocksSnippet::is_function() const
{
    if (m_start.is_valid_snippet() || m_tail.is_valid_snippet()) {
        return false;
    }
    for (auto& block : *m_function) {
        if (m_blocks.find(&block) == m_blocks.end()) {
            return false;
        }
    }
    return true;
}

bool BasicBlocksSnippet::intersects(const Snippet& snippet) const
{
    //llvm::dbgs() << "Intersects " << get_begin_block()->getName() << " \n";
    auto block_snippet = const_cast<Snippet&>(snippet).to_blockSnippet();
    if (block_snippet) {
        bool result = contains_block(block_snippet->get_begin_block())
                        || contains_block(block_snippet->get_end_block());
        if (!result) {
            result |= (is_predecessing_block_snippet(*this, *block_snippet) &&
                    (!block_snippet->get_start_snippet().is_valid_snippet() ||
                    block_snippet->get_start_snippet().is_block()));
        }
        if (!result) {
            result |= (is_predecessing_block_snippet(*block_snippet, *this) &&
                    (!m_start.is_valid_snippet() || m_start.is_block()));
        }
        return result;
    }
    auto instr_snippet = const_cast<Snippet&>(snippet).to_instrSnippet();
    if (m_start.is_valid_snippet() && m_start.intersects(snippet)) {
        return true;
    }
    if (m_tail.is_valid_snippet() && m_tail.intersects(snippet)) {
        return true;
    }
    if (contains_block(instr_snippet->get_block())) {
        return true;
    }
    if (instr_snippet->get_begin_index() == 0
            && instr_snippet->is_block()
            && is_predecessing_block_snippet(*this, *instr_snippet)) {
        return true;
    }
    return false;
}

Snippet::InstructionSet BasicBlocksSnippet::expand()
{
    if (!m_start.is_valid_snippet()) {
        return Snippet::InstructionSet();
    }
    const auto& new_instructions = m_start.expand();
    // can include block in snippet
    if (m_start.is_block()) {
        //m_begin = Utils::get_block_pos(m_start.get_block());
        m_begin = m_start.get_block()->getIterator();
        m_start.clear();
    }
    return new_instructions;
}

void BasicBlocksSnippet::adjust_end()
{
    // Nothing to do for now
}

void BasicBlocksSnippet::collect_used_values(const Snippet* parent_snippet)
{
    //if (!m_used_values.empty()) {
    //    return;
    //}
    // ignoring parent_snippet
    m_used_values.clear();
    if (m_start.is_valid_snippet()) {
        m_start.collect_used_values(this);
        auto used_in_start = m_start.get_used_values();
        m_used_values.insert(used_in_start.begin(), used_in_start.end());
    }

    for (const auto& block : m_blocks) {
        if (m_start.is_valid_snippet() && block == m_start.get_block()) {
            continue;
        }
        collect_values(block->begin(), block->end(), *this, m_used_values);
    }
    if (m_blocks.find(&*m_begin) == m_blocks.end()) {
        collect_values(m_begin->begin(), m_begin->end(), *this, m_used_values);
    }
   if (m_tail.is_valid_snippet()) {
        m_tail.collect_used_values(this);
        auto used_in_start = m_tail.get_used_values();
        m_used_values.insert(used_in_start.begin(), used_in_start.end());
    }
}

bool BasicBlocksSnippet::merge(const Snippet& snippet)
{
    if (!intersects(snippet)) {
        llvm::dbgs() << "Snippets do not intersect. Will not merge\n";
        return false;
    }
    //if (m_function->getName() == "") {
    //    llvm::dbgs() << "Merging\n";
    //    dump();
    //    llvm::dbgs() << "WITH\n";
    //    snippet.dump();
    //}
    auto instr_snippet = const_cast<Snippet&>(snippet).to_instrSnippet();
    if (instr_snippet) {
        if (instr_snippet->get_begin_index() == 0 && is_predecessing_block_snippet(*this, *instr_snippet)) {
            if (instr_snippet->is_block()) {
                m_end = Utils::get_block_pos(instr_snippet->get_block());
                m_blocks.insert(&*m_end);
            } else {
                m_tail = *instr_snippet;
            }
            return true;
        } else if (m_tail.is_valid_snippet()) {
            bool result = m_tail.merge(snippet);
            if (result && m_tail.is_block()) {
                m_end = Utils::get_block_pos(m_tail.get_block());
                m_blocks.insert(&*m_end);
                m_tail.clear();
            }
            return result;
        } else if (m_start.is_valid_snippet()) {
            bool result = m_start.merge(snippet);
            if (result&& m_start.is_block()) {
                m_begin = Utils::get_block_pos(m_start.get_block());
                m_blocks.insert(&*m_begin);
                m_start.clear();
            }
            return result;
        }
        return false;
    }
    bool modified = false;
    auto block_snippet = const_cast<Snippet&>(snippet).to_blockSnippet();
    if (m_start.is_valid_snippet()) {
        m_start.merge(block_snippet->get_start_snippet());
        //m_start.dump();
    }
    if (!contains_block(block_snippet->get_begin_block()) && block_snippet->contains_block(&*m_begin)) {
        m_begin = block_snippet->get_begin();
        if (block_snippet->get_start_snippet().is_valid_snippet()) {
            m_start = block_snippet->get_start_snippet();
            modified = true;
        }
    }
    if (!contains_block(block_snippet->get_end_block()) && block_snippet->contains_block(&*m_end)) {
        m_end = block_snippet->get_end();
        modified = true;
    }
    if (!modified) {
        // check for case when one snippet continues the other
        // check if begin of the snippet is successor of the end of this
        if (is_predecessing_block_snippet(*this, *block_snippet) &&
            ((block_snippet->get_start_snippet().is_valid_snippet() && block_snippet->get_start_snippet().is_block()) ||
            !block_snippet->get_start_snippet().is_valid_snippet())) {
            m_end = block_snippet->get_end();
            modified = true;
        } else if (is_predecessing_block_snippet(*block_snippet, *this) && ((m_start.is_valid_snippet() &&
        m_start.is_block()) || !m_start.is_valid_snippet())) {
            m_begin = block_snippet->get_begin();
            m_start = block_snippet->get_start_snippet();
            modified = true;
        }
    }
    if (modified) {
        m_blocks.clear();
        m_blocks = Utils::get_blocks_in_range(m_begin, m_end);
        return true;
    }
    return false;
}

bool BasicBlocksSnippet::can_erase_instruction_range(llvm::BasicBlock::iterator begin,
                                                     llvm::BasicBlock::iterator end) const
{
    auto it = begin;
    while (it != end) {
        for (const auto& user : it->users()) {
            if (auto* instr = llvm::dyn_cast<llvm::Instruction>(user)) {
                if (!contains_instruction(instr)) {
                    llvm::dbgs() << "does not contain use of instruction " << *it << "  " << *instr << "\n";
                    return false;
                }
            } else if (auto* b = llvm::dyn_cast<llvm::BasicBlock>(user)) {
                if (!contains_block(b)) {
                    llvm::dbgs() << "does not contain block use of instruction " << *it << "  " << b->getName() << "\n";
                    return false;
                }
            }
        }
        ++it;
    }
    if (end != begin->getParent()->end()) {
        for (const auto& user : end->users()) {
            if (auto* instr = llvm::dyn_cast<llvm::Instruction>(user)) {
                if (!contains_instruction(instr)) {
                    llvm::dbgs() << "does not contain use of instruction " << *it << "  " << *instr << "\n";
                    return false;
                }
            } else if (auto* b = llvm::dyn_cast<llvm::BasicBlock>(user)) {
                if (!contains_block(b)) {
                    llvm::dbgs() << "does not contain block use of instruction " << *it << "  " << b->getName() << "\n";
                    return false;
                }
            }
        }
    }
    return true;
}

bool BasicBlocksSnippet::can_erase_block(llvm::BasicBlock* block) const
{
    for (const auto& user : block->users()) {
        if (auto* instr = llvm::dyn_cast<llvm::Instruction>(user)) {
            if (!contains_instruction(instr)) {
                llvm::dbgs() << "does not contain use of block " << block->getName() << "  " << *instr << "\n";
                return false;
            }
        } else if (auto* b = llvm::dyn_cast<llvm::BasicBlock>(user)) {
            if (!contains_block(b)) {
                llvm::dbgs() << "does not contain block use of block " << block->getName() << "  " << b->getName() << "\n";
                return false;
            }
        }
    }
    return true;
}

llvm::BasicBlock* BasicBlocksSnippet::find_return_block() const
{
    for (const auto& B : m_blocks) {
        if (auto* ret = llvm::dyn_cast<llvm::ReturnInst>(B->getTerminator())) {
            return B;
        }
    }
    return nullptr;
}

bool BasicBlocksSnippet::can_erase_block_snippet() const
{
    if (m_start.is_valid_snippet()) {
        if (!can_erase_instruction_range(m_start.get_begin(), m_start.get_end())) {
            return false;
        }
    }
    for (const auto& block : m_blocks) {
        if (m_start.is_valid_snippet() && block == m_start.get_block()) {
            continue;
        }
        if (!m_start.is_valid_snippet() && block == &*m_begin) {
            continue;
        }
        if (!can_erase_block(block)) {
            return false;
        }
        if (!can_erase_instruction_range(block->begin(), block->end())) {
            return false;
        }
    }
    if (m_tail.is_valid_snippet()) {
        if (!can_erase_instruction_range(m_tail.get_begin(), m_tail.get_end())) {
            return false;
        }
    }
    if (m_end != m_function->end() && m_blocks.find(&*m_end) == m_blocks.end()) {
        if (m_end->isLandingPad()) {
            llvm::dbgs() << "End block is landing pad block and is not contained in the snippet. Do not extract\n";
            return false;
        }
    }
    return true;
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
    if (m_start.is_valid_snippet()) {
        m_start.compute_indices();
    }
    // erase end if end is exit block of a function and is fully contained in a snippet
    llvm::BasicBlock* returnBlock = find_return_block();
    bool has_return_terminator = returnBlock != nullptr;
    llvm::ReturnInst* ret_inst;
    if (has_return_terminator) {
        ret_inst = llvm::dyn_cast<llvm::ReturnInst>(returnBlock->getTerminator());
    }
    has_return_terminator = has_return_terminator && (ret_inst && contains_instruction(ret_inst));
    bool ends_function = (m_end == m_function->end() || has_return_terminator);
    m_blocks = Utils::get_blocks_in_range(m_begin, m_end);
    if (ends_function) {
        m_blocks.insert(&*m_end);
    }
    collect_used_values(nullptr);
    collect_locally_used_allocas(*this, m_used_values, m_allocas_to_extract);
    if (!can_erase_block_snippet()) {
        llvm::dbgs() << "Can not extract snippet as a function\n";
        return nullptr;
    }
    auto blocks_in_erase_order = Utils::get_blocks_in_bfs(m_begin, m_end);
    llvm::LLVMContext& Ctx = m_function->getParent()->getContext();
    llvm::Type* return_type = llvm::Type::getVoidTy(Ctx);
    if (ends_function) {
//        llvm::dbgs() << "Snippet ends function\n";
        return_type = m_function->getReturnType();
    }

    // maps argument index to corresponding value
    ArgIdxToValueMap arg_index_to_value;
    llvm::FunctionType* type = create_function_type(Ctx, m_used_values, return_type, arg_index_to_value);
    if (!type) {
        return nullptr;
    }

    std::string f_name = unique_name_generator::get().get_unique(m_begin->getParent()->getName());
    llvm::Function* new_F = llvm::Function::Create(type,
                                                   llvm::GlobalValue::LinkageTypes::ExternalLinkage,
                                                   f_name,
                                                   m_function->getParent());
    ValueToValueMap value_ptr_map;
    ValueToValueMap value_map;
    std::unordered_map<llvm::Value*, int> operand_indices;
    setup_function_mappings(new_F, arg_index_to_value, value_ptr_map, value_map, operand_indices);
    const bool has_start_snippet = m_start.is_valid_snippet();
    llvm::ValueToValueMapTy value_to_value_map;
    create_value_to_value_map(value_ptr_map, value_to_value_map);
    llvm::BasicBlock* entry_block = &new_F->getEntryBlock();
    if (has_start_snippet) {
        value_to_value_map.insert(std::make_pair(m_start.get_block(), llvm::WeakTrackingVH(entry_block)));
    }
    // this function will also create new exit block
    bool has_tail_snippet = m_tail.is_valid_snippet();
    if (m_blocks.find(m_tail.get_block()) != m_blocks.end()) {
        has_tail_snippet &= (m_end != m_function->end() && m_tail.get_block() == &*m_end);
    }
//    has_tail_snippet &= (m_blocks.find(m_tail.get_block()) != m_blocks.end() && m_end != m_function->end() && m_tail.get_block() == &*m_end);
    if (has_tail_snippet) {
        //llvm::dbgs() << "Create empty tail block\n";
        llvm::BasicBlock* tail_block = llvm::BasicBlock::Create(new_F->getContext(), m_tail.get_block()->getName());
        new_F->getBasicBlockList().push_back(tail_block);
        value_to_value_map.insert(std::make_pair(m_tail.get_block(), llvm::WeakTrackingVH(tail_block)));
    }
    //llvm::dbgs() << "   Clone blocks\n";
    clone_allocas(entry_block, m_allocas_to_extract, value_to_value_map);
    const auto& cloned_blocks = clone_blocks_snippet_to_function(new_F, m_blocks, m_begin, m_end,
                                     !has_start_snippet,
                                     !has_tail_snippet,
                                     value_to_value_map);
    unsigned setup_size = entry_block->size();
    if (has_start_snippet) {
        //llvm::dbgs() << "   Clone start snippet\n";
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
    llvm::remapInstructionsInBlocks(cloned_blocks, value_to_value_map);
    llvm::BasicBlock* new_function_exit_block = nullptr;
    if (has_tail_snippet) {
        //llvm::dbgs() << "   Clone tail snippet\n";
        auto tail_block_pos = value_to_value_map.find(m_tail.get_block());
        llvm::BasicBlock* tail_block = llvm::dyn_cast<llvm::BasicBlock>(&*tail_block_pos->second);
        clone_snippet_to_function(tail_block, m_tail.get_begin(), m_tail.get_end(), value_to_value_map);
        remap_instructions_in_new_function(tail_block, 0, value_to_value_map);
        new_function_exit_block = tail_block;
        if (!new_function_exit_block->getTerminator()) {
            auto retInst = llvm::ReturnInst::Create(new_F->getParent()->getContext());
            new_function_exit_block->getInstList().push_back(retInst);
        }
    }
    if (has_return_terminator) {
        auto ret_pos = value_to_value_map.find(&*returnBlock);
        new_function_exit_block = llvm::dyn_cast<llvm::BasicBlock>(&*ret_pos->second);
    } else if (has_tail_snippet) {
        auto tail_block_pos = value_to_value_map.find(m_tail.get_block());
        llvm::BasicBlock* tail_block = llvm::dyn_cast<llvm::BasicBlock>(&*tail_block_pos->second);
        new_function_exit_block = tail_block;
        if (!new_function_exit_block->getTerminator()) {
            auto retInst = llvm::ReturnInst::Create(new_F->getParent()->getContext());
            new_function_exit_block->getInstList().push_back(retInst);
        }
    } else if (!ends_function) {
        auto exit_block_entry = value_to_value_map.find(&*m_end);
        assert(exit_block_entry != value_to_value_map.end());
        llvm::BasicBlock* exit_block = llvm::dyn_cast<llvm::BasicBlock>(&*exit_block_entry->second);
        assert(exit_block);
        create_new_exit_block(new_F, exit_block);
        new_function_exit_block = &new_F->back();
    } else {
        auto end_pos = value_to_value_map.find(&*m_end);
        new_function_exit_block = llvm::dyn_cast<llvm::BasicBlock>(&*end_pos->second);
    }
    create_return_stores(new_function_exit_block, value_map, operand_indices);

    llvm::CallInst* call;
    if (has_start_snippet) {
        auto insert_before = m_start.get_begin();
        //llvm::dbgs() << "  Create call to new function in start snippet\n";
        call = create_call_to_snippet_function(new_F, &*insert_before, true, arg_index_to_value, value_ptr_map);
        if (ends_function) {
            llvm::IRBuilder<> builder(&*insert_before);
            if (ret_inst && !m_function->getReturnType()->isVoidTy()) {
                builder.CreateRet(call);
            } else {
                builder.CreateRetVoid();
            }
            //llvm::dbgs() << "  Erase start snippet from original function\n";
            erase_instruction_snippet(m_start.get_block(), m_start.get_begin(), m_start.get_end());
        } else {
            //llvm::dbgs() << "  Erase start snippet from original function \n";
            erase_instruction_snippet(m_start.get_block(), m_start.get_begin(), m_start.get_end());
            auto branch_inst = llvm::BranchInst::Create(&*m_end);
            m_start.get_block()->getInstList().push_back(branch_inst);
        }
        call = nullptr;
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
        //llvm::dbgs() << "  Create call to new function in end block\n";
        call = create_call_to_snippet_function(new_F, &*insert_before, true, arg_index_to_value, value_ptr_map);
        if (ends_function) {
            call_block->getInstList().pop_back();
            if (ret_inst && !m_function->getReturnType()->isVoidTy()) {
                call_block->getInstList().push_back(llvm::ReturnInst::Create(Ctx, call));
            } else {
                call_block->getInstList().push_back(llvm::ReturnInst::Create(Ctx));
            }
        }
    }
    if (has_tail_snippet) {
        //llvm::dbgs() << "  Erase tail snippet from original function\n";
        erase_instruction_snippet(m_tail.get_block(), m_tail.get_begin(), m_tail.get_end());
    }
    if (ends_function) {
        blocks_in_erase_order.push_back(&*m_end);
    }
    llvm::dbgs() << "  Erase blocks from original function\n";
    erase_block_snippet(m_function, !has_start_snippet, ends_function,
                        m_begin, m_end, m_blocks, blocks_in_erase_order);

    erase_instructions(m_allocas_to_extract);
    if (m_function->hasPersonalityFn()) {
        new_F->setPersonalityFn(m_function->getPersonalityFn());
    }
    // **** DEBUG
    //if (m_function->getName() == "" || m_function->getName() == "") {
    //    llvm::dbgs() << "After extraction: \n";
    //    llvm::dbgs() << *m_function << "\n\n";

    //    llvm::dbgs() << "Extracted function\n";
    //    llvm::dbgs() << *new_F << "\n\n";
    //}
    // **** DEBUG END
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

llvm::BasicBlock* BasicBlocksSnippet::get_begin_block() const
{
    return &*m_begin;
}

llvm::BasicBlock* BasicBlocksSnippet::get_end_block() const
{
    return &*m_end;
}

const InstructionsSnippet& BasicBlocksSnippet::get_start_snippet() const
{
    return m_start;
}

BasicBlocksSnippet* BasicBlocksSnippet::to_blockSnippet()
{
    return this;
}

void BasicBlocksSnippet::dump() const
{
    llvm::dbgs() << "****Block snippet*****\n";
    if (m_start.is_valid_snippet()) {
        llvm::dbgs() << "    Start \n";
        m_start.dump();
    }
    if (m_begin->getName().empty()) {
        llvm::dbgs() << "Begin block " <<  *m_begin << "\n";
    } else {
        llvm::dbgs() << "Begin block " << m_begin->getName() << "\n";
    }
    for (const auto& b : m_blocks) {
        if (b->getName().empty()) {
            llvm::dbgs() << "BB: " << *b << "\n";
        } else {
            llvm::dbgs() << b->getName() << "\n";
        }
    }
    if (m_end != m_begin->getParent()->end()) {
        if (m_end->getName().empty()) {
            llvm::dbgs() << "End block " <<  *m_end << "\n";
        } else {
            llvm::dbgs() << "End block " << m_end->getName() << "\n";
        }
    }
    if (m_tail.is_valid_snippet()) {
        llvm::dbgs() << "    Tail \n";
        m_tail.dump();
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

