#include "input-dependency/Analysis/ValueDepInfo.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

namespace {

int64_t get_composite_type_elements_num(llvm::Type* type)
{
    int64_t el_num = -1;
    if (auto struct_type = llvm::dyn_cast<llvm::StructType>(type)) {
        el_num = struct_type->getNumElements();
    } else if (auto array_type = llvm::dyn_cast<llvm::ArrayType>(type)) {
        el_num = array_type->getNumElements();
    } else if (type->isPointerTy()) {
        // at this point don't know if pointer is heap applocated array or not.
        el_num = 0;
    }
    return el_num;
}

}

ValueDepInfo::ValueDepInfo(llvm::Type* type)
    : m_depInfo(DepInfo::INPUT_INDEP)
{
    int64_t el_num = get_composite_type_elements_num(type);
    if (el_num != -1) {
        m_isComposite = true;
        m_elementDeps.resize(el_num, ValueDepInfo(DepInfo(DepInfo::INPUT_INDEP)));
    }
}

ValueDepInfo::ValueDepInfo(const DepInfo& depInfo)
    : m_depInfo(depInfo)
{
}

ValueDepInfo::ValueDepInfo(llvm::Type* type, const DepInfo& depInfo)
    : m_depInfo(depInfo)
{
    int64_t el_num = get_composite_type_elements_num(type);
    if (el_num != -1) {
        m_isComposite = true;
        m_elementDeps.resize(el_num, ValueDepInfo(depInfo));
    }
}

const ValueDepInfo& ValueDepInfo::getValueDep(llvm::Instruction* el_instr) const
{
    //llvm::dbgs() << "Element " << *el_instr << "\n";
    // assuming only way to get access to composite type element is with GetElementPtrInst instruction
    if (!m_isComposite) {
        return *this;
    }
    auto get_el_instr = llvm::dyn_cast<llvm::GetElementPtrInst>(el_instr);
    if (!get_el_instr) {
        return *this;
    }
    // get element index
    unsigned last_index = get_el_instr->getNumIndices();
    auto idx_op = get_el_instr->getOperand(last_index);
    if (auto* const_idx = llvm::dyn_cast<llvm::ConstantInt>(idx_op)) {
        int64_t idx = const_idx->getSExtValue();
        if (idx >= 0) {
            if (m_elementDeps.size() <= idx) {
                return *this;
            }
            return m_elementDeps[idx];
        }
    }
    // element accessed with non-const index may be any of the elements,
    // m_depInfo contains info for all elements, thus returning it is safe
    return *this;
}

void ValueDepInfo::updateValueDep(const ValueDepInfo& valueDepInfo)
{
    m_depInfo = valueDepInfo.getValueDep();
    if (valueDepInfo.getCompositeValueDeps().empty()) {
        updateCompositeValueDep(valueDepInfo.getValueDep());
    } else {
        // update element dependencies existing in valueDepInfo, keep the others the same
        const auto& valueElementsInfo = valueDepInfo.getCompositeValueDeps();
        m_elementDeps.reserve(std::max(m_elementDeps.size(), valueElementsInfo.size()));
        for (unsigned i = 0; i < std::min(m_elementDeps.size(), valueElementsInfo.size()); ++i) {
            m_elementDeps[i] = valueElementsInfo[i];
        }
        if (m_elementDeps.size() < valueElementsInfo.size()) {
            for (unsigned i = m_elementDeps.size(); i < valueElementsInfo.size(); ++i) {
                m_elementDeps.push_back(valueElementsInfo[i]);
            }
        }
    }
}

void ValueDepInfo::updateValueDep(const DepInfo& depInfo)
{
    m_depInfo = depInfo;
}

void ValueDepInfo::updateCompositeValueDep(const DepInfo& depInfo)
{
    m_depInfo = depInfo;
    for (auto& dep : m_elementDeps) {
        dep.updateCompositeValueDep(depInfo);
    }
}

