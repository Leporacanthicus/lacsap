#ifndef BUILTIN_H
#define BUILTIN_H

#include "namedobject.h"
#include "stack.h"
#include "visitor.h"
#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <string>
#include <vector>

class ExprAST;

namespace Builtin
{
    class BuiltinFunctionBase
    {
    public:
	BuiltinFunctionBase(const std::vector<ExprAST*>& a) : args(a) {}
	virtual llvm::Value* CodeGen(llvm::IRBuilder<>& builder) = 0;
	virtual Types::TypeDecl* Type() const = 0;
	virtual bool Semantics() = 0;
	virtual void accept(ASTVisitor& v);
	virtual ~BuiltinFunctionBase() {}
    protected:
	std::vector<ExprAST*> args;
    };

    bool IsBuiltin(std::string funcname);
    void InitBuiltins();
    BuiltinFunctionBase* CreateBuiltinFunction(std::string name, std::vector<ExprAST*>& args);
} // namespace Builtin

#endif
