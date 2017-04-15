#pragma once

#include <functional>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace llvm {

class AAResults;
class Argument;
class BasicBlock;
class Function;
class Instruction;
class Module;
class Value;
class BranchInst;
class CallInst;
class LoadInst;
class InvokeInst;
class ReturnInst;
class StoreInst;

}

namespace input_dependency {

class FunctionAnaliser;

typedef std::vector<llvm::Argument*> Arguments;
typedef std::unordered_set<llvm::Value*> ValueSet;
typedef std::unordered_set<llvm::Argument*> ArgumentSet;
typedef std::function<const FunctionAnaliser* (llvm::Function*)> FunctionAnalysisGetter; 

using FunctionSet = std::unordered_set<llvm::Function*>;
using CalleeCallersMap = std::unordered_map<llvm::Function*, FunctionSet>;

typedef std::unordered_set<llvm::Instruction*> InstrSet;

} // namespace input_dependency