void ValueDepInfo::updateValueDep(llvm::Instruction* el_instr,
                                  const ValueDepInfo& depInfo)
{
    //llvm::dbgs() << "Element: " << *el_instr << "\n";
    if (!m_isComposite) {
        m_depInfo = depInfo.getValueDep();
        return;
    }
    auto get_el_instr = llvm::dyn_cast<llvm::GetElementPtrInst>(el_instr);
    if (!get_el_instr) {
        updateCompositeValueDep(depInfo.getValueDep());
        return;
    }
    // get element index
    int64_t idx = -1;
    auto idx_op = get_el_instr->getOperand(get_el_instr->getNumIndices());
    if (auto* const_idx = llvm::dyn_cast<llvm::ConstantInt>(idx_op)) {
        idx = const_idx->getSExtValue();
    }
   if (idx >= 0) {
       if (m_elementDeps.size() <= idx) {
           m_elementDeps.resize(idx + 1, ValueDepInfo(m_depInfo));
       }
       m_elementDeps[idx] = depInfo;
   } else {
       // If the index is not constant, assign given input dep info to every element
       std::for_each(m_elementDeps.begin(), m_elementDeps.end(), [&depInfo] (ValueDepInfo& el_dep) {el_dep.mergeDependencies(depInfo);});
   }
    m_depInfo = DepInfo(DepInfo::INPUT_INDEP);
    // this will increase runtime, but is the correct way to process
    std::for_each(m_elementDeps.begin(), m_elementDeps.end(),
                  [this] (ValueDepInfo& el_dep) {
                      this->m_depInfo.mergeDependencies(el_dep.getValueDep());});
}

void ValueDepInfo::mergeDependencies(const ValueDepInfo& depInfo)
{
    m_depInfo.mergeDependencies(depInfo.getValueDep());

    if (!m_isComposite) {
        return;
    }
    const ValueDeps& valueDeps = depInfo.getCompositeValueDeps();
    const auto& el_size = m_elementDeps.size();
    m_elementDeps.reserve(std::max(el_size, valueDeps.size()));
    for (unsigned i = 0; i < std::min(el_size, valueDeps.size()); ++i) {
        m_elementDeps[i].mergeDependencies(valueDeps[i]);
    }
    if (m_elementDeps.size() < valueDeps.size()) {
        for (unsigned i = m_elementDeps.size(); i < valueDeps.size(); ++i) {
            m_elementDeps.push_back(valueDeps[i]);
        }
    }
}

void ValueDepInfo::mergeDependencies(llvm::Instruction* el_instr, const ValueDepInfo& depInfo)
{
    auto* get_el_instr =  llvm::dyn_cast<llvm::GetElementPtrInst>(el_instr);
    if (!m_isComposite) {
        // Fix for bug #92.This is a quick fix which needs to be reviewed again.
        if (!get_el_instr) {
            m_depInfo.mergeDependencies(depInfo.getValueDep());
        }
        return;
    }
    if (!get_el_instr) {
        m_depInfo.mergeDependencies(depInfo.getValueDep());
        for (auto& dep : m_elementDeps) {
            dep.mergeDependencies(depInfo);
        }
        return;
    }
    m_depInfo.mergeDependencies(depInfo.getValueDep());
    auto idx_op = get_el_instr->getOperand(get_el_instr->getNumIndices());
    if (auto* const_idx = llvm::dyn_cast<llvm::ConstantInt>(idx_op)) {
        int64_t idx = const_idx->getSExtValue();
        if (idx >= 0) {
            if (m_elementDeps.size() <= idx)  {
                //m_elementDeps.resize(idx + 1, ValueDepInfo(DepInfo::INPUT_INDEP));
                // TODO: try to decide finally, what should happen here.
                m_elementDeps.resize(idx + 1, ValueDepInfo(m_depInfo));
            }
            m_elementDeps[idx].mergeDependencies(depInfo.getValueDep());
            return;
        }
    }
    std::for_each(m_elementDeps.begin(), m_elementDeps.end(), [&depInfo] (ValueDepInfo& el_dep) {el_dep.mergeDependencies(depInfo);});
}

} // namespace input_dependency

