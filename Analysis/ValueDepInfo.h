#pragma once

#include "DependencyInfo.h"

#include <vector>
#include <functional>

namespace llvm {
class AllocaInst;
class Instruction;
}

namespace input_dependency {

class ValueDepInfo
{
public:
    using ValueDeps = std::vector<DepInfo>;
    using ValueDepRequest = std::function<const DepInfo& (llvm::Value* )>;

public:
    ValueDepInfo() = default;

    ValueDepInfo(llvm::Value* val);
    ValueDepInfo(llvm::AllocaInst* alloca);
    ValueDepInfo(llvm::Value* val, const DepInfo& depInfo);

public:
    llvm::Value* getValue() const;
    const DepInfo& getValueDep() const;
    DepInfo& getValueDep();
    const ValueDeps& getCompositeValueDeps() const;
    ValueDeps& getCompositeValueDeps();
    const DepInfo& getValueDep(llvm::Instruction* el_instr,
                               const ValueDepRequest& valueDepRequester);

    void updateValueDep(const DepInfo& depInfo);
    void updateCompositeValueDep(const DepInfo& depInfo);
    void updateValueDep(llvm::Instruction* el_instr,
                        const DepInfo& depInfo,
                        const ValueDepRequest& valueDepRequester);

private:
    llvm::Value* m_value;
    DepInfo m_depInfo;
    ValueDeps m_elementDeps;
}; // class ValueDepInfo

} // namespace input_dependency

