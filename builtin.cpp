#include "expr.h"
#include "builtin.h"

typedef llvm::Value* (*CodeGenFunc)(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& expr);

struct BuiltinFunction
{
    const char *name;
    CodeGenFunc CodeGen;
};

llvm::Value* AbsCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to abs");

    llvm::Value* a = args[0]->CodeGen();
    if (a->getType()->getTypeID() == llvm::Type::IntegerTyID)
    {
	llvm::Value* neg = builder.CreateNeg(a, "neg");
	llvm::Value* cmp = builder.CreateICmpSGE(a, MakeIntegerConstant(0), "abscond");
	llvm::Value* res = builder.CreateSelect(cmp, a, neg, "abs");
	return res;
    }
    if (a->getType()->getTypeID() == llvm::Type::DoubleTyID)
    {
	llvm::Value* neg = builder.CreateFNeg(a, "neg");
	llvm::Value* zero = llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.0));
	llvm::Value* cmp = builder.CreateFCmpOGE(a, zero, "abscond");
	llvm::Value* res = builder.CreateSelect(cmp, a, neg, "abs");
	return res;
    }
    return ErrorV("Expected type of real or integer for 'abs'");
}

llvm::Value* OddCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    llvm::Value* a = args[0]->CodeGen();
    if (a->getType()->getTypeID() == llvm::Type::IntegerTyID)
    {
	llvm::Value* res = builder.CreateAnd(a, MakeIntegerConstant(1));
	return res;
    }
    return ErrorV("Expected type of integer for 'odd'");
}

llvm::Value* TruncCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    llvm::Value* a = args[0]->CodeGen();
    if (a->getType()->getTypeID() == llvm::Type::DoubleTyID)
    {
	llvm::Value* res = builder.CreateFPToSI(a, Types::GetType("integer"));
	return res;
    }
    return ErrorV("Expected type of real for 'trunc'");
}

llvm::Value* RoundCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    llvm::Value* a = args[0]->CodeGen();
    if (a->getType()->getTypeID() == llvm::Type::DoubleTyID)
    {
	llvm::Value* zero = llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.0));
	llvm::Value* cmp = builder.CreateFCmpOGE(a, zero, "abscond");
	llvm::Value* phalf = llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.5));
	llvm::Value* mhalf = llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(-0.5));
	
	llvm::Value* half  = builder.CreateSelect(cmp, phalf, mhalf, "half");
	llvm::Value* sum = builder.CreateFAdd(a, half, "aplushalf");
	llvm::Value* res = builder.CreateFPToSI(sum, Types::GetType("integer"));
	return res;
    }
    return ErrorV("Expected type of real for 'round'");
}

llvm::Value* SqrCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to abs");

    llvm::Value* a = args[0]->CodeGen();
    if (a->getType()->getTypeID() == llvm::Type::IntegerTyID)
    {
	llvm::Value* res = builder.CreateMul(a, a, "sqr");
	return res;
    }
    if (a->getType()->getTypeID() == llvm::Type::DoubleTyID)
    {
	llvm::Value* res = builder.CreateFMul(a, a, "sqr");
	return res;
    }
    return ErrorV("Expected type of real or integer for 'sqr'");
}

const static BuiltinFunction bifs[] =
{
    { "abs",    AbsCodeGen   },
    { "odd",    OddCodeGen   },
    { "trunc",  TruncCodeGen },
    { "round",  RoundCodeGen },
    { "sqr",    SqrCodeGen   },
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

llvm::Value* Builtin::CodeGen(llvm::IRBuilder<>& builder,
			      const std::string& name, 
			      const std::vector<ExprAST*>& args)
{
    const BuiltinFunction* b = find(name);
    assert(b && "Expected to find builtin function here!");

    return b->CodeGen(builder, args);
}



