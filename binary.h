#ifndef BINARY_H
#define BINARY_H
#include "options.h"
#include <string>
#include <llvm/IR/Module.h>

void CreateBinary(llvm::Module *module, const std::string& fileName, EmitType emit);

#endif
