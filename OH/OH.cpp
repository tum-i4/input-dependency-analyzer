#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h" 

#include "CutVertice/CutVerticesPass.h"
#include "Analysis/InputDependencyAnalysis.h"

using namespace llvm;
namespace {
	struct OHPass : public FunctionPass {
		static char ID;
		OHPass() : FunctionPass(ID) {}
		virtual bool runOnFunction(Function &F){
			bool didModify = false;
			for (auto& B : F) {
                                auto FI = getAnalysis<input_dependency::InputDependencyAnalysis>().getAnalysisInfo(&F);
				std::vector<const char*> CutVertices=getAnalysis<CutVerticesPass>().getArray();
				if(!CutVertices.empty()&& std::find(CutVertices.begin(),CutVertices.end(),
					B.getName())!=CutVertices.end()){
					errs() << "Cut Vertices: " << B.getName() << "\n";
				}
                continue;
				for (auto& I : B) {
					//dbgs() << I << I.getOpcodeName() << "\n";
                                        if (FI->isInputDependent(&I)) {
                                            continue;
                                        }
					if (auto* op = dyn_cast<BinaryOperator>(&I)) {
						// Insert *after* `op`.
						updateHash(&B, &I, op, false);
						didModify =true;
					} else if (CmpInst* cmpInst = dyn_cast<CmpInst>(&I)){
						didModify = handleCmp(cmpInst,&B);
					} else if (StoreInst* storeInst = dyn_cast<StoreInst>(&I)){
						didModify = handleStore(storeInst, &B);
					} //TODO: else if (handle switch case and other conditions)
					//terminator indicates the last block
					else if(ReturnInst *RI = dyn_cast<ReturnInst>(&I)){
						// Insert *before* ret
						dbgs() << "**returnInst**\n";
						printHash(&B, RI, true);	
						didModify = true;
					}
				}
			}
			return didModify;
		}

		virtual void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.addRequired<CutVerticesPass>();
			AU.addRequired<input_dependency::InputDependencyAnalysis>();
			AU.setPreservesAll();
		}

		bool handleStore(StoreInst *storeInst, BasicBlock *BB){
			dbgs() << "**HandleStore**\n";
			if(storeInst->getNumOperands()>0){
				Value *val = storeInst -> getOperand(0);
				//skip the store instruction if its referring to a pointer
				//TODO: make sure this does not have harmful side effects
				if(!val->getType()->isPointerTy()){
					storeInst->print(dbgs());
					dbgs()<<"\n";
					//Insert *after* cmp;
					updateHash(BB, storeInst, val, false);
					std::string type_str;
					llvm::raw_string_ostream rso(type_str);
					val->print(rso);
					dbgs() << "Handled  Type:"<<val->getType()->isPointerTy()<<" "<<rso.str()<<"\n";
					return true;
				}

			}
			return false;
		}
		bool handleCmp(CmpInst *cmpInst, BasicBlock *BB){
			dbgs() << "**HandleCmp**\n";
			//first check whether cmp has two operands 
			if(cmpInst->getNumOperands() >=2){
				//get the left hand operand, i.e. condition value
				Value *secondOperand = cmpInst->getOperand(0);
				//Insert *before* cmp;
				updateHash(BB, cmpInst, secondOperand, false);
				return true;
			}
			return false;
		}
		void updateHash(BasicBlock *BB, Instruction *I, 
				Value *value, bool insertBeforeInstruction){
			LLVMContext& Ctx = BB->getParent()->getContext();
			// get BB parent -> Function -> get parent -> Module	
			Constant* hashFunc = BB->getParent()->getParent()->getOrInsertFunction(
					"hashMe", Type::getVoidTy(Ctx), Type::getInt32Ty(Ctx), NULL
					);
			IRBuilder <> builder(I);
			auto insertPoint = ++builder.GetInsertPoint();
			if(insertBeforeInstruction){
				insertPoint--;
				insertPoint--;
			}
			Value *args = {value};
			builder.SetInsertPoint(BB, insertPoint);
			printArg(BB, &builder, value->getName());
			builder.CreateCall(hashFunc, args);
		}
		void printArg(BasicBlock *BB, IRBuilder<> *builder, std::string valueName){
			LLVMContext &context = BB->getParent()->getContext();;
			std::vector<llvm::Type *> args;
			args.push_back(llvm::Type::getInt8PtrTy(context));
			// accepts a char*, is vararg, and returns int
			FunctionType *printfType =
				llvm::FunctionType::get(builder->getInt32Ty(), args, true);
			Constant *printfFunc =
				BB->getParent()->getParent()->getOrInsertFunction("printf", printfType);
			Value *formatStr = builder->CreateGlobalStringPtr("arg = %s\n");
			Value *argument = builder->CreateGlobalStringPtr(valueName);
			std::vector<llvm::Value *> values;
			values.push_back(formatStr);
			values.push_back(argument);
			builder->CreateCall(printfFunc, values);
		}
		void printHash(BasicBlock *BB, Instruction *I, bool insertBeforeInstruction){
			LLVMContext& Ctx = BB->getParent()->getContext();
			// get BB parent -> Function -> get parent -> Module 
			Constant* logHashFunc = BB->getParent()->getParent()->getOrInsertFunction(
					"logHash", Type::getVoidTy(Ctx),NULL);
			IRBuilder <> builder(I);
			auto insertPoint = ++builder.GetInsertPoint();
			if(insertBeforeInstruction){
				insertPoint--;
				insertPoint--;
			}
			dbgs() << "FuncName: "<<BB->getParent()->getName()<<"\n";
			builder.SetInsertPoint(BB, insertPoint);
			builder.CreateCall(logHashFunc);	
		}
	};
}

char OHPass::ID = 0;

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerOHPass(const PassManagerBuilder &,
		legacy::PassManagerBase &PM) {
	PM.add(new CutVerticesPass());
    PM.add(new OHPass());
}
static RegisterStandardPasses
RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
		registerOHPass);
