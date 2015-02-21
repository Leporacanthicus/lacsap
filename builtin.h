#ifndef BUILTIN_H
#define BUILTIN_H

#include "namedobject.h"
#include "stack.h"
#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <string>
#include <vector>

namespace Builtin
{
    bool IsBuiltin(const std::string& funcname);
    llvm::Value* CodeGen(llvm::IRBuilder<>& builder,
			 const std::string& funcname,
			 const std::vector<ExprAST*>& args);
    Types::TypeDecl* Type(Stack<NamedObject*>& ns,
			  const std::string& funcname,
			  const std::vector<ExprAST*>& args);
};

#endif
