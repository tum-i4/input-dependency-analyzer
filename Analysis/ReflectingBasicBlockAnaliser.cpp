#include "ReflectingBasicBlockAnaliser.h"

#include "VirtualCallSitesAnalysis.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/OperandTraits.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

namespace {

DependencyAnaliser::ValueDependencies mergeValueDependencies(const std::vector<DependencyAnaliser::ValueDependencies>& dependencies)
{
    DependencyAnaliser::ValueDependencies mergedDependencies;
    for (const auto& deps : dependencies) {
        for (const auto& valDeps : deps) {
            assert(!valDeps.second.isValueDep());
            auto res = mergedDependencies.insert(valDeps);
            if (res.second) {
                continue;
            }
            res.first->second.mergeDependencies(valDeps.second);
        }
    }
    return mergedDependencies;
}

DependencyAnaliser::ValueDependencies mergeSuccessorDependenciesWithInitialDependencies(
                                                    const DependencyAnaliser::ValueDependencies& initialDependencies,
                                                    const DependencyAnaliser::ValueDependencies& mergeInto)
{
    auto finalMerged = mergeInto;
    for (const auto& initialDep : initialDependencies) {
        finalMerged.insert(initialDep);
    }
    return finalMerged;
}

class value_dependence_graph
{
public:
    class node;
    using nodeT = std::shared_ptr<node>;
    using node_set = std::unordered_set<nodeT>;

    class node
    {
    public:
        node(llvm::Value* val)
            : value(val)
        {
        }

        llvm::Value* get_value() const
        {
            return value;
        }

        node_set& get_depends_on_values()
        {
            return depends_on_values;
        }

        node_set& get_dependent_values()
        {
            return dependent_values;
        }

        bool add_depends_on_value(nodeT dep_node)
        {
            return depends_on_values.insert(dep_node).second;
        }

        bool add_dependent_value(nodeT dep_node)
        {
            return dependent_values.insert(dep_node).second;
        }
        
        void add_depends_on_values(nodeT self, node_set& nodes)
        {
            for (auto& n : nodes) {
                if (n == self) {
                    continue;
                }
                if (n->depends_on(self)) {
                    add_depends_on_values(self, n->get_depends_on_values());
                    continue;
                }
                depends_on_values.insert(n);
            }
        }

        void remove_depends_on(nodeT dep_node)
        {
            depends_on_values.erase(dep_node);
        }

        void remove_dependent_value(nodeT dep_node)
        {
            dependent_values.erase(dep_node);
        }

        void clear_dependent_values()
        {
            dependent_values.clear();
        }

        bool is_leaf() const
        {
            return depends_on_values.empty();
        }

        // TODO: is building circular graph and then traversing and cuting circles more efficient?
        bool depends_on(nodeT n) const
        {
            if (depends_on_values.empty()) {
                return false;
            }
            if (depends_on_values.find(n) != depends_on_values.end()) {
                return true;
            }
            for (const auto& dep_on : depends_on_values) {
                if (dep_on->depends_on(n)) {
                    return true;
                }
            }
            return false;
            //return depends_on_values.find(n) != depends_on_values.end();
        }

        void dump()
        {
            llvm::dbgs() << *value;
        }

    private:
     llvm::Value* value;
     // values this node depends on
     node_set depends_on_values;
     node_set dependent_values;
    };

public:
    value_dependence_graph() = default;
    void build(DependencyAnaliser::ValueDependencies& valueDeps,
               DependencyAnaliser::ValueDependencies& initialDeps);

    void dump() const;

