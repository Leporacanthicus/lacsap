#ifndef BUILTIN_H
#define BUILTIN_H

#include "namedobject.h"
#include "stack.h"
#include "visitor.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <string>
#include <vector>

class ExprAST;

namespace Builtin
{
    enum class ErrorType
    {
	Ok,
	WrongArgCount,
	WrongArgType
    };

    class FunctionBase
    {
    public:
	FunctionBase(const std::string& nm, const std::vector<ExprAST*>& a) : name(nm), args(a) {}
	virtual llvm::Value*     CodeGen(llvm::IRBuilder<>& builder) = 0;
	virtual Types::TypeDecl* Type() const = 0;
	virtual ErrorType        Semantics() = 0;
	virtual void             accept(ASTVisitor& v);
	const std::string&       Name() const { return name; }
	virtual ~FunctionBase() {}

    protected:
	std::string           name;
	std::vector<ExprAST*> args;
    };

    bool          IsBuiltin(std::string funcname);
    void          InitBuiltins();
    FunctionBase* CreateBuiltinFunction(std::string name, const std::vector<ExprAST*>& args);
} // namespace Builtin

#endif
