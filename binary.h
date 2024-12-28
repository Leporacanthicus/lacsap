#ifndef BINARY_H
#define BINARY_H
#include "options.h"
#include <string>

namespace llvm
{
    class Module;
}

bool CreateBinary(llvm::Module* module, const std::string& fileName, EmitType emit);

llvm::Module* CreateModule();

#endif