    node_set& get_leaves()
    {
        return m_leaves;
    }

private:
    void dump(nodeT n) const;

private:
    node_set m_leaves;
};

void value_dependence_graph::build(DependencyAnaliser::ValueDependencies& valueDeps,
                                   DependencyAnaliser::ValueDependencies& initialDeps)
{
    std::unordered_map<llvm::Value*, nodeT> nodes;
    std::list<llvm::Value*> processing_list;
    for (auto& val : valueDeps) {
        processing_list.push_back(val.first);
    }
    while (!processing_list.empty()) {
        auto process_val = processing_list.back();
        processing_list.pop_back();

        auto item = valueDeps.find(process_val);
        if (item == valueDeps.end()) {
            item = initialDeps.find(process_val);
            if (item == initialDeps.end()) {
                continue;
            }
            valueDeps[process_val] = item->second;
            item = valueDeps.find(process_val);
        }
        auto res = nodes.insert(std::make_pair(item->first, nodeT(new node(item->first))));
        nodeT item_node = res.first->second;
        if (!item->second.isValueDep()) {
            m_leaves.insert(item_node);
            continue;
        }
        auto& value_deps = item->second.getValueDependencies();
        std::vector<llvm::Value*> values_to_erase;
        for (auto& val : value_deps) {
            if (val == item->first) {
                values_to_erase.push_back(item->first);
                continue;
            }
            auto val_res = nodes.insert(std::make_pair(val, nodeT(new node(val))));
            auto dep_node = val_res.first->second;
            bool is_global = llvm::dyn_cast<llvm::GlobalVariable>(val);
            if (is_global && valueDeps.find(val) == valueDeps.end()) {
                continue;
            }
            if (dep_node->depends_on(item_node)) {
                values_to_erase.push_back(val);
                continue;
            } else if (item_node->add_depends_on_value(dep_node)) {
                dep_node->add_dependent_value(item_node);
            }
            // is not in value list modified or referenced in this block
            if (valueDeps.find(val) == valueDeps.end()) {
                processing_list.push_back(val);
            }
        }
        for (auto& val : values_to_erase) {
            value_deps.erase(val);
        }
        if (value_deps.empty() && item->second.getDependency() == DepInfo::VALUE_DEP) {
            item->second.setDependency(DepInfo::INPUT_INDEP);
        }
        if (item_node->is_leaf()) {
            m_leaves.insert(item_node);
        }
    }
}

void value_dependence_graph::dump() const
{
    for (const auto& leaf : m_leaves) {
        dump(leaf);
        llvm::dbgs() << "\n";
    }
}

void value_dependence_graph::dump(nodeT n) const
{
    n->dump();
    for (const auto& dep_n : n->get_dependent_values()) {
        dump(dep_n);
    }
}

} // unnamed namespace

ReflectingBasicBlockAnaliser::ReflectingBasicBlockAnaliser(
                        llvm::Function* F,
                        llvm::AAResults& AAR,
                        const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                        const Arguments& inputs,
                        const FunctionAnalysisGetter& Fgetter,
                        llvm::BasicBlock* BB)
                    : BasicBlockAnalysisResult(F, AAR, virtualCallsInfo, inputs, Fgetter, BB)
                    , m_isReflected(false)
{
}

void ReflectingBasicBlockAnaliser::reflect(const DependencyAnaliser::ValueDependencies& dependencies,
                                           const DepInfo& mandatory_deps)
{
    resolveValueDependencies(dependencies, mandatory_deps);
    for (auto& item : m_valueDependencies) {
        if (!item.second.isDefined()) {
            continue;
        }
        reflect(item.first, item.second);
    }
    // TODO: would not need this part remove if all instructions are collected together in one map
    for (auto& instrDep : m_instructionValueDependencies) {
        assert(instrDep.second.isValueDep());
        m_inputDependentInstrs[instrDep.first].mergeDependencies(instrDep.second);
    }
    m_instructionValueDependencies.clear();
    m_valueDependentInstrs.clear();

    assert(m_valueDependentInstrs.empty());
    assert(m_valueDependentOutArguments.empty());
    assert(m_valueDependentFunctionCallArguments.empty());
    assert(m_valueDependentFunctionInvokeArguments.empty());
    m_isReflected = true;
}

DepInfo ReflectingBasicBlockAnaliser::getInstructionDependencies(llvm::Instruction* instr) const
{
    auto indeppos = m_inputIndependentInstrs.find(instr);
    if (indeppos != m_inputIndependentInstrs.end()) {
        return DepInfo(DepInfo::INPUT_INDEP);
    }
    auto valpos = m_instructionValueDependencies.find(instr);
    if (valpos != m_instructionValueDependencies.end()) {
        return valpos->second;
    }
    auto deppos = m_inputDependentInstrs.find(instr);
    assert(deppos != m_inputDependentInstrs.end());
    assert(deppos->second.isInputDep() || deppos->second.isInputArgumentDep());
    return deppos->second;
}

