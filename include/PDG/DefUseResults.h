#pragma once

#include <memory>
#include <unordered_set>

namespace llvm {

class CallSite;
class Function;
class Value;
} // namespace llvm

namespace pdg {

/// Interface to query def-use results
class DefUseResults
{
public:
    using PDGNodeTy = std::shared_ptr<PDGNode>;
    using PDGNodes = std::vector<PDGNodeTy>;
    using FunctionSet = std::unordered_set<llvm::Function*>;

public:
    virtual ~DefUseResults() {}

    virtual PDGNodeTy getDefSite(llvm::Value* value) = 0;
    virtual PDGNodes getDefSites(llvm::Value* value) = 0;
    virtual bool hasIndCSCallees(const llvm::CallSite& callSite) const = 0;
    virtual FunctionSet getIndCSCallees(const llvm::CallSite& callSite) = 0;
}; // class DefUseResults

} // namespace pdg

