#ifndef BUILTIN_H
#define BUILTIN_H

#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <string>
#include <vector>

class Builtin
{
public:
    static bool IsBuiltin(const std::string& funcname);
    static llvm::Value* CodeGen(llvm::IRBuilder<>& builder,
				const std::string& funcname, 
				const std::vector<ExprAST*>& args);
};

#endif
