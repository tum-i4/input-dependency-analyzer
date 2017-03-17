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
//typedef std::unordered_map<llvm::Argument*, ArgumentSet> ArgumentDependenciesMap;
// key is an argument, and the value are actual arguments, it depends on. Used for function info collecting
//typedef std::unordered_map<llvm::Function*, ArgumentDependenciesMap> FunctionArgumentsDependencies;

typedef std::function<const FunctionAnaliser* (llvm::Function*)> FunctionAnalysisGetter; 

typedef std::unordered_set<llvm::Instruction*> InstrSet;
//TODO: shouldn't this be DepInfo?
typedef std::unordered_map<llvm::Instruction*, ArgumentSet> InstrDependencyMap;

} // namespace input_dependency