void ReflectingBasicBlockAnaliser::markAllInputDependent()
{
    BasicBlockAnalysisResult::markAllInputDependent();
    DepInfo info(DepInfo::INPUT_DEP);
    m_valueDependentInstrs.clear();
    for (auto& instrItem : m_instructionValueDependencies) {
        m_inputDependentInstrs.insert(std::make_pair(instrItem.first, info));
    }
    m_instructionValueDependencies.clear();
    for (auto& depItem : m_valueDependentOutArguments) {
        for (auto& arg : depItem.second) {
            m_outArgDependencies[arg] = info;
        }
    }
    m_valueDependentFunctionCallArguments.clear();
    m_valueDependentFunctionInvokeArguments.clear();
    m_valueDependentCallGlobals.clear();
    m_valueDependentInvokeGlobals.clear();
}

void ReflectingBasicBlockAnaliser::processInstrForOutputArgs(llvm::Instruction* I)
{
    if (m_outArgDependencies.empty()) {
        return;
    }
    const auto& DL = I->getModule()->getDataLayout();
    auto item = m_outArgDependencies.begin();
    while (item != m_outArgDependencies.end()) {
        llvm::Value* val = llvm::dyn_cast<llvm::Value>(item->first);
        const auto& info = m_AAR.getModRefInfo(I, val, DL.getTypeStoreSize(val->getType()));
        if (info != llvm::ModRefInfo::MRI_Mod) {
            ++item;
            continue;
        }
        auto valueDepPos = m_instructionValueDependencies.find(I);
        if (valueDepPos != m_instructionValueDependencies.end()) {
            assert(valueDepPos->second.isValueDep());
            const auto& dependencies = valueDepPos->second;
            item->second.setDependency(valueDepPos->second.getDependency());
            item->second.mergeDependencies(valueDepPos->second.getValueDependencies());
            for (const auto& val : item->second.getValueDependencies()) {
                m_valueDependentOutArguments[val].insert(item->first);
            }
            ++item;
            continue;
        }
        auto depInstrPos = m_inputDependentInstrs.find(I);
        if (depInstrPos != m_inputDependentInstrs.end()) {
            const auto& dependencies = depInstrPos->second;
            item->second.mergeDependencies(depInstrPos->second);
        } else {
            // making output input independent
            item->second = DepInfo(DepInfo::INPUT_INDEP);
        }
        ++item;
    }
}

DepInfo ReflectingBasicBlockAnaliser::getInstructionDependencies(llvm::Instruction* instr)
{
    auto deppos = m_inputDependentInstrs.find(instr);
    if (deppos != m_inputDependentInstrs.end()) {
        return deppos->second;
    }
    auto indeppos = m_inputIndependentInstrs.find(instr);
    if (indeppos != m_inputIndependentInstrs.end()) {
        return DepInfo(DepInfo::INPUT_INDEP);
    }
    auto valdeppos = m_instructionValueDependencies.find(instr);
    if (valdeppos != m_instructionValueDependencies.end()) {
        return valdeppos->second;
    }
    if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(instr)) {
        auto deps = getValueDependencies(allocaInst);
        DepInfo info;
        info.mergeDependencies(deps);
        return info;
    }
    if (auto* loadInst = llvm::dyn_cast<llvm::LoadInst>(instr)) {
        return getLoadInstrDependencies(loadInst);
    }

    return determineInstructionDependenciesFromOperands(instr);

}

void ReflectingBasicBlockAnaliser::updateInstructionDependencies(llvm::Instruction* instr,
                                                                 const DepInfo& info)
{
    assert(info.isDefined());
    if (info.isValueDep()) {
        m_instructionValueDependencies[instr] = info;
        updateValueDependentInstructions(info, instr);
    } else if (info.isInputIndep()) {
        assert(info.getArgumentDependencies().empty());
        assert(info.getValueDependencies().empty());
        m_inputIndependentInstrs.insert(instr);
    } else {
        assert(info.isInputDep() || info.isInputArgumentDep());
        m_inputDependentInstrs[instr] = info;
    }
}

