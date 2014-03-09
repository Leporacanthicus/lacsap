#include "expr.h"
#include "builtin.h"

typedef llvm::Value* (*CodeGenFunc)(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& expr);

struct BuiltinFunction
{
    const char *name;
    CodeGenFunc CodeGen;
};

static llvm::Value* AbsCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to abs");

    llvm::Value* a = args[0]->CodeGen();
    assert(a && "Expected codegen to work for args[0]");
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

static llvm::Value* OddCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    llvm::Value* a = args[0]->CodeGen();
    if (a->getType()->getTypeID() == llvm::Type::IntegerTyID)
    {
	llvm::Value* tmp = builder.CreateAnd(a, MakeIntegerConstant(1));
	return builder.CreateBitCast(tmp, Types::GetType(Types::Boolean));
    }
    return ErrorV("Expected type of integer for 'odd'");
}

static llvm::Value* TruncCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to trunc");
    llvm::Value* a = args[0]->CodeGen();
    assert(a && "Expected codegen to work for args[0]");
    if (a->getType()->getTypeID() == llvm::Type::DoubleTyID)
    {
	llvm::Value* res = builder.CreateFPToSI(a, Types::GetType(Types::Integer));
	return res;
    }
    return ErrorV("Expected type of real for 'trunc'");
}

static llvm::Value* RoundCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to round");
    llvm::Value* a = args[0]->CodeGen();
    assert(a && "Expected codegen to work for args[0]");
    if (a->getType()->getTypeID() == llvm::Type::DoubleTyID)
    {
	llvm::Value* zero = llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.0));
	llvm::Value* cmp = builder.CreateFCmpOGE(a, zero, "rndcond");
	llvm::Value* phalf = llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.5));
	llvm::Value* mhalf = llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(-0.5));
	
	llvm::Value* half  = builder.CreateSelect(cmp, phalf, mhalf, "half");
	llvm::Value* sum = builder.CreateFAdd(a, half, "aplushalf");
	llvm::Value* res = builder.CreateFPToSI(sum, Types::GetType(Types::Integer));
	return res;
    }
    return ErrorV("Expected type of real for 'round'");
}

static llvm::Value* SqrCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to sqr");

    llvm::Value* a = args[0]->CodeGen();
    assert(a && "Expected codegen to work for args[0]");
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


static llvm::Value* CallBuiltinFunc(llvm::IRBuilder<>& builder, const std::string& func, 
				    const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to sqrt");

    llvm::Value* a = args[0]->CodeGen();
    assert(a && "Expected codegen to work for args[0]");
    std::vector<llvm::Type*> argTypes;
    llvm::Type* ty = Types::GetType(Types::Real);
    argTypes.push_back(ty);
    std::string name = "llvm." + func + ".f64";
    llvm::FunctionType* ft = llvm::FunctionType::get(ty, argTypes, false);
    llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, 
					       name, theModule);
    if (f->getName() != name)
    {
	f->eraseFromParent();
	f = theModule->getFunction(name);
    }

    if (a->getType()->getTypeID() == llvm::Type::IntegerTyID)
    {
	a = builder.CreateSIToFP(a, Types::GetType(Types::Real), "sqrttofp");
    }
    if (a->getType()->getTypeID() == llvm::Type::DoubleTyID)
    {
	std::vector<llvm::Value*> argsV;
	argsV.push_back(a);
	return builder.CreateCall(f, argsV, "calltmp");
    }
    return ErrorV("Expected type of real or integer for 'sqr'");
}

static llvm::Value* SqrtCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    return CallBuiltinFunc(builder, "sqrt", args);
}

static llvm::Value* SinCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    return CallBuiltinFunc(builder, "sin", args);
}

static llvm::Value* CosCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    return CallBuiltinFunc(builder, "cos", args);
}

static llvm::Value* ArctanCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    (void)builder;
    (void)args;
//    return CallBuiltinFunc(builder, "arctan", args);
    assert(0 && "arctan not supported right now");
}

static llvm::Value* LnCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    return CallBuiltinFunc(builder, "log", args);
}

static llvm::Value* ExpCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    return CallBuiltinFunc(builder, "exp", args);
}

static llvm::Value* ChrCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to chr");

    llvm::Value* a = args[0]->CodeGen();
    assert(a && "Expected codegen to work for args[0]");
    if (a->getType()->getTypeID() == llvm::Type::IntegerTyID)
    {
	return builder.CreateBitCast(a, Types::GetType(Types::Char), "chr");
    }
    return ErrorV("Expected integer type for chr function");
}

static llvm::Value* OrdCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to ord");

    llvm::Value* a = args[0]->CodeGen();
    assert(a && "Expected codegen to work for args[0]");
    if (a->getType()->getTypeID() == llvm::Type::IntegerTyID)
    {
	return builder.CreateBitCast(a, Types::GetType(Types::Integer), "ord");
    }
    return ErrorV("Expected integer type for ord function");
}

static llvm::Value* SuccCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to succ");

    llvm::Value* a = args[0]->CodeGen();
    assert(a && "Expected codegen to work for args[0]");
    if (a->getType()->getTypeID() == llvm::Type::IntegerTyID)
    {
	return builder.CreateAdd(a, MakeConstant(1, a->getType()), "succ");
    }
    return ErrorV("Expected integer type for succ function");
}


static llvm::Value* PredCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to pred");

    llvm::Value* a = args[0]->CodeGen();
    assert(a && "Expected codegen to work for args[0]");
    if (a->getType()->getTypeID() == llvm::Type::IntegerTyID)
    {
	return builder.CreateSub(a, MakeConstant(1, a->getType()), "pred");
    }
    return ErrorV("Expected integer type for pred function");
}


const static BuiltinFunction bifs[] =
{
    { "abs",    AbsCodeGen    },
    { "odd",    OddCodeGen    },
    { "trunc",  TruncCodeGen  },
    { "round",  RoundCodeGen  },
    { "sqr",    SqrCodeGen    },
    { "sqrt",   SqrtCodeGen   },
    { "sin",    SinCodeGen    },
    { "cos",    CosCodeGen    },
    { "arctan", ArctanCodeGen },
    { "ln",     LnCodeGen     },
    { "exp",    ExpCodeGen    },
    { "chr",    ChrCodeGen    },
    { "ord",    OrdCodeGen    },
    { "succ",   SuccCodeGen   },
    { "pred",   PredCodeGen   },
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
