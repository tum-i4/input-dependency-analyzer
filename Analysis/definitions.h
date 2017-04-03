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
class Value;
class CallInst;
class BranchInst;
class LoadInst;
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
//typedef std::unordered_map<llvm::Instruction*, ArgumentSet> InstrDependencyMap;

} // namespace input_dependency