// basic block analysis result could do this same way
void ReflectingBasicBlockAnaliser::updateReturnValueDependencies(const DepInfo& info)
{
    m_returnValueDependencies.mergeDependencies(info);
}

DepInfo ReflectingBasicBlockAnaliser::getLoadInstrDependencies(llvm::LoadInst* instr)
{
    auto* loadOp = instr->getPointerOperand();
    llvm::Value* loadedValue = getMemoryValue(loadOp);

    DepInfo info = BasicBlockAnalysisResult::getLoadInstrDependencies(instr);
    if (auto loadedInst = llvm::dyn_cast<llvm::Instruction>(loadedValue)) {
        auto alloca = llvm::dyn_cast<llvm::AllocaInst>(loadedInst);
        if (!alloca) {
            // or?
            info.mergeDependencies(getInstructionDependencies(loadedInst));
            return info;
        }
    }
    info.mergeDependencies(ValueSet{loadedValue});
    info.mergeDependency(DepInfo::VALUE_DEP);
    return info;
}

void ReflectingBasicBlockAnaliser::updateFunctionCallSiteInfo(llvm::CallInst* callInst, llvm::Function* F)
{
    BasicBlockAnalysisResult::updateFunctionCallSiteInfo(callInst, F);
    updateValueDependentCallArguments(callInst, F);
    updateValueDependentCallReferencedGlobals(callInst, F);
}

void ReflectingBasicBlockAnaliser::updateFunctionInvokeSiteInfo(llvm::InvokeInst* invokeInst, llvm::Function* F)
{
    BasicBlockAnalysisResult::updateFunctionInvokeSiteInfo(invokeInst, F);
    updateValueDependentInvokeArguments(invokeInst, F);
    updateValueDependentInvokeReferencedGlobals(invokeInst, F);
}

void ReflectingBasicBlockAnaliser::updateValueDependentInstructions(const DepInfo& info,
                                                                    llvm::Instruction* instr)
{
    for (const auto& val : info.getValueDependencies()) {
        m_valueDependentInstrs[val].insert(instr);
    }
}

void ReflectingBasicBlockAnaliser::updateValueDependentCallArguments(llvm::CallInst* callInst, llvm::Function* F)
{
    assert(F != nullptr);
    auto pos = m_functionCallInfo.find(F);
    if (pos == m_functionCallInfo.end()) {
        // is this possible?
        return;
    }

    const auto& dependencies = pos->second.getArgumentDependenciesForCall(callInst);
    for (const auto& dep : dependencies) {
        if (!dep.second.isValueDep()) {
            continue;
        }
        for (const auto& val : dep.second.getValueDependencies()) {
            m_valueDependentFunctionCallArguments[val][callInst].insert(dep.first);
        }
    }
}

void ReflectingBasicBlockAnaliser::updateValueDependentInvokeArguments(llvm::InvokeInst* invokeInst, llvm::Function* F)
{
    assert(F != nullptr);
    auto pos = m_functionCallInfo.find(F);
    assert(pos != m_functionCallInfo.end());
    const auto& dependencies = pos->second.getArgumentDependenciesForInvoke(invokeInst);
    for (const auto& dep : dependencies) {
        if (!dep.second.isValueDep()) {
            continue;
        }
        for (const auto& val : dep.second.getValueDependencies()) {
            m_valueDependentFunctionInvokeArguments[val][invokeInst].insert(dep.first);
        }
    }
}

void ReflectingBasicBlockAnaliser::updateValueDependentCallReferencedGlobals(llvm::CallInst* callInst, llvm::Function* F)
{
    assert(F != nullptr);
    auto pos = m_functionCallInfo.find(F);
    assert(pos != m_functionCallInfo.end());
    const auto& dependencies = pos->second.getGlobalsDependenciesForCall(callInst);
    for (const auto& dep : dependencies) {
        if (!dep.second.isValueDep()) {
            continue;
        }
        for (const auto& val : dep.second.getValueDependencies()) {
            m_valueDependentCallGlobals[val][callInst].insert(dep.first);
        }
    }
}

