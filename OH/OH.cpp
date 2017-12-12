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

//#include "CutVertice/CutVerticesPass.h"
#include "Analysis/InputDependencyAnalysis.h"

using namespace llvm;
namespace {
	struct OHPass : public FunctionPass {
		static char ID;

                unsigned count;
		OHPass()
                    : FunctionPass(ID)
                    , count(0)
                {}
		virtual bool runOnFunction(Function &F){
			bool didModify = false;
			for (auto& B : F) {
                auto FI = getAnalysis<input_dependency::InputDependencyAnalysisPass>().getInputDependencyAnalysis()->getAnalysisInfo(&F);
				//std::vector<const char*> CutVertices=getAnalysis<CutVerticesPass>().getArray();
				//if(!CutVertices.empty()&& std::find(CutVertices.begin(),CutVertices.end(),
				//	B.getName())!=CutVertices.end()){
				//	errs() << "Cut Vertices: " << B.getName() << "\n";
				//}
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
					//else if(ReturnInst *RI = dyn_cast<ReturnInst>(&I)){
					//	// Insert *before* ret
					//	dbgs() << "**returnInst**\n";
					//	printHash(&B, RI, true);	
					//	didModify = true;
					//}
				}
			}
                        printHash(&F.back(), count);
                        ++count;
			return didModify;
		}

		virtual void getAnalysisUsage(AnalysisUsage &AU) const {
			//AU.addRequired<CutVerticesPass>();
			AU.addRequired<input_dependency::InputDependencyAnalysisPass>();
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
			Constant* hashFunc;
                        if (value->getType()->isIntegerTy(32)) {
                            llvm::dbgs() << "hash me for integer\n";
                            hashFunc = BB->getParent()->getParent()->getOrInsertFunction(
                                    "hashMeInt", Type::getVoidTy(Ctx), Type::getInt32Ty(Ctx), NULL);
                        } else if (value->getType()->isIntegerTy(64)) {
                            llvm::dbgs() << "hash me for long integer\n";
                            hashFunc = BB->getParent()->getParent()->getOrInsertFunction(
                                    "hashMeLong", Type::getVoidTy(Ctx), Type::getInt64Ty(Ctx), NULL);
                        } else {
                            llvm::dbgs() << "skip hashing for type " << *value->getType();
                            return;
                        }

			IRBuilder <> builder(I);
			auto insertPoint = ++builder.GetInsertPoint();
			if(insertBeforeInstruction){
				insertPoint--;
				insertPoint--;
			}
			Value *args = {value};
			builder.SetInsertPoint(BB, insertPoint);
			//printArg(BB, &builder, value->getName());
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
		void printHash(BasicBlock *BB, unsigned count) {
			LLVMContext& Ctx = BB->getParent()->getContext();
			// get BB parent -> Function -> get parent -> Module 
                        llvm::ArrayRef<llvm::Type*> params{llvm::Type::getInt32Ty(Ctx)};
                        llvm::FunctionType* function_type = llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), params, false);
			Constant* logHashFunc = BB->getParent()->getParent()->getOrInsertFunction(
					                    "logHash", function_type);
			IRBuilder <> builder(BB);
                        builder.SetInsertPoint(BB, --builder.GetInsertPoint());
			dbgs() << "FuncName: "<<BB->getParent()->getName()<<"\n";

                        std::vector<llvm::Value*> arg_values;
                        arg_values.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), count));
                        llvm::ArrayRef<llvm::Value*> args(arg_values);
			builder.CreateCall(logHashFunc, args);	
		}
	};
}

char OHPass::ID = 0;

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerOHPass(const PassManagerBuilder &,
		legacy::PassManagerBase &PM) {
    PM.add(new input_dependency::InputDependencyAnalysisPass());
    //PM.add(new CutVerticesPass());
    PM.add(new OHPass());
}
static RegisterStandardPasses
RegisterOHPass(PassManagerBuilder::EP_EarlyAsPossible,
		registerOHPass);

static llvm::RegisterPass<OHPass> X("oh","runs oblivious hashing");

