#pragma once

#include <memory>
#include <vector>

namespace llvm {

class Value;
} // namespace llvm

namespace pdg {

class PDGNode;

/// Interface to query def-use results
class DefUseResults
{
public:
    using PDGNodeTy = std::shared_ptr<PDGNode>;
    using PDGNodes = std::vector<PDGNodeTy>;

public:
    virtual ~DefUseResults() {}

    virtual PDGNodeTy getDefSite(llvm::Value* value) = 0;
}; // class DefUseResults

} // namespace pdg

