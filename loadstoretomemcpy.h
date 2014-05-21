#ifndef LOADSTORETOMEMCPY_H__
#define LOADSTORETOMEMCPY_H__

namespace llvm
{
    llvm::FunctionPass *createLdSt2MemCpyOptPass();
}

#endif
