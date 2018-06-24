#include "FunctionExtraction.h"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Analysis/LoopInfo.h"

#include "llvm/PassRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "FunctionSnippet.h"
#include "Utils.h"
#include "Analysis/FunctionAnaliser.h"
#include "Analysis/InputDependentFunctionAnalysisResult.h"
#include "Analysis/BasicBlocksUtils.h"
#include "Analysis/InputDepConfig.h"

#include <vector>
#include <memory>

namespace oh {

namespace {

llvm::Loop* get_outermost_loop(llvm::Loop* loop)
{
    auto* parent_loop = loop;
    while (parent_loop) {
        loop = parent_loop;
        parent_loop = loop->getParentLoop();
    }
    return loop;
}

class InstructionExtraction
{
public:
    using InputDependencyAnalysisInfo = input_dependency::InputDependencyAnalysisInterface::InputDepResType;
    using InstructionSet = std::unordered_set<llvm::Instruction*>;
    using ExtractionPredicate = std::function<bool (llvm::Instruction*)>;

public:
    InstructionExtraction(llvm::Module* module);

    void set_input_dep_info(InputDependencyAnalysisInfo input_dep_info)
    {
        m_input_dep_info = input_dep_info;
    }

    bool can_extract(llvm::Instruction* instr,
                     bool check_reachability,
                     bool check_operands,
                     bool no_data_indep_operand) const;

private:
    void collect_argument_reachable_instructions(llvm::Function* F);
    void collect_global_reachable_instructions(llvm::Function* F);
    bool has_extractable_operand(llvm::Instruction* instr,
                                 bool check_reachability) const;
    bool has_data_indep_operands(llvm::Instruction* instr) const;

private:
    llvm::Module* m_module;
    InputDependencyAnalysisInfo m_input_dep_info;
};

InstructionExtraction::InstructionExtraction(llvm::Module* module)
    : m_module(module)
{
}

bool InstructionExtraction::has_extractable_operand(llvm::Instruction* instr,
                                                    bool check_reachability) const
{
    for (auto op = instr->op_begin(); op != instr->op_end(); ++op) {
        auto* op_instr = llvm::dyn_cast<llvm::Instruction>(&*op);
        if (!op_instr) {
            continue;
        }
        if (llvm::dyn_cast<llvm::AllocaInst>(op_instr)) {
            continue;
        }
        if (can_extract(op_instr, check_reachability, false, false)) {
            return true;
        }
    }
    return false;
}

bool InstructionExtraction::has_data_indep_operands(llvm::Instruction* instr) const
{
    for (auto op = instr->op_begin(); op != instr->op_end(); ++op) {
        auto* op_instr = llvm::dyn_cast<llvm::Instruction>(&*op);
        if (!op_instr) {
            continue;
        }
        if (llvm::dyn_cast<llvm::AllocaInst>(op_instr)) {
            continue;
        }
        if (!m_input_dep_info->isDataDependent(op_instr)) {
            return true;
        }
    }
    return false;
}

bool InstructionExtraction::can_extract(llvm::Instruction* instr,
                                        bool check_reachability,
                                        bool check_operands,
                                        bool no_data_indep_operand) const
{
    if (llvm::dyn_cast<llvm::AllocaInst>(instr)) {
        return false;
    }
    if (llvm::dyn_cast<llvm::TerminatorInst>(instr)) {
        return false;
    }
    llvm::Function* F = instr->getParent()->getParent();
    if (m_input_dep_info->isDataDependent(instr)) {
        if (no_data_indep_operand) {
            return !has_data_indep_operands(instr);
        }
        return true;
    }
    if (m_input_dep_info->isDataDependent(instr)) {
        return true;
    }
    if (check_reachability) {
        if (m_input_dep_info->isGlobalDependent(instr)) {
            return true;
        }
        if (m_input_dep_info->isArgumentDependent(instr)) {
            return true;
        }
    }
    if (check_operands) {
        return has_extractable_operand(instr, check_reachability);
    }
    return false;
}

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
        , m_dont_extract_data_indeps(false)
        , m_is_whole_function_snippet(false)
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

    void set_loop_info(llvm::LoopInfo* loopInfo)
    {
        m_loop_info = loopInfo;
    }

    void set_instruction_extraction_predicate(InstructionExtraction* instr_extr)
    {
        m_extract_instruction = instr_extr;
    }

    void set_dont_extract_data_indeps(bool dont_extract)
    {
        m_dont_extract_data_indeps = dont_extract;
    }

