#ifndef BINARY_H
#define BINARY_H
#include <string>
#include <llvm/IR/Module.h>

void CreateBinary(llvm::Module *module, const std::string& objname, const std::string& exename);

#endif