void ReflectingBasicBlockAnaliser::updateValueDependentInvokeReferencedGlobals(llvm::InvokeInst* invokeInst, llvm::Function* F)
{
    assert(F != nullptr);
    auto pos = m_functionCallInfo.find(F);
    assert(pos != m_functionCallInfo.end());
    const auto& dependencies = pos->second.getGlobalsDependenciesForInvoke(invokeInst);
    for (const auto& dep : dependencies) {
        if (!dep.second.isValueDep()) {
            continue;
        }
        for (const auto& val : dep.second.getValueDependencies()) {
            m_valueDependentInvokeGlobals[val][invokeInst].insert(dep.first);
        }
    }
}

void ReflectingBasicBlockAnaliser::reflect(llvm::Value* value, const DepInfo& deps)
{
    assert(deps.isDefined());
    if (deps.isValueDep()) {
        assert(deps.isOnlyGlobalValueDependent());
    }
    reflectOnInstructions(value, deps); // need to go trough instructions one more time and add to correspoinding set
    reflectOnOutArguments(value, deps);
    reflectOnCalledFunctionArguments(value, deps);
    reflectOnCalledFunctionReferencedGlobals(value, deps);
    reflectOnInvokedFunctionArguments(value, deps);
    reflectOnInvokedFunctionReferencedGlobals(value, deps);
    reflectOnReturnValue(value, deps);
}

void ReflectingBasicBlockAnaliser::reflectOnInstructions(llvm::Value* value, const DepInfo& depInfo)
{
    auto instrDepPos = m_valueDependentInstrs.find(value);
    if (instrDepPos == m_valueDependentInstrs.end()) {
        return;
    }
    for (const auto& instr : instrDepPos->second) {
        auto instrPos = m_instructionValueDependencies.find(instr);
        assert(instrPos != m_instructionValueDependencies.end());
        reflectOnDepInfo(value, instrPos->second, depInfo);
        if (instrPos->second.isValueDep()) {
            continue;
        }
        if (instrPos->second.isInputDep() || instrPos->second.isInputArgumentDep()) {
            m_inputDependentInstrs[instr].mergeDependencies(instrPos->second.getArgumentDependencies());
            m_inputDependentInstrs[instr].mergeDependency(instrPos->second.getDependency());
            assert(!m_inputDependentInstrs[instr].isValueDep());
        } else if (instrPos->second.isInputIndep()) {
            m_inputIndependentInstrs.insert(instr);
        }
        m_instructionValueDependencies.erase(instrPos);
    }
    m_valueDependentInstrs.erase(instrDepPos);
}

void ReflectingBasicBlockAnaliser::reflectOnOutArguments(llvm::Value* value, const DepInfo& depInfo)
{
    auto outArgPos = m_valueDependentOutArguments.find(value);
    if (outArgPos == m_valueDependentOutArguments.end()) {
        return;
    }
    for (const auto& outArg : outArgPos->second) {
        auto argPos = m_outArgDependencies.find(outArg);
        assert(argPos != m_outArgDependencies.end());
        reflectOnDepInfo(value, argPos->second, depInfo);
    }
    m_valueDependentOutArguments.erase(outArgPos);
}

void ReflectingBasicBlockAnaliser::reflectOnCalledFunctionArguments(llvm::Value* value, const DepInfo& depInfo)
{
    auto valPos = m_valueDependentFunctionCallArguments.find(value);
    if (valPos == m_valueDependentFunctionCallArguments.end()) {
        return;
    }

    for (const auto& fargs : valPos->second) {
        auto callInst = fargs.first;
        auto F = callInst->getCalledFunction();
        auto Fpos = m_functionCallInfo.find(F);
        assert(Fpos != m_functionCallInfo.end());
        auto& callDeps = Fpos->second.getArgumentDependenciesForCall(callInst);
        for (auto& arg : fargs.second) {
            auto argPos = callDeps.find(arg);
            assert(argPos != callDeps.end());
            reflectOnDepInfo(value, argPos->second, depInfo);
            // TODO: need to delete if becomes input indep?
        }
    }
    m_valueDependentFunctionCallArguments.erase(valPos);
}

