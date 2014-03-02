#include "expr.h"
#include "builtin.h"

typedef llvm::Value* (*CodeGenFunc)(const std::vector<ExprAST*>& expr);

struct BuiltinFunction
{
    const char *name;
    CodeGenFunc CodeGen;
};
llvm::Value* AbsCodeGen(const std::vector<ExprAST*>& expr);
llvm::Value* OddCodeGen(const std::vector<ExprAST*>& expr);

const static BuiltinFunction bifs[] =
{
    { "abs", AbsCodeGen },
    { "odd", OddCodeGen }
};

static const BuiltinFunction* find(const std::string& name)
{
    for(size_t i = 0; i < sizeof(bifs)/sizeof(bifs[0]); i++)
    {
	if (name == bifs[i].name)
	{
	    return &bifs[i];
	}
    }
    return 0;
}

bool Builtin::IsBuiltin(const std::string& name)
{
    const BuiltinFunction* b = find(name);
    return b != 0;
}


llvm::Value* Builtin::CodeGen(const std::string& name, const std::vector<ExprAST*>& args)
{
    const BuiltinFunction* b = find(name);
    assert(b && "Expected to find builtin function here!");

    return b->CodeGen(args);
}

llvm::Value* AbsCodeGen(const std::vector<ExprAST*>& args)
{
    (void) args;
    return 0;
}

llvm::Value* OddCodeGen(const std::vector<ExprAST*>& args)
{
    (void) args;
    return 0;
}


