#ifndef BUILTIN_H
#define BUILTIN_H

#include "namedobject.h"
#include "stack.h"
#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <string>
#include <vector>

class ExprAST;

namespace Builtin
{
    // Deprecated interface.
    llvm::Value* CodeGen(llvm::IRBuilder<>& builder,
			 const std::string& funcname,
			 const std::vector<ExprAST*>& args);
    Types::TypeDecl* Type(Stack<NamedObject*>& ns,
			  const std::string& funcname,
			  const std::vector<ExprAST*>& args);

    // New interface.
    class BuiltinFunctionBase
    {
    public:
	BuiltinFunctionBase(const std::vector<ExprAST*>& a) : args(a) {}
	virtual llvm::Value* CodeGen(llvm::IRBuilder<>& builder) = 0;
	virtual Types::TypeDecl* Type() const = 0;
	virtual bool Semantics() const = 0;
	virtual ~BuiltinFunctionBase() {}
    protected:
	const std::vector<ExprAST*> args;
    };

    bool IsBuiltin(std::string funcname);
    void InitBuiltins();
    BuiltinFunctionBase* CreateBuiltinFunction(std::string name, std::vector<ExprAST*>& args);
};

#endif
