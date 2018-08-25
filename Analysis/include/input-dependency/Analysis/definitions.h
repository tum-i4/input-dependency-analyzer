#pragma once

#include <functional>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace llvm {

class AAResults;
class Argument;
class BitCastInst;
class BasicBlock;
class Function;
class GlobalVariable;
class Instruction;
class Module;
class Value;
class BranchInst;
class GetElementPtrInst;
class CallInst;
class LoadInst;
class InvokeInst;
class ReturnInst;
class StoreInst;
class PHINode;

}

namespace input_dependency {

class FunctionAnaliser;

using Arguments = std::vector<llvm::Argument*>;
using ValueSet = std::unordered_set<llvm::Value*>;
using GlobalsSet = std::unordered_set<llvm::GlobalVariable*>;
using ArgumentSet = std::unordered_set<llvm::Argument*>;
using FunctionAnalysisGetter = std::function<FunctionAnaliser* (llvm::Function*)>;
using FunctionSet = std::unordered_set<llvm::Function*>;
using CalleeCallersMap = std::unordered_map<llvm::Function*, FunctionSet>;
typedef std::unordered_set<llvm::Instruction*> InstrSet;

} // namespace input_dependency

