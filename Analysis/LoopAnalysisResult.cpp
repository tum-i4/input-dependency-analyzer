#include "LoopAnalysisResult.h"

#include "ReflectingBasicBlockAnaliser.h"
#include "NonDeterministicReflectingBasicBlockAnaliser.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
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

LoopAnalysisResult::LoopAnalysisResult(llvm::Function* F,
                                       llvm::AAResults& AAR,
                                       const Arguments& inputs,
                                       const FunctionAnalysisGetter& Fgetter,
                                       llvm::Loop& LI)
                                : m_F(F)
                                , m_AAR(AAR)
                                , m_inputs(inputs)
                                , m_FAG(Fgetter)
                                , m_LI(LI)
{
}

void LoopAnalysisResult::gatherResults()
{
    const auto& blocks = m_LI.getBlocks();
    for (const auto& B : blocks) {
        m_BBAnalisers[B] = createReflectingBasicBlockAnaliser(B);
        m_BBAnalisers[B]->setInitialValueDependencies(getBasicBlockPredecessorsDependencies(B));
        m_BBAnalisers[B]->setOutArguments(getBasicBlockPredecessorsArguments(B));
        m_BBAnalisers[B]->gatherResults();
        //m_BBAnalisers[B]->dumpResults();
    }
    reflect();
    updateFunctionCallInfo();
    updateReturnValueDependencies();
    updateOutArgumentDependencies();
    updateValueDependencies();
}

void LoopAnalysisResult::finalizeResults(const DependencyAnaliser::ArgumentDependenciesMap& dependentArgs)
{
    for (auto& item : m_BBAnalisers) {
        item.second->finalizeResults(dependentArgs);
    }
}

void LoopAnalysisResult::dumpResults() const
{
    for (const auto& item : m_BBAnalisers) {
        item.second->dumpResults();
    }
}

void LoopAnalysisResult::setInitialValueDependencies(
            const DependencyAnalysisResult::InitialValueDpendencies& valueDependencies)
{
    // In practice number of predecessors will be at most 2
    //llvm::dbgs() << "setting initial value dependencies for BB " << m_BB->getName() << "\n";
    for (const auto& item : valueDependencies) {
        //llvm::dbgs() << "Value " << *item.first << "\n";
        auto& valDep = m_valueDependencies[item.first];
        for (const auto& dep : item.second) {
            //llvm::dbgs() << " in predecessor " << dep.dependency << "\n";
            if (valDep.getDependency() <= dep.getDependency()) {
                valDep.setDependency(dep.getDependency());
                valDep.getArgumentDependencies().insert(dep.getArgumentDependencies().begin(),
                                                        dep.getArgumentDependencies().end());
            }
        }
    }
}

void LoopAnalysisResult::setOutArguments(const DependencyAnalysisResult::InitialArgumentDependencies& outArgs)
{
    for (const auto& arg : outArgs) {
        //llvm::dbgs() << "out arg " << *arg.first << "\n";
        auto& argDep = m_outArgDependencies[arg.first];
        for (const auto& dep : arg.second) {
            if (argDep.getDependency() <= dep.getDependency()) {
                argDep.setDependency(dep.getDependency());
                argDep.mergeDependencies(dep.getArgumentDependencies());
            }
        }
    }
}

bool LoopAnalysisResult::isInputDependent(llvm::Instruction* instr) const
{
    auto parentBB = instr->getParent();
    auto pos = m_BBAnalisers.find(parentBB);
    assert(pos != m_BBAnalisers.end());
    return pos->second->isInputDependent(instr);
}

const ArgumentSet& LoopAnalysisResult::getValueInputDependencies(llvm::Value* val) const
{
    auto pos = m_valueDependencies.find(val);
    assert(pos != m_valueDependencies.end());
    return pos->second.getArgumentDependencies();
}

DepInfo LoopAnalysisResult::getInstructionDependencies(llvm::Instruction* instr) const
{
    auto parentBB = instr->getParent();
    auto pos = m_BBAnalisers.find(parentBB);
    assert(pos != m_BBAnalisers.end());
    return pos->second->getInstructionDependencies(instr);
}

const DependencyAnaliser::ValueDependencies& LoopAnalysisResult::getValuesDependencies() const
{
    return m_valueDependencies;
}

//const ValueSet& LoopAnalysisResult::getValueDependencies(llvm::Value* val)
//{
//    return ValueSet();
//}

