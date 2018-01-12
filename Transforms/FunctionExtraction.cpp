#include "FunctionExtraction.h"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"

#include "llvm/PassRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "FunctionSnippet.h"
#include "Utils.h"
#include "Analysis/FunctionAnaliser.h"
#include "Analysis/InputDependentFunctionAnalysisResult.h"
#include "Analysis/BasicBlocksUtils.h"

#include <vector>
#include <memory>

namespace oh {

namespace {

class SnippetsCreator
{
public:
    using InputDependencyAnalysisInfo = input_dependency::InputDependencyAnalysisInterface::InputDepResType;
    using Snippet_type = std::shared_ptr<Snippet>;
    using BasicBlockRange = std::pair<BasicBlocksSnippet::iterator, BasicBlocksSnippet::iterator>;
    using snippet_list = std::vector<Snippet_type>;

public:
    SnippetsCreator(llvm::Function& F)
        : m_F(F)
    {
    }

    void set_input_dep_info(const InputDependencyAnalysisInfo& info)
    {
        m_input_dep_info = info;
    }

    void set_post_dom_tree(llvm::PostDominatorTree* pdom)
    {
        m_pdom = pdom;
    }

    const snippet_list& get_snippets() const
    {
        return m_snippets;
    }

public:
    void collect_snippets(bool expand);
    void expand_snippets();

private:
    snippet_list collect_block_snippets(llvm::Function::iterator block_it);
    template <class T>
    bool derive_input_dependency_from_args(T* I) const;
    bool can_root_blocks_snippet(llvm::BasicBlock* block) const;
    BasicBlockRange get_blocks_snippet(llvm::Function::iterator begin_block_pos);
    llvm::BasicBlock* find_block_postdominator(llvm::BasicBlock* block);
    void update_processed_blocks(const llvm::BasicBlock* block,
                                 const llvm::BasicBlock* stop_block,
                                 std::unordered_set<const llvm::BasicBlock*>& processed_blocks);

private:
    llvm::Function& m_F;
    InputDependencyAnalysisInfo m_input_dep_info;
    llvm::PostDominatorTree* m_pdom;
    snippet_list m_snippets;
};

void SnippetsCreator::collect_snippets(bool expand)
{
    std::unordered_set<const llvm::BasicBlock*> processed_blocks;
    auto it = m_F.begin();
    while (it != m_F.end()) {
        auto B = &*it;
        if (input_dependency::BasicBlocksUtils::get().isBlockUnreachable(B)) {
            ++it;
            continue;
        }
        auto pos = processed_blocks.find(B);
        if (pos != processed_blocks.end()) {
            ++it;
            continue;
        }
        auto block_snippets = collect_block_snippets(it);
        if (!can_root_blocks_snippet(B)) {
            ++it;
            processed_blocks.insert(B);
            m_snippets.insert(m_snippets.end(), block_snippets.begin(), block_snippets.end());
            continue;
        }
        // assert back end iter is block's terminator
        auto blocks_range = get_blocks_snippet(it);
        if (!BasicBlocksSnippet::is_valid_snippet(blocks_range.first, blocks_range.second, &m_F)) {
            llvm::dbgs() << "Failed to create snippet out of blocks, starting with block "
                         << B->getName() << "\n";
        } else {
            auto back = block_snippets.back();
            block_snippets.pop_back();
            update_processed_blocks(&*blocks_range.first, &*blocks_range.second, processed_blocks);
            Snippet_type blocks_snippet(new BasicBlocksSnippet(&m_F,
                                                               blocks_range.first,
                                                               blocks_range.second,
                                                               *back->to_instrSnippet()));
            block_snippets.push_back(blocks_snippet);
        }
        // for some blocks will run insert twice
        processed_blocks.insert(B);
        m_snippets.insert(m_snippets.end(), block_snippets.begin(), block_snippets.end());
        ++it;
    }
    if (expand) {
        expand_snippets();
    }
}

void SnippetsCreator::expand_snippets()
{
    for (auto& snippet : m_snippets) {
        //snippet->dump();
        snippet->expand();
        snippet->adjust_end();
    }
    if (m_snippets.size() == 1) {
        if ((*m_snippets.begin())->is_single_instr_snippet()) {
            m_snippets.clear();
        }
    }

    auto it = m_snippets.begin();
    std::vector<snippet_list::iterator> to_erase;
    while (it != m_snippets.end()) {
        auto next_it = it + 1;
        if (next_it == m_snippets.end()) {
            // last snippet and is one instruction snippet
            if ((*it)->is_single_instr_snippet()) {
                m_snippets.erase(it);
            }
            break;
        }
        if ((*it)->intersects(**next_it)) {
            if ((*next_it)->merge(**it)) {
                to_erase.push_back(it);
            }
            ++it;
        } else {
            ++it;
        }
    }
    for (auto& elem : to_erase) {
        m_snippets.erase(elem);
    }
}

SnippetsCreator::snippet_list SnippetsCreator::collect_block_snippets(llvm::Function::iterator block_it)
{
    snippet_list snippets;
    auto block = &*block_it;
    InstructionsSnippet::iterator begin = block->end();
    InstructionsSnippet::iterator end = block->end();
    auto it = block->begin();
    while (it != block->end()) {
        llvm::Instruction* I = &*it;
        //llvm::dbgs() << "instr " << *I << "\n";
        bool is_input_dep = m_input_dep_info->isInputDependent(I);
        if (!is_input_dep) {
            // TODO: what other instructions might be intresting?
            if (auto* callInst = llvm::dyn_cast<llvm::CallInst>(I)) {
                llvm::Function* called_f = callInst->getCalledFunction();
                if (called_f && called_f->getReturnType()->isVoidTy()) {
                    is_input_dep = derive_input_dependency_from_args(callInst);
                }
            } else if (auto* invokeInst = llvm::dyn_cast<llvm::InvokeInst>(I)) {
                llvm::Function* called_f = invokeInst->getCalledFunction();
                if (called_f && called_f->getReturnType()->isVoidTy()) {
                    is_input_dep = derive_input_dependency_from_args(invokeInst);
                }
            }
        }
        if (!is_input_dep) {
            if (InstructionsSnippet::is_valid_snippet(begin, end, block)) {
                snippets.push_back(Snippet_type(new InstructionsSnippet(block, begin, end)));
                begin = block->end();
                end = block->end();
            }
        } else {
            if (auto store = llvm::dyn_cast<llvm::StoreInst>(I)) {
                // Skip the instruction storing argument to a local variable. This should happen anyway, no need to extract
                if (llvm::dyn_cast<llvm::Argument>(store->getValueOperand())) {
                    ++it;
                    continue;
                }
            }
            if (begin != block->end()) {
                end = it;
            } else {
                begin = it;
                end = it;
            }
        }
        ++it;
    }
    if (InstructionsSnippet::is_valid_snippet(begin, end, block)) {
        snippets.push_back(Snippet_type(new InstructionsSnippet(block, begin, end)));
    }
    return snippets;
}

template <class T>
bool SnippetsCreator::derive_input_dependency_from_args(T* I) const
{
    // return true if at least one argument are input dependent
    bool is_input_dep = true;
    for (unsigned i = 0; i < I->getNumArgOperands(); ++i) {
        auto op = I->getArgOperand(i);
        if (auto op_inst = llvm::dyn_cast<llvm::Instruction>(op)) {
            is_input_dep = m_input_dep_info->isInputDependent(op_inst);
            if (!is_input_dep) {
                break;
            }
        } else if (llvm::dyn_cast<llvm::Constant>(op)) {
            is_input_dep = false;
            break;
        }
    }
    return is_input_dep;
}

bool SnippetsCreator::can_root_blocks_snippet(llvm::BasicBlock* block) const
{
    auto terminator = block->getTerminator();
    if (!m_input_dep_info->isInputDependent(terminator)) {
        return false;
    }
    auto branch = llvm::dyn_cast<llvm::BranchInst>(terminator);
    if (branch) {
        return branch->isConditional();
    }
    auto switch_inst = llvm::dyn_cast<llvm::SwitchInst>(terminator);
    return (switch_inst != nullptr);
}

SnippetsCreator::BasicBlockRange SnippetsCreator::get_blocks_snippet(llvm::Function::iterator begin_block_pos)
{
    auto end_block = find_block_postdominator(&*begin_block_pos);
    llvm::Function::iterator end_block_pos = Utils::get_block_pos(end_block);
    while (m_input_dep_info->isInputDependentBlock(end_block) && end_block != &end_block->getParent()->back()) {
        end_block = find_block_postdominator(&*end_block_pos);
        end_block_pos = Utils::get_block_pos(end_block);
    }
    return std::make_pair(begin_block_pos, end_block_pos);
}

llvm::BasicBlock* SnippetsCreator::find_block_postdominator(llvm::BasicBlock* block)
{
    const auto& b_node = (*m_pdom)[block];
    auto F = block->getParent();
    auto block_to_process = block;
    std::unordered_set<llvm::BasicBlock*> seen_blocks;
    while (block_to_process != nullptr) {
        if (block_to_process != block) {
            auto pr_node = (*m_pdom)[block_to_process];
            if (m_pdom->dominates(pr_node, b_node)) {
                break;
            }
        }
        auto succ = succ_begin(block_to_process);
        if (succ == succ_end(block_to_process)) {
            // block_to_process is the exit block
            block_to_process = nullptr;
            break;
        }

        llvm::BasicBlock* tmp_block;
        do {
            tmp_block = *succ;
        } while (!seen_blocks.insert(tmp_block).second && ++succ != succ_end(block_to_process));
        block_to_process = tmp_block;
    }
    if (block_to_process == nullptr) {
        block_to_process = &block->getParent()->back();
    }
    assert(block_to_process != nullptr);
    return block_to_process;
}

void SnippetsCreator::update_processed_blocks(const llvm::BasicBlock* block,
                                              const llvm::BasicBlock* stop_block,
                                              std::unordered_set<const llvm::BasicBlock*>& processed_blocks)
{
    if (block == stop_block) {
        return;
    }
    auto res = processed_blocks.insert(block);
    if (!res.second) {
        return; // block is added, hence successors are also processed
    }
    auto it = succ_begin(block);
    while (it != succ_end(block)) {
        update_processed_blocks(*it, stop_block, processed_blocks);
        ++it;
    }
}

void run_on_function(llvm::Function& F,
                     llvm::PostDominatorTree* PDom,
                     const SnippetsCreator::InputDependencyAnalysisInfo& input_dep_info,
                     std::unordered_map<llvm::Function*, unsigned>& extracted_functions)
{
    // map from block to snippets?
    SnippetsCreator creator(F);
    creator.set_input_dep_info(input_dep_info);
    creator.set_post_dom_tree(PDom);
    creator.collect_snippets(true);
    const auto& snippets = creator.get_snippets();

    llvm::dbgs() << "number of snippets " << snippets.size() << "\n";
    for (auto& snippet : snippets) {
        if (snippet->is_single_instr_snippet()) {
            llvm::dbgs() << "Do not extract single instruction snippet\n";
            snippet->dump();
            continue;
        }
        // **** DEBUG
        //if (F.getName() == "") {
        //    llvm::dbgs() << "To Function\n";
        //    snippet->dump();
        //}
        // **** DEBUG END
        auto extracted_function = snippet->to_function();
        //llvm::dbgs() << "Extracted to function " << *extracted_function << "\n";
        extracted_functions.insert(std::make_pair(extracted_function, snippet->get_instructions_number()));
    }
}

} // unnamed namespace


void ExtractionStatistics::report()
{
    write_entry(m_module_name, "NumOfExtractedInst", m_numOfExtractedInst);
    write_entry(m_module_name, "NumOfMediateInst", m_numOfMediateInst);
    write_entry(m_module_name, "ExtractedFuncs", m_extractedFuncs);
    flush();
}

static llvm::cl::opt<bool> stats(
    "extraction-stats",
    llvm::cl::desc("Dump statistics"),
    llvm::cl::value_desc("boolean flag"));

static llvm::cl::opt<std::string> stats_format(
    "extraction-stats-format",
    llvm::cl::desc("Statistics format"),
    llvm::cl::value_desc("format name"));

static llvm::cl::opt<std::string> stats_file(
    "extraction-stats-file",
    llvm::cl::desc("Statistics file"),
    llvm::cl::value_desc("file name"));

char FunctionExtractionPass::ID = 0;

void FunctionExtractionPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<llvm::PostDominatorTreeWrapperPass>();
    // FunctionExtractionPass does not preserve results of InputDependency Analysis.
    // While it adds extracted functions as input dependent functions, the CFG of old functions change, thus input
    // dependency results are invalidated.
    AU.addRequired<input_dependency::InputDependencyAnalysisPass>();
}