    const snippet_list& get_snippets() const
    {
        return m_snippets;
    }

    bool is_whole_function_snippet() const
    {
        return m_is_whole_function_snippet;
    }

    int get_number_of_extracted_data_indep_instrs() const
    {
        return m_extracted_data_indep_instrs.size();
    }

public:
    void collect_snippets();
    void expand_snippets();
    void merge_snippets();

private:
    snippet_list create_instruction_snippets(llvm::BasicBlock* B);
    Snippet_type create_block_snippet_from_loop(llvm::Loop* loop);
    void update_processed_blocks(Snippet_type snippet,
                                 std::unordered_set<const llvm::BasicBlock*>& processed_blocks);
    void add_to_extraced_data_indep_instrs(const std::unordered_set<llvm::Instruction*>& expanded_instrs);

private:
    llvm::Function& m_F;
    bool m_is_whole_function_snippet;
    InputDependencyAnalysisInfo m_input_dep_info;
    llvm::PostDominatorTree* m_pdom;
    llvm::LoopInfo* m_loop_info;
    bool m_dont_extract_data_indeps;
    InstructionExtraction* m_extract_instruction;
    snippet_list m_snippets;
    std::unordered_set<llvm::Instruction*> m_extracted_data_indep_instrs;
};

void SnippetsCreator::collect_snippets()
{
    llvm::dbgs() << "Start collecting snippets\n";
    std::unordered_set<const llvm::BasicBlock*> processed_blocks;
    for (auto& B : m_F) {
        if (input_dependency::BasicBlocksUtils::get().isBlockUnreachable(&B)) {
            continue;
        }
        if (processed_blocks.find(&B) != processed_blocks.end()) {
            continue;
        }
        auto instr_snippets = create_instruction_snippets(&B);
        processed_blocks.insert(&B);
        m_snippets.insert(m_snippets.end(), instr_snippets.begin(), instr_snippets.end());
    }
    if (!m_dont_extract_data_indeps) {
        expand_snippets();
    }
    merge_snippets();
}

void SnippetsCreator::expand_snippets()
{
    for (auto& snippet : m_snippets) {
        if (!snippet) {
            continue;
        }
        const auto& expanded_instrs = snippet->expand();
        add_to_extraced_data_indep_instrs(expanded_instrs);
        snippet->adjust_end();
    }
}

void SnippetsCreator::merge_snippets()
{
    if (m_snippets.size() == 1) {
        if ((*m_snippets.begin())->is_single_instr_snippet()) {
            m_snippets.clear();
        }
    }

    std::vector<int> to_erase;
    for (unsigned i = 0; i < m_snippets.size(); ++i) {
        if (!m_snippets[i]) {
            continue;
        }
        unsigned next = i + 1;
        if (next == m_snippets.size()) {
            if (m_snippets[i]->is_single_instr_snippet()) {
                to_erase.push_back(i);
            }
            break;
        }
        if (m_snippets[i]->intersects(*m_snippets[next])) {
            if (m_snippets[next]->merge(*m_snippets[i])) {
                if (m_snippets[next]->is_function()) {
                    m_is_whole_function_snippet = true;
                    break;
                }
                to_erase.push_back(i);
            } else if (m_snippets[i]->merge(*m_snippets[next])) {
                if (m_snippets[i]->is_function()) {
                    m_is_whole_function_snippet = true;
                    break;
                }
                // swap
                auto tmp = m_snippets[next];
                m_snippets[next] = m_snippets[i];
                m_snippets[i] = tmp;
                to_erase.push_back(i);
            }
        }
    }
    if (m_is_whole_function_snippet) {
        m_snippets.clear();
        return;
    }
    for (const auto& idx : to_erase) {
        // don't erase as is expensive. Replace with nulls
        m_snippets[idx].reset();
    }
}

SnippetsCreator::snippet_list SnippetsCreator::create_instruction_snippets(llvm::BasicBlock* block)
{
    snippet_list snippets;
    InstructionsSnippet::iterator snippet_begin = block->end();
    InstructionsSnippet::iterator snippet_end = block->end();
    auto it = block->begin();
    bool check_reachability = m_input_dep_info->isInputDepFunction()
                                        || m_input_dep_info->isInputDependentBlock(block);
    while (it != block->end()) {
        llvm::Instruction* I = &*it;
        bool check_operands = !m_dont_extract_data_indeps && check_reachability;
        if (auto* callInst = llvm::dyn_cast<llvm::CallInst>(I)) {
            llvm::Function* called_f = callInst->getCalledFunction();
            check_operands &=  callInst->getFunctionType()->getReturnType()->isVoidTy()
                                && (!called_f || !called_f->isIntrinsic());
        }
        if (auto* invokeInst =  llvm::dyn_cast<llvm::InvokeInst>(I)) {
            llvm::Function* called_f = invokeInst->getCalledFunction();
            check_operands &=  invokeInst->getFunctionType()->getReturnType()->isVoidTy()
                                    && (!called_f || !called_f->isIntrinsic());
        }
        bool can_extract = m_extract_instruction->can_extract(I, check_reachability,
                                                              check_operands, m_dont_extract_data_indeps);
        if (can_extract) {
            if (auto store = llvm::dyn_cast<llvm::StoreInst>(I)) {
                // Skip the instruction storing argument to a local variable. This should happen anyway, no need to extract
                if (llvm::dyn_cast<llvm::Argument>(store->getValueOperand())) {
                    ++it;
                    continue;
                }
            }
            if (snippet_begin != block->end()) {
                snippet_end = it;
            } else {
                snippet_begin = it;
                snippet_end = it;
            }
        } else {
            if (InstructionsSnippet::is_valid_snippet(snippet_begin, snippet_end, block)) {
                snippets.push_back(Snippet_type(new InstructionsSnippet(block, snippet_begin, snippet_end)));
                snippet_begin = block->end();
                snippet_end = block->end();
            }
        }
        ++it;
    }
    if (InstructionsSnippet::is_valid_snippet(snippet_begin, snippet_end, block)) {
        snippets.push_back(Snippet_type(new InstructionsSnippet(block, snippet_begin, snippet_end)));
    }
    return snippets;
}

SnippetsCreator::Snippet_type SnippetsCreator::create_block_snippet_from_loop(llvm::Loop* loop)
{
    std::unordered_set<llvm::BasicBlock*> blocks;
    blocks.insert(loop->getBlocks().begin(), loop->getBlocks().end());

    llvm::BasicBlock* begin_block = loop->getHeader();
    auto begin = Utils::get_block_pos(begin_block);
    llvm::BasicBlock* exit_block = loop->getExitBlock();
    if (!exit_block) {
        llvm::SmallVector<llvm::BasicBlock*, 10> exit_blocks;
        loop->getExitBlocks(exit_blocks);
        const auto& header_node = (*m_pdom)[begin_block];
        for (auto& block : exit_blocks) {
            const auto& b_node = (*m_pdom)[block];
            if (m_pdom->dominates(b_node, header_node)) {
                exit_block = block;
                break;
            }
        }
        if (!exit_block) {
            // assuming that neares common dominator will dominate all other exit blocks
            auto it = exit_blocks.begin();
            while (!exit_block && it != exit_blocks.end()) {
                exit_block = m_pdom->findNearestCommonDominator(begin_block, *it);
                ++it;
            }
            blocks.insert(exit_blocks.begin(), exit_blocks.end());
        }
        // one of exit blocks does not have terminating instruction. Then just choose one which has
        if (!exit_block) {
            for (auto block : exit_blocks) {
                if (!llvm::dyn_cast<llvm::UnreachableInst>(block->getTerminator())) {
                    exit_block = block;
                    break;
                }
            }
        }
    }
    auto end = Utils::get_block_pos(exit_block);
    return Snippet_type(new BasicBlocksSnippet(&m_F, begin, end, InstructionsSnippet()));
}


void SnippetsCreator::update_processed_blocks(Snippet_type snippet,
                                              std::unordered_set<const llvm::BasicBlock*>& processed_blocks)
{
    auto* block_snippet = snippet->to_blockSnippet();
    if (!block_snippet) {
        return;
    }
    processed_blocks.insert(block_snippet->get_blocks().begin(), block_snippet->get_blocks().end());
}

void SnippetsCreator::add_to_extraced_data_indep_instrs(const std::unordered_set<llvm::Instruction*>& expanded_instrs)
{
    for (auto& instr : expanded_instrs) {
        if (!m_input_dep_info->isDataDependent(instr)) {
            m_extracted_data_indep_instrs.insert(instr);
        }
    }
}

void run_on_function(llvm::Function& F,
                     llvm::PostDominatorTree* PDom,
                     llvm::LoopInfo* loopInfo,
                     const SnippetsCreator::InputDependencyAnalysisInfo& input_dep_info,
                     InstructionExtraction* instr_extr_pred,
                     bool dont_extract_data_indeps,
                     std::unordered_map<llvm::Function*, unsigned>& extracted_functions,
                     int& numberOfExtractedDataIndepInstrs)
{
    // map from block to snippets?
    SnippetsCreator creator(F);
    creator.set_input_dep_info(input_dep_info);
    creator.set_post_dom_tree(PDom);
    creator.set_loop_info(loopInfo);
    creator.set_instruction_extraction_predicate(instr_extr_pred);
    creator.set_dont_extract_data_indeps(dont_extract_data_indeps);
    creator.collect_snippets();
    numberOfExtractedDataIndepInstrs += creator.get_number_of_extracted_data_indep_instrs();
    if (creator.is_whole_function_snippet()) {
        llvm::dbgs() << "Whole function " << F.getName() << " is input dependent\n";
        input_dependency::InputDepConfig::get().add_extracted_function(&F);
        return;
    }
    const auto& snippets = creator.get_snippets();

    //llvm::dbgs() << "number of snippets " << snippets.size() << "\n";
    for (auto& snippet : snippets) {
        if (!snippet) {
            continue;
        }
        if (snippet->is_single_instr_snippet()) {
            llvm::dbgs() << "Do not extract single instruction snippet\n";
            snippet->dump();
            continue;
        }
        // **** DEBUG
        //if (F.getName() == "") {
        //    llvm::dbgs() << "To Function " << F.getName() << "\n";
        //    snippet->dump();
        //    llvm::dbgs() << "\n";
        //}
        // **** DEBUG END
        auto extracted_function = snippet->to_function();
        if (!extracted_function) {
            continue;
        }
        input_dependency::InputDepConfig::get().add_extracted_function(extracted_function);
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

static llvm::cl::opt<bool> dont_extract_data_indep(
    "dont-extract-data-indeps",
    llvm::cl::desc("Do not extract data indep instrs"),
    llvm::cl::value_desc("boolean flag"));

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
    AU.addRequired<llvm::LoopInfoWrapperPass>();
    // FunctionExtractionPass does not preserve results of InputDependency Analysis.
    // While it adds extracted functions as input dependent functions, the CFG of old functions change, thus input
    // dependency results are invalidated.
    AU.addRequired<input_dependency::InputDependencyAnalysisPass>();
}

bool FunctionExtractionPass::runOnModule(llvm::Module& M)
{
    bool modified = false;
    auto input_dep = getAnalysis<input_dependency::InputDependencyAnalysisPass>().getInputDependencyAnalysis();
    if (dont_extract_data_indep) {
        llvm::dbgs() << "Will not extract any data indep instruction\n";
    }

    createStatistics(M, *input_dep);
    m_coverageStatistics->setSectionName("input_dep_coverage_before_extraction");
    m_coverageStatistics->reportInputDepCoverage();
    std::unordered_map<llvm::Function*, unsigned> extracted_functions;
    InstructionExtraction extract_instr_pred(&M);
    int numberOfExtractedDataIndepInstrs = 0;
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
        llvm::PostDominatorTree* PDom = &getAnalysis<llvm::PostDominatorTreeWrapperPass>(F).getPostDomTree();
        llvm::LoopInfo* loopInfo = &getAnalysis<llvm::LoopInfoWrapperPass>(F).getLoopInfo();
        extract_instr_pred.set_input_dep_info(f_input_dep_info);
        run_on_function(F, PDom, loopInfo, f_input_dep_info, &extract_instr_pred, dont_extract_data_indep,
                        extracted_functions, numberOfExtractedDataIndepInstrs);
        modified = true;
        llvm::dbgs() << "Done function extraction on function " << F.getName() << "\n";
    }

    llvm::dbgs() << "\nExtracted functions are \n";
    const std::string extracted = "extracted";
    auto* extracted_function_md_str = llvm::MDString::get(M.getContext(), extracted);
    llvm::MDNode* extracted_function_md = llvm::MDNode::get(M.getContext(), extracted_function_md_str);
    for (const auto& f : extracted_functions) {
        llvm::Function* extracted_f = f.first;
        m_extracted_functions.insert(extracted_f);
        extracted_f->setMetadata(extracted, extracted_function_md);
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
    llvm::dbgs() << "Number of extracted data independent instructions " << numberOfExtractedDataIndepInstrs << "\n";
    m_coverageStatistics->setSectionName("input_dep_coverage_after_extraction");
    m_coverageStatistics->invalidate_stats_data();
    m_coverageStatistics->reportInputDepCoverage();
    m_extractionStatistics->report();

    //Utils::check_module(M);
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