void ReflectingBasicBlockAnaliser::reflectOnCalledFunctionReferencedGlobals(llvm::Value* value, const DepInfo& depInfo)
{
    auto valPos = m_valueDependentCallGlobals.find(value);
    if (valPos == m_valueDependentCallGlobals.end()) {
        return;
    }

    for (const auto& fargs : valPos->second) {
        auto callInst = fargs.first;
        auto F = callInst->getCalledFunction();
        auto Fpos = m_functionCallInfo.find(F);
        assert(Fpos != m_functionCallInfo.end());
        auto& callDeps = Fpos->second.getGlobalsDependenciesForCall(callInst);
        for (auto& arg : fargs.second) {
            auto argPos = callDeps.find(arg);
            assert(argPos != callDeps.end());
            reflectOnDepInfo(value, argPos->second, depInfo);
        }
    }
    m_valueDependentCallGlobals.erase(valPos);
}

void ReflectingBasicBlockAnaliser::reflectOnInvokedFunctionArguments(llvm::Value* value, const DepInfo& depInfo)
{
    auto valPos = m_valueDependentFunctionInvokeArguments.find(value);
    if (valPos == m_valueDependentFunctionInvokeArguments.end()) {
        return;
    }

    for (const auto& fargs : valPos->second) {
        auto invokeInst = fargs.first;
        auto F = invokeInst->getCalledFunction();
        auto Fpos = m_functionCallInfo.find(F);
        assert(Fpos != m_functionCallInfo.end());
        auto& invokeDeps = Fpos->second.getArgumentDependenciesForInvoke(invokeInst);
        for (auto& arg : fargs.second) {
            auto argPos = invokeDeps.find(arg);
            assert(argPos != invokeDeps.end());
            reflectOnDepInfo(value, argPos->second, depInfo);
            // TODO: need to delete if becomes input indep?
        }
    }
    m_valueDependentFunctionInvokeArguments.erase(valPos);
}

void ReflectingBasicBlockAnaliser::reflectOnInvokedFunctionReferencedGlobals(llvm::Value* value, const DepInfo& depInfo)
{
    auto valPos = m_valueDependentInvokeGlobals.find(value);
    if (valPos == m_valueDependentInvokeGlobals.end()) {
        return;
    }

    for (const auto& fargs : valPos->second) {
        auto invokeInst = fargs.first;
        auto F = invokeInst->getCalledFunction();
        auto Fpos = m_functionCallInfo.find(F);
        assert(Fpos != m_functionCallInfo.end());
        auto& invokeDeps = Fpos->second.getGlobalsDependenciesForInvoke(invokeInst);
        for (auto& arg : fargs.second) {
            auto argPos = invokeDeps.find(arg);
            assert(argPos != invokeDeps.end());
            reflectOnDepInfo(value, argPos->second, depInfo);
        }
    }
    m_valueDependentInvokeGlobals.erase(valPos);
}

void ReflectingBasicBlockAnaliser::reflectOnReturnValue(llvm::Value* value, const DepInfo& depInfo)
{
    if (!m_returnValueDependencies.isValueDep()) {
        return;
    }
    auto pos = m_returnValueDependencies.getValueDependencies().find(value);
    if (pos == m_returnValueDependencies.getValueDependencies().end()) {
        return;
    }
    reflectOnDepInfo(value, m_returnValueDependencies, depInfo);
}

void ReflectingBasicBlockAnaliser::reflectOnDepInfo(llvm::Value* value,
                                                    DepInfo& depInfoTo,
                                                    const DepInfo& depInfoFrom,
                                                    bool eraseAfterReflection)
{
    // note: this won't change pos dependency, if it is of maximum value input_dep
    assert(depInfoTo.isValueDep());
    if (depInfoTo.getDependency() == DepInfo::VALUE_DEP) {
        depInfoTo.setDependency(depInfoFrom.getDependency());
    }
    depInfoTo.mergeDependencies(depInfoFrom);
    if (!eraseAfterReflection) {
        return;
    }
    auto& valueDeps = depInfoTo.getValueDependencies();
    auto valPos = valueDeps.find(value);
    assert(valPos != valueDeps.end());
    valueDeps.erase(valPos);
}