const DepInfo& LoopAnalysisResult::getReturnValueDependencies() const
{
    return m_returnValueDependencies;
}

const DependencyAnaliser::ArgumentDependenciesMap&
LoopAnalysisResult::getOutParamsDependencies() const
{
    return m_outArgDependencies;
}

const DependencyAnaliser::FunctionArgumentsDependencies&
LoopAnalysisResult::getFunctionsCallInfo() const
{
    return m_calledFunctionsInfo;
}

LoopAnalysisResult::PredValDeps LoopAnalysisResult::getBasicBlockPredecessorsDependencies(llvm::BasicBlock* B)
{
    PredValDeps deps;

    if (m_LI.getHeader() == B) {
        return deps;
    }
    auto pred = pred_begin(B);
    unsigned predCount = 0;
    while (pred != pred_end(B)) {
        ++predCount;
        auto pos = m_BBAnalisers.find(*pred);
        if (pos == m_BBAnalisers.end()) {
            ++pred;
            continue;
        }
        assert(pos != m_BBAnalisers.end());
        const auto& valueDeps = pos->second->getValuesDependencies();
        for (auto& dep : valueDeps) {
            //llvm::dbgs() << "Add to " << B->getName() << " initial dependencies " << *dep.first << "   " << dep.second.dependency << "\n"; 
            deps[dep.first].push_back(dep.second);
        }
        ++pred;
    }
    for (auto& valDeps : deps) {
        if (valDeps.second.size() == predCount) {
            continue;
        }
        auto pos = m_valueDependencies.find(valDeps.first);
        assert(pos != m_valueDependencies.end());
        if (!pos->second.isDefined()) {
            continue;
        }
        //llvm::dbgs() << "Not complete deps for value " << *pos->first << " for block " << B->getName() << "\n";
        //llvm::dbgs() << "Completing\n";
        valDeps.second.push_back(DepInfo(DepInfo::VALUE_DEP, ValueSet{pos->first}));
    }
    return deps;
}

LoopAnalysisResult::PredArgDeps LoopAnalysisResult::getBasicBlockPredecessorsArguments(llvm::BasicBlock* B)
{
    PredArgDeps deps;
    auto pred = pred_begin(B);
    while (pred != pred_end(B)) {
        if (!m_LI.contains(*pred)) {
            for (const auto& item : m_outArgDependencies) {
                deps[item.first].push_back(item.second);
            }
            ++pred;
            continue;
        }
        auto pos = m_BBAnalisers.find(*pred);
        if (pos == m_BBAnalisers.end()) {
            ++pred;
            continue;
        }
        assert(pos != m_BBAnalisers.end());
        const auto& argDeps = pos->second->getOutParamsDependencies();
        for (const auto& dep : argDeps) {
            deps[dep.first].push_back(dep.second);
        }
        ++pred;
    }
    return deps;
}

void LoopAnalysisResult::updateFunctionCallInfo()
{
    for (const auto& BA : m_BBAnalisers) {
        const auto& callInfo = BA.second->getFunctionsCallInfo();
        for (const auto& item : callInfo) {
            auto res = m_calledFunctionsInfo.insert(item);
            if (res.second) {
                continue;
            }
            auto& depMap = res.first->second;
            for (const auto& depItem : item.second) {
                depMap[depItem.first].mergeDependencies(depItem.second);
            }
        }
    }
}

void LoopAnalysisResult::updateReturnValueDependencies()
{
    for (const auto& BA : m_BBAnalisers) {
        const auto& retValDeps = BA.second->getReturnValueDependencies();
        m_returnValueDependencies.mergeDependencies(retValDeps);
    }
}

void LoopAnalysisResult::updateOutArgumentDependencies()
{
    // Out args are the same for all blocks,
    // after reflection all blocks will contains same information for out args.
    // So just pick one of blocks (say header) and get dep info from it
    auto BB = m_LI.getHeader();
    const auto& outArgs = m_BBAnalisers[BB]->getOutParamsDependencies();
    for (const auto& item : outArgs) {
        auto pos = m_outArgDependencies.find(item.first);
        assert(pos != m_outArgDependencies.end());
        pos->second = item.second;
    }
}

void LoopAnalysisResult::updateValueDependencies()
{
    auto BB = m_LI.getHeader();
    const auto& valuesDeps = m_BBAnalisers[BB]->getValuesDependencies();
    for (auto& item : m_valueDependencies) {
        auto pos = valuesDeps.find(item.first);
        if (pos != valuesDeps.end()) {
            item.second.mergeDependencies(pos->second);
        }
    }
}

