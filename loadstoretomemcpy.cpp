#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Dominators.h"

#include <iostream>


using namespace llvm;

namespace llvm
{
    void initializeLdSt2MemCpyOptPass(PassRegistry&);
};

namespace {
    class LdSt2MemCpyOpt : public FunctionPass {
      MemoryDependenceAnalysis *MD;
      TargetLibraryInfo *TLI;
      const DataLayout *DL;
  public:
      static char ID; // Pass identification, replacement for typeid
      LdSt2MemCpyOpt() : FunctionPass(ID) {
	  initializeLdSt2MemCpyOptPass(*PassRegistry::getPassRegistry());
	  MD = nullptr;
	  TLI = nullptr;
	  DL = nullptr;
    }

    bool runOnFunction(Function &F) override;

  private:
    // This transformation requires dominator postdominator info
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesCFG();
      AU.addRequired<DominatorTreeWrapperPass>();
      AU.addRequired<MemoryDependenceAnalysis>();
      AU.addRequired<AliasAnalysis>();
      AU.addRequired<TargetLibraryInfo>();
      AU.addPreserved<AliasAnalysis>();
      AU.addPreserved<MemoryDependenceAnalysis>();
    }

#if 0
    // Helper fuctions
    bool processStore(StoreInst *SI, BasicBlock::iterator &BBI);
    bool processMemSet(MemSetInst *SI, BasicBlock::iterator &BBI);
    bool processMemCpy(MemCpyInst *M);
    bool processMemMove(MemMoveInst *M);
    bool processByValArgument(CallSite CS, unsigned ArgNo);
#endif

	bool iterateOnFunction(Function &F);
  };

  char LdSt2MemCpyOpt::ID = 0;

bool LdSt2MemCpyOpt::runOnFunction(Function &F)
{
    std::cerr << "I'm here" << std::endl;

    return false;
}

};

INITIALIZE_PASS_BEGIN(LdSt2MemCpyOpt, "ldst2memcpyopt", "Load/Store to MemCpy Optimization",
                      false, false)
    INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
    INITIALIZE_PASS_DEPENDENCY(MemoryDependenceAnalysis)
    INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfo)
    INITIALIZE_AG_DEPENDENCY(AliasAnalysis)
INITIALIZE_PASS_END(LdSt2MemCpyOpt, "ldst2memcpyopt", "MemCpy Optimization",
                    false, false)


namespace llvm
{
    FunctionPass *createLdSt2MemCpyOptPass()
    {
	return new LdSt2MemCpyOpt();
    }
};