bool FunctionExtractionPass::runOnModule(llvm::Module& M)
{
    bool modified = false;
    auto input_dep = getAnalysis<input_dependency::InputDependencyAnalysisPass>().getInputDependencyAnalysis();

    createStatistics(M, *input_dep);
    m_coverageStatistics->setSectionName("input_dep_coverage_before_extraction");
    m_coverageStatistics->reportInputDepFunctionCoverage(false);
    std::unordered_map<llvm::Function*, unsigned> extracted_functions;
    for (auto& F : M) {
        llvm::dbgs() << "\nStart function extraction on function " << F.getName() << "\n";
        if (F.isDeclaration()) {
            llvm::dbgs() << "Skip: Declaration function " << F.getName() << "\n";
            continue;
        }
        auto f_input_dep_info = input_dep->getAnalysisInfo(&F);
        if (f_input_dep_info == nullptr) {
            llvm::dbgs() << "Skip: No input dep info for function " << F.getName() << "\n";
            continue;
        }
        //auto f_input_dep_info = input_dep_info->toFunctionAnalysisResult();
        if (!f_input_dep_info) {
            continue;
        }
        if (f_input_dep_info->isInputDepFunction()) {
            llvm::dbgs() << "Skip: Input dependent function " << F.getName() << "\n";
            continue;
        }
        llvm::PostDominatorTree* PDom = &getAnalysis<llvm::PostDominatorTreeWrapperPass>(F).getPostDomTree();
        run_on_function(F, PDom, f_input_dep_info, extracted_functions);
        modified = true;
        llvm::dbgs() << "Done function extraction on function " << F.getName() << "\n";
    }

    llvm::dbgs() << "\nExtracted functions are \n";
    for (const auto& f : extracted_functions) {
        llvm::Function* extracted_f = f.first;
        m_extracted_functions.insert(extracted_f);
        llvm::dbgs() << extracted_f->getName() << "\n";
        input_dep->insertAnalysisInfo(
                extracted_f, input_dependency::InputDependencyAnalysis::InputDepResType(new
                input_dependency::InputDependentFunctionAnalysisResult(extracted_f)));
        if (stats) {
            unsigned f_instr_num = Utils::get_function_instrs_count(*extracted_f);
            m_extractionStatistics->add_numOfExtractedInst(f.second);
            m_extractionStatistics->add_numOfMediateInst(f_instr_num - f.second);
            m_extractionStatistics->add_extractedFunction(extracted_f->getName());
        }
    }
    m_coverageStatistics->setSectionName("input_dep_coverage_after_extraction");
    m_coverageStatistics->reportInputDepFunctionCoverage(false);
    m_extractionStatistics->report();

    Utils::check_module(M);
    return modified;
}

const std::unordered_set<llvm::Function*>& FunctionExtractionPass::get_extracted_functions() const
{
    return m_extracted_functions;
}

void FunctionExtractionPass::createStatistics(llvm::Module& M, input_dependency::InputDependencyAnalysisInterface& IDA)
{
    if (!stats) {
        m_extractionStatistics = ExtractionStatisticsType(new DummyExtractionStatistics());
        m_coverageStatistics = CoverageStatisticsType(new input_dependency::DummyInputDependencyStatistics());
        return;
    }
    std::string file_name = stats_file;
    if (file_name.empty()) {
        file_name = "stats";
    }
    m_coverageStatistics = CoverageStatisticsType(new input_dependency::InputDependencyStatistics(stats_format, file_name, &M,
                                                                          &IDA.getAnalysisInfo()));
    m_extractionStatistics = ExtractionStatisticsType(new ExtractionStatistics(m_coverageStatistics->getReportWriter()));
    m_extractionStatistics->setSectionName("extraction_stats");
    m_extractionStatistics->set_module_name(M.getName());
}

//Transformation pass to extract input dependent snippets into separate functions
static llvm::RegisterPass<FunctionExtractionPass> X(
                                "extract-functions",
                                "Function Extraction");
}