void LoopAnalysisResult::reflect()
{
    // Go through bb-s starting from the bottom
    // For each get it's successor value dependencies, reflect on it
    Latches latches;
    m_LI.getLoopLatches(latches);

    const auto& blocks = m_LI.getBlocks();
    auto rit = blocks.rbegin();
    while (rit != blocks.rend()) {
        auto B = *rit;
        const auto& succDeps = getBlockSuccessorDependencies(B, latches);
        if (succDeps.empty()) {
            llvm::dbgs() << "Reflecting on " << B->getName() << " with only initial dependencies\n";
            m_BBAnalisers[B]->reflect(m_valueDependencies);
        } else {
            llvm::dbgs() << "Reflecting on " << B->getName() << " with only successors and initial dependencies\n";
            m_BBAnalisers[B]->reflect(succDeps, m_valueDependencies);
        }
        ++rit;
    }
}

LoopAnalysisResult::SuccessorDeps LoopAnalysisResult::getBlockSuccessorDependencies(llvm::BasicBlock* B,
                                                                                    const LoopAnalysisResult::Latches& latches) const
{
    SuccessorDeps deps;
    bool isLatch = (std::find(latches.begin(), latches.end(), B) != latches.end());
    if (isLatch) {
        return deps;
    }
    auto succ = succ_begin(B);
    while (succ != succ_end(B)) {
        auto succPos = m_BBAnalisers.find(*succ);
        if (succPos == m_BBAnalisers.end()) {
            assert(!m_LI.contains(*succ));
            ++succ;
            continue;
        }
        assert(succPos != m_BBAnalisers.end());
        if (!succPos->second->isReflected()) {
            llvm::dbgs() << "Successor of " << B->getName() << " " << succ->getName() << " not reflected yet. skipping\n";
            ++succ;
            continue;
        }
        deps.push_back(succPos->second->getValuesDependencies());
        ++succ;
    }
    return deps;
}

LoopAnalysisResult::ReflectingDependencyAnaliserT LoopAnalysisResult::createReflectingBasicBlockAnaliser(llvm::BasicBlock* B)
{
    auto depInfo = getBasicBlockPredecessorInstructionsDeps(B);
    if (depInfo.isInputIndep()) {
        return ReflectingDependencyAnaliserT(new ReflectingBasicBlockAnaliser(m_F, m_AAR, m_inputs, m_FAG, B));
    }
    return ReflectingDependencyAnaliserT(new NonDeterministicReflectingBasicBlockAnaliser(m_F, m_AAR, m_inputs, m_FAG, B, depInfo));
}

DepInfo LoopAnalysisResult::getBasicBlockPredecessorInstructionsDeps(llvm::BasicBlock* B) const
{
    DepInfo dep(DepInfo::INPUT_INDEP);
    //llvm::dbgs() << "LOOP isNonDet BB " << B->getName();
    auto pred = pred_begin(B);
    while (pred != pred_end(B)) {
        auto pb = *pred;
        const auto& termInstr = pb->getTerminator();
        //llvm::dbgs() << "Predecessor " << pb->getName() << " with terminating instruction ";
        if (termInstr == nullptr) {
            dep.setDependency(DepInfo::INPUT_DEP);
            break;
        }

        auto pos = m_BBAnalisers.find(pb);
        if (pos == m_BBAnalisers.end()) {
            ++pred;
            continue;
        }
        const auto& instrDeps = pos->second->getInstructionDependencies(termInstr);
        dep.mergeDependencies(instrDeps);
        //DEBUG code
//        llvm::dbgs() << " dependency is " << instrDeps.dependency << "\n";
//        for (const auto& val : instrDeps.valueDependencies) {
//            llvm::dbgs() << "dependent values are: \n";
//            llvm::dbgs() << *val << "\n";
//        }
//        for (const auto& arg : instrDeps.argumentDependencies) {
//            llvm::dbgs() << "dependent arguments are: \n";
//            llvm::dbgs() << *arg << "\n";
//        }
        ++pred;
    }
    //if (dep.dependency == DependencyAnaliser::DepInfo::INPUT_INDEP) {
    //    llvm::dbgs() << "\nIs deterministic\n";
    //} else {
    //    llvm::dbgs() << "\nIs Non deterministic\n";
    //}
    return dep;
}


} // namespace input_dependency

