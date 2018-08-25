#include "input-dependency/Analysis/IndirectCallSitesAnalysis.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/MapVector.h"

#include "llvm/Transforms/IPO/WholeProgramDevirt.h"
#include "llvm/Analysis/TypeMetadataUtils.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <set>
#include <unordered_map>

namespace input_dependency {

class IndirectCallSitesAnalysis::VirtualsImpl
{
private:
    struct VTableSlot
    {
        llvm::Metadata* TypeID;
        uint64_t ByteOffset; 
    };

    struct VirtualCallSite
    {
        llvm::Value* VTable;
        llvm::CallSite CS;
    };

    class VTableSlotEqual
    {
     public:
        bool operator() (const VTableSlot& slot1, const VTableSlot& slot2) const
        {
            return slot1.TypeID == slot2.TypeID && slot1.ByteOffset == slot2.ByteOffset;
        }
    };

    class VTableSlotHasher
    {
     public:
        unsigned long operator() (const VTableSlot& slot) const
        {
            return std::hash<llvm::Metadata*>{}(slot.TypeID) ^ std::hash<uint64_t>{}(slot.ByteOffset);
        }
    };

    using VirtualCallSites = std::vector<VirtualCallSite>;
    using VTableSlotCallSitesMap = std::unordered_map<VTableSlot, VirtualCallSites, VTableSlotHasher, VTableSlotEqual>;

public:
    void runOnModule(llvm::Module& M);

public:
    VirtualCallSiteAnalysisResult& getAnalysisResult();
    const VirtualCallSiteAnalysisResult& getAnalysisResult() const;
    
private:
    void collectTypeTestUsers(llvm::Function* F);
    void buildTypeIdentifierMap(std::vector<llvm::wholeprogramdevirt::VTableBits> &Bits,
                                std::unordered_map<llvm::Metadata*, std::set<llvm::wholeprogramdevirt::TypeMemberInfo>> &TypeIdMap);
    bool tryFindVirtualCallTargets(std::vector<llvm::wholeprogramdevirt::VirtualCallTarget>& TargetsForSlot,
                                   const std::set<llvm::wholeprogramdevirt::TypeMemberInfo>& TypeMemberInfos,
                                   uint64_t ByteOffset);
    void updateResults(const std::vector<VirtualCallSite>& S,
                       const std::vector<llvm::wholeprogramdevirt::VirtualCallTarget> TargetsForSlot);

private:
   llvm::Module* m_module; 
   VirtualCallSiteAnalysisResult m_results;
   VTableSlotCallSitesMap m_callSlots;

};

VirtualCallSiteAnalysisResult& IndirectCallSitesAnalysis::VirtualsImpl::getAnalysisResult()
{
    return m_results;
}

const VirtualCallSiteAnalysisResult& IndirectCallSitesAnalysis::VirtualsImpl::getAnalysisResult() const
{
    return m_results;
}

void IndirectCallSitesAnalysis::VirtualsImpl::runOnModule(llvm::Module& M)
{
    m_module = &M;

    llvm::Function* TypeTestFunc = M.getFunction(llvm::Intrinsic::getName(llvm::Intrinsic::type_test));
    llvm::Function *TypeCheckedLoadFunc = M.getFunction(llvm::Intrinsic::getName(llvm::Intrinsic::type_checked_load));
    llvm::Function *AssumeFunc = M.getFunction(llvm::Intrinsic::getName(llvm::Intrinsic::assume));

    if ((!TypeTestFunc || TypeTestFunc->use_empty() || !AssumeFunc ||
                AssumeFunc->use_empty()) &&
            (!TypeCheckedLoadFunc || TypeCheckedLoadFunc->use_empty()))
        return;

    if (TypeTestFunc && AssumeFunc) {
        collectTypeTestUsers(TypeTestFunc);
    }

    std::vector<llvm::wholeprogramdevirt::VTableBits> Bits;
    std::unordered_map<llvm::Metadata*, std::set<llvm::wholeprogramdevirt::TypeMemberInfo>> TypeIdMap;
    buildTypeIdentifierMap(Bits, TypeIdMap);
    if (TypeIdMap.empty()) {
        return;
    }
    for (auto& S : m_callSlots) {
        std::vector<llvm::wholeprogramdevirt::VirtualCallTarget> TargetsForSlot;
        if (!tryFindVirtualCallTargets(TargetsForSlot, TypeIdMap[S.first.TypeID], S.first.ByteOffset)) {
            continue;
        }
        updateResults(S.second, TargetsForSlot);
    }

    //m_results.dump();
    // cleanup uneccessary data
    m_callSlots.clear();
}

void IndirectCallSitesAnalysis::VirtualsImpl::collectTypeTestUsers(llvm::Function* F)
{
    auto I = F->use_begin();
    while (I != F->use_end()) {
        auto CI = llvm::dyn_cast<llvm::CallInst>(I->getUser());
        ++I;
        if (!CI) {
            continue;
        }
        llvm::SmallVector<llvm::DevirtCallSite, 1> DevirtCalls;
        llvm::SmallVector<llvm::CallInst *, 1> Assumes;
        llvm::findDevirtualizableCallsForTypeTest(DevirtCalls, Assumes, CI);

        if (Assumes.empty()) {
            return;
        }
        std::unordered_set<llvm::Value*> SeenPtrs;
        llvm::Metadata* TypeId = llvm::cast<llvm::MetadataAsValue>(CI->getArgOperand(1))->getMetadata();
        llvm::Value* Ptr = CI->getArgOperand(0)->stripPointerCasts();
        if (!SeenPtrs.insert(Ptr).second) {
            continue;
        }
        for (const auto& Call : DevirtCalls) {
            m_callSlots[{TypeId, Call.Offset}].push_back({CI->getArgOperand(0), Call.CS});
        }
    }
}

void IndirectCallSitesAnalysis::VirtualsImpl::buildTypeIdentifierMap(
                                          std::vector<llvm::wholeprogramdevirt::VTableBits>& Bits,
                                          std::unordered_map<llvm::Metadata*, std::set<llvm::wholeprogramdevirt::TypeMemberInfo>>& TypeIdMap)
{
    llvm::DenseMap<llvm::GlobalVariable*, llvm::wholeprogramdevirt::VTableBits*> GVToBits;
    Bits.reserve(m_module->getGlobalList().size());
    llvm::SmallVector<llvm::MDNode *, 2> Types;
    for (auto& GV : m_module->globals()) {
        Types.clear();
        GV.getMetadata(llvm::LLVMContext::MD_type, Types);
        if (Types.empty())
            continue;

        llvm::wholeprogramdevirt::VTableBits *&BitsPtr = GVToBits[&GV];
        if (!BitsPtr) {
            Bits.emplace_back();
            Bits.back().GV = &GV;
            Bits.back().ObjectSize = m_module->getDataLayout().getTypeAllocSize(GV.getInitializer()->getType());
            BitsPtr = &Bits.back();
        }

        for (auto Type : Types) {
            auto TypeID = Type->getOperand(1).get();
            uint64_t Offset = llvm::cast<llvm::ConstantInt>(llvm::cast<llvm::ConstantAsMetadata>(
                                                                            Type->getOperand(0))->getValue())->getZExtValue();
            TypeIdMap[TypeID].insert({BitsPtr, Offset});
        }
    }
}

bool IndirectCallSitesAnalysis::VirtualsImpl::tryFindVirtualCallTargets(
                                   std::vector<llvm::wholeprogramdevirt::VirtualCallTarget>& TargetsForSlot,
                                   const std::set<llvm::wholeprogramdevirt::TypeMemberInfo>& TypeMemberInfos,
                                   uint64_t ByteOffset)
{
    for (const auto& TM : TypeMemberInfos) {
        if (!TM.Bits->GV->isConstant()) {
            return false;
        }

        auto Init = llvm::dyn_cast<llvm::ConstantArray>(TM.Bits->GV->getInitializer());
        if (!Init) {
            return false;
        }
        llvm::ArrayType* VTableTy = Init->getType();

        uint64_t ElemSize = m_module->getDataLayout().getTypeAllocSize(VTableTy->getElementType());
        uint64_t GlobalSlotOffset = TM.Offset + ByteOffset;
        if (GlobalSlotOffset % ElemSize != 0) {
            return false;
        }

        unsigned Op = GlobalSlotOffset / ElemSize;
        if (Op >= Init->getNumOperands()) {
            return false;
        }

        auto Fn = llvm::dyn_cast<llvm::Function>(Init->getOperand(Op)->stripPointerCasts());
        if (!Fn) {
            return false;
        }

        // We can disregard __cxa_pure_virtual as a possible call target, as
        // calls to pure virtuals are UB.
        if (Fn->getName() == "__cxa_pure_virtual")
            continue;

        TargetsForSlot.push_back({Fn, &TM});
    }

    // Give up if we couldn't find any targets.
    return !TargetsForSlot.empty();
}

void IndirectCallSitesAnalysis::VirtualsImpl::updateResults(const std::vector<VirtualCallSite>& S,
                                                   const std::vector<llvm::wholeprogramdevirt::VirtualCallTarget> TargetsForSlot)
{
    for (const auto& cs : S) {
        FunctionSet candidates;
        for (const auto& slot : TargetsForSlot) {
            candidates.insert(slot.Fn);
        }
        if (cs.CS.isCall()) {
            m_results.addVirtualCallCandidates(llvm::dyn_cast<llvm::CallInst>(cs.CS.getInstruction()), std::move(candidates));
        } else if (cs.CS.isInvoke()) {
            m_results.addVirtualInvokeCandidates(llvm::dyn_cast<llvm::InvokeInst>(cs.CS.getInstruction()), std::move(candidates));
        }
    }
}

void VirtualCallSiteAnalysisResult::addVirtualCall(llvm::CallInst* call)
{
    addInstr(call);
}

void VirtualCallSiteAnalysisResult::addVirtualCallCandidates(llvm::CallInst* call, FunctionSet&& candidates)
{
    addCandidates(call, std::move(candidates));
}

void VirtualCallSiteAnalysisResult::addVirtualInvoke(llvm::InvokeInst* invoke)
{
    addInstr(invoke);
}

void VirtualCallSiteAnalysisResult::addVirtualInvokeCandidates(llvm::InvokeInst* call, FunctionSet&& candidates)
{
    addCandidates(call, std::move(candidates));
}


bool VirtualCallSiteAnalysisResult::hasVirtualCallCandidates(llvm::CallInst* call) const
{
    return hasCandidates(call);
}

const FunctionSet& VirtualCallSiteAnalysisResult::getVirtualCallCandidates(llvm::CallInst* call) const
{
    return getCandidates(call);
}

bool VirtualCallSiteAnalysisResult::hasVirtualInvokeCandidates(llvm::InvokeInst* invoke) const
{
    return hasCandidates(invoke);
}

const FunctionSet& VirtualCallSiteAnalysisResult::getVirtualInvokeCandidates(llvm::InvokeInst* invoke) const
{
    return getCandidates(invoke);
}

void VirtualCallSiteAnalysisResult::dump()
{
    for (const auto& item : m_virtualCallCandidates) {
        llvm::dbgs() << "Virtual call: " << *item.first << " candidates\n";
        for (const auto& candidate : item.second) {
            llvm::dbgs() << candidate->getName() << "\n";
        }
    }
}

void VirtualCallSiteAnalysisResult::addInstr(llvm::Instruction* instr)
{
    m_virtualCallCandidates[instr];
}

void VirtualCallSiteAnalysisResult::addCandidates(llvm::Instruction* instr, FunctionSet&& candidates)
{
    m_virtualCallCandidates[instr].insert(candidates.begin(), candidates.end());
}

bool VirtualCallSiteAnalysisResult::hasCandidates(llvm::Instruction* instr) const
{
    return m_virtualCallCandidates.find(instr) != m_virtualCallCandidates.end();
}

const FunctionSet& VirtualCallSiteAnalysisResult::getCandidates(llvm::Instruction* instr) const
{
    auto pos = m_virtualCallCandidates.find(instr);
    return pos->second;
}

char IndirectCallSitesAnalysis::ID = 0;

IndirectCallSitesAnalysis::IndirectCallSitesAnalysis()
    : llvm::ModulePass(ID)
    , m_vimpl(new VirtualsImpl())
    , m_iimpl(new IndirectsImpl())
{
}

bool IndirectCallSitesAnalysis::runOnModule(llvm::Module& M)
{
    m_vimpl->runOnModule(M);
    m_iimpl->runOnModule(M);
    return false;
}

VirtualCallSiteAnalysisResult& IndirectCallSitesAnalysis::getVirtualsAnalysisResult()
{
    return m_vimpl->getAnalysisResult();
}

const VirtualCallSiteAnalysisResult& IndirectCallSitesAnalysis::getVirtualsAnalysisResult() const
{
    return m_vimpl->getAnalysisResult();
}

static llvm::RegisterPass<IndirectCallSitesAnalysis> X("indirect-calls","runs indirect and virtual call analysis pass");

}