void ReflectingBasicBlockAnaliser::resolveValueDependencies(const DependencyAnaliser::ValueDependencies& successorDependencies,
                                                            const DepInfo& mandatory_deps)
{
    for (const auto& dep : successorDependencies) {
        auto res = m_valueDependencies.insert(dep);
        if (!res.second) {
            res.first->second.mergeDependencies(dep.second);
        }
    }
    for (auto& val_dep : m_valueDependencies) {
        val_dep.second.mergeDependencies(mandatory_deps);
    }
    value_dependence_graph graph;
    graph.build(m_valueDependencies, m_initialDependencies);
    //graph.dump();
    auto& graph_leaves = graph.get_leaves();
    std::list<value_dependence_graph::nodeT> leaves(graph_leaves.begin(), graph_leaves.end());
    while (!leaves.empty()) {
        auto leaf = leaves.back();
        auto val_pos = m_valueDependencies.find(leaf->get_value());
        assert(val_pos != m_valueDependencies.end());
        assert(!val_pos->second.isValueDep() || val_pos->second.isOnlyGlobalValueDependent());
        assert(!val_pos->second.isValueDep());
        for (auto& dep_node : leaf->get_dependent_values()) {
            auto dep_val = dep_node->get_value();
            auto dep_val_pos = m_valueDependencies.find(dep_val);
            assert(dep_val_pos != m_valueDependencies.end());
            //assert(!dep_val_pos->second.isValueDep() || dep_val_pos->second.isOnlyGlobalValueDependent());
            dep_val_pos->second.mergeDependencies(val_pos->second);
            dep_val_pos->second.getValueDependencies().erase(val_pos->first);
            if (dep_val_pos->second.getDependency() == DepInfo::VALUE_DEP && dep_val_pos->second.getValueDependencies().empty()) {
                dep_val_pos->second.setDependency(val_pos->second.getDependency());
            } else {
                dep_val_pos->second.mergeDependency(val_pos->second.getDependency());
            }
            dep_node->remove_depends_on(leaf);
            //leaf->remove_dependent_value(dep_node);
            if (dep_node->is_leaf()) {
                // to safely remove from back the leaf
                leaves.push_front(dep_node);
            }
        }
        leaf->clear_dependent_values();
        if (leaf->get_dependent_values().empty() || val_pos->second.isOnlyGlobalValueDependent()) {
            leaves.pop_back();
        }
    }
    for (auto& item : m_valueDependencies) {
        assert(!item.second.isValueDep() || item.second.isOnlyGlobalValueDependent());
    }
}

DepInfo ReflectingBasicBlockAnaliser::getValueFinalDependencies(llvm::Value* value, ValueSet& processed)
{
    auto pos = m_valueDependencies.find(value);
    if (pos == m_valueDependencies.end()) {
        assert(llvm::dyn_cast<llvm::GlobalVariable>(value));
        processed.insert(value);
        return DepInfo(DepInfo::VALUE_DEP, ValueSet{value});
    }
    assert(pos != m_valueDependencies.end());
    if (pos->second.getValueDependencies().empty()) {
        processed.insert(value);
        return DepInfo(pos->second.getDependency(), ValueSet{value});
    }
    DepInfo depInfo(pos->second.getDependency());
    for (auto val : pos->second.getValueDependencies()) {
        if (val == value) {
            // ???
            processed.insert(value);
            depInfo.mergeDependencies(ValueSet{value});
            continue;
        }
        if (processed.find(val) != processed.end()) {
            continue;
        }
        processed.insert(value);
        const auto& deps = getValueFinalDependencies(val, processed);
        depInfo.mergeDependencies(deps);
    }
    return depInfo;
}


} // namespace input_dependency

