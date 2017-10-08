#include "ValueDepInfo.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

ValueDepInfo::ValueDepInfo(llvm::Value* val)
    : m_value(val)
    , m_depInfo(DepInfo::INPUT_INDEP)
{
}

ValueDepInfo::ValueDepInfo(llvm::AllocaInst* alloca)
    : m_value(alloca)
    , m_depInfo(DepInfo::INPUT_INDEP)
{
    int64_t el_num = -1;
    if (auto struct_type = llvm::dyn_cast<llvm::StructType>(alloca->getAllocatedType())) {
        el_num = struct_type->getNumElements();
    } else if (auto array_type = llvm::dyn_cast<llvm::ArrayType>(alloca->getAllocatedType())) {
        el_num = array_type->getNumElements();
    } else if (alloca->getAllocatedType()->isPointerTy()) {
        // at this point don't know if pointer is heap applocated array or not.
        el_num = 0;
    }
    if (el_num != -1) {
        m_elementDeps.resize(el_num, DepInfo(DepInfo::INPUT_INDEP));
    }
}

ValueDepInfo::ValueDepInfo(llvm::Value* val, const DepInfo& depInfo)
    : m_value(val)
    , m_depInfo(depInfo)
{
}

llvm::Value* ValueDepInfo::getValue() const
{
    return m_value;
}

const DepInfo& ValueDepInfo::getValueDep() const
{
    return m_depInfo;
}

DepInfo& ValueDepInfo::getValueDep()
{
    return m_depInfo;
}

const ValueDepInfo::ValueDeps& ValueDepInfo::getCompositeValueDeps() const
{
    return m_elementDeps;
}

ValueDepInfo::ValueDeps& ValueDepInfo::getCompositeValueDeps()
{
    return m_elementDeps;
}

const DepInfo& ValueDepInfo::getValueDep(llvm::Instruction* el_instr) const
{
    //llvm::dbgs() << "Get dep info for composite value " << *m_value << "\n";
    //llvm::dbgs() << "Element " << *el_instr << "\n";
    // assuming only way to get access to composite type element is with GetElementPtrInst instruction
    auto get_el_instr = llvm::dyn_cast<llvm::GetElementPtrInst>(el_instr);
    if (!get_el_instr || get_el_instr->getOperand(0) != m_value) {
        return m_depInfo;
    }
    // get element index
    unsigned last_index = get_el_instr->getNumIndices();
    auto idx_op = get_el_instr->getOperand(last_index);
    if (auto* const_idx = llvm::dyn_cast<llvm::ConstantInt>(idx_op)) {
        uint64_t idx = const_idx->getZExtValue();
        if (m_elementDeps.size() <= idx) {
            const_cast<ValueDepInfo*>(this)->m_elementDeps.resize(idx + 1, DepInfo(DepInfo::INPUT_INDEP));
        }
        return m_elementDeps[idx];
    }
    // element accessed with non-const index may be any of the elements,
    // m_depInfo contains info for all elements, thus returning it is safe
    return m_depInfo;
}

void ValueDepInfo::updateValueDep(const ValueDepInfo& valueDepInfo)
{
    m_depInfo = valueDepInfo.getValueDep();
    m_elementDeps = valueDepInfo.getCompositeValueDeps();
}

void ValueDepInfo::updateValueDep(const DepInfo& depInfo)
{
    m_depInfo = depInfo;
}

void ValueDepInfo::updateCompositeValueDep(const DepInfo& depInfo)
{
    m_depInfo = depInfo;
    for (auto& dep : m_elementDeps) {
        dep = depInfo;
    }
}

void ValueDepInfo::updateValueDep(llvm::Instruction* el_instr,
                                  const DepInfo& depInfo)
{
    //llvm::dbgs() << "Update dep for composite value " << *m_value << "\n";
    //llvm::dbgs() << "Element: " << *el_instr << "\n";
    auto get_el_instr = llvm::dyn_cast<llvm::GetElementPtrInst>(el_instr);
    if (!get_el_instr || get_el_instr->getOperand(0) != m_value) {
        m_depInfo = depInfo;
        return;
    }
    // get element index
    auto idx_op = get_el_instr->getOperand(get_el_instr->getNumIndices());
    if (auto* const_idx = llvm::dyn_cast<llvm::ConstantInt>(idx_op)) {
        uint64_t idx = const_idx->getZExtValue();
        if (m_elementDeps.size() <= idx) {
            m_elementDeps.resize(idx, DepInfo(DepInfo::INPUT_INDEP));
        }
        m_elementDeps[idx] = depInfo;
        // input dependency of composite type depends on input dep of each element
        m_depInfo.mergeDependencies(depInfo);
        return;
    }
    // If the index is not constant, assign given input dep info to every element
    std::for_each(m_elementDeps.begin(), m_elementDeps.end(), [&depInfo] (DepInfo& el_dep) {el_dep.mergeDependencies(depInfo);});
    // make composite type dependent on both given depInfo and dep info of the index
    m_depInfo.mergeDependencies(depInfo);
}

void ValueDepInfo::mergeDependencies(const ValueDepInfo& depInfo)
{
    m_depInfo.mergeDependencies(depInfo.getValueDep());

    const ValueDeps& valueDeps = depInfo.getCompositeValueDeps();
    const auto& el_size = m_elementDeps.size();
    assert(el_size == valueDeps.size());
    for (unsigned i = 0; i < el_size; ++i) {
        m_elementDeps[i].mergeDependencies(valueDeps[i]);
    }
}

} // namespace input_dependency

