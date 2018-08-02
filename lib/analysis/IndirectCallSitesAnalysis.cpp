#include "analysis/IndirectCallSitesAnalysis.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/MapVector.h"

#include "llvm/Analysis/TypeMetadataUtils.h"
#include "llvm/Transforms/IPO/WholeProgramDevirt.h"
#include "llvm/Analysis/TypeMetadataUtils.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <set>
#include <unordered_map>

namespace input_dependency {

namespace {

template <class CallInstTy>
bool isIndirectCall(CallInstTy* inst)
{
    return inst->getCalledFunction() == nullptr;
}

}

class IndirectCallSitesAnalysis::IndirectsImpl
{
public:
    IndirectsImpl(IndirectCallSitesAnalysisResult& results);
    void runOnModule(llvm::Module& M);

private:
    IndirectCallSitesAnalysisResult&  m_results;
};

void IndirectCallSitesAnalysis::IndirectsImpl::runOnModule(llvm::Module& M)
{
    for (auto& F : M) {
        if (F.isDeclaration()) {
            continue;
        }
        auto type = F.getFunctionType();
        m_results.addIndirectCallTarget(type, &F);
    }
}

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
    using FunctionSet = IndirectCallSitesAnalysisResult::FunctionSet;
    VirtualsImpl(IndirectCallSitesAnalysisResult& results);

public:
    void runOnModule(llvm::Module& M);

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
   VTableSlotCallSitesMap m_callSlots;
   IndirectCallSitesAnalysisResult& m_results;
};

IndirectCallSitesAnalysis::VirtualsImpl::VirtualsImpl(IndirectCallSitesAnalysisResult& results)
    : m_results(results)
{
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
        llvm::FunctionType* functionType = cs.CS.getFunctionType();
        m_results.addIndirectCallTargets(functionType, std::move(candidates));
    }
}

void IndirectCallSitesAnalysisResult::addIndirectCallTarget(llvm::FunctionType* type, llvm::Function* target)
{
    m_indirectCallTargets[type].insert(target);
}

void IndirectCallSitesAnalysisResult::addIndirectCallTargets(llvm::FunctionType* type, const FunctionSet& targets)
{
    m_indirectCallTargets[type].insert(targets.begin(), targets.end());
}

bool IndirectCallSitesAnalysisResult::hasIndirectTargets(llvm::FunctionType* func_ty) const
{
    return m_indirectCallTargets.find(func_ty) != m_indirectCallTargets.end();
}

const IndirectCallSitesAnalysisResult::FunctionSet& IndirectCallSitesAnalysisResult::getIndirectTargets(llvm::FunctionType* func_ty) const
{
    auto pos = m_indirectCallTargets.find(func_ty);
    return pos->second;
}

bool IndirectCallSitesAnalysisResult::hasIndirectTargets(const llvm::CallSite& callSite) const
{
    return hasIndirectTargets(callSite.getFunctionType());
}

const IndirectCallSitesAnalysisResult::FunctionSet& IndirectCallSitesAnalysisResult::getIndirectTargets(const llvm::CallSite& callSite) const
{
    return getIndirectTargets(callSite.getFunctionType());
}

void IndirectCallSitesAnalysisResult::dump()
{
    for (const auto& item : m_indirectCallTargets) {
        llvm::dbgs() << "Indirect call: " << *item.first << " candidates\n";
        for (const auto& candidate : item.second) {
            llvm::dbgs() << candidate->getName() << "\n";
        }
    }
}

char IndirectCallSitesAnalysis::ID = 0;

IndirectCallSitesAnalysis::IndirectCallSitesAnalysis()
    : llvm::ModulePass(ID)
    , m_vimpl(new VirtualsImpl(m_results))
    , m_iimpl(new IndirectsImpl(m_results))
{
}

bool IndirectCallSitesAnalysis::runOnModule(llvm::Module& M)
{
    m_vimpl->runOnModule(M);
    m_iimpl->runOnModule(M);
    return false;
}

IndirectCallSitesAnalysisResult& IndirectCallSitesAnalysis::getIndirectsAnalysisResult()
{
    return m_results;
}

const IndirectCallSitesAnalysisResult& IndirectCallSitesAnalysis::getIndirectsAnalysisResult() const
{
    return m_results;
}

static llvm::RegisterPass<IndirectCallSitesAnalysis> X("indirect-calls","runs indirect and virtual calls analysis");

}


