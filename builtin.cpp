#include "expr.h"
#include "builtin.h"

#include <llvm/IR/DataLayout.h>

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


static llvm::Value* CallRuntimeFPFunc(llvm::IRBuilder<>& builder, 
				      const std::string& func, 
				      const std::vector<ExprAST*>& args)
{
    llvm::Value* a = args[0]->CodeGen();
    assert(a && "Expected codegen to work for args[0]");
    std::vector<llvm::Type*> argTypes;
    llvm::Type* ty = Types::GetType(Types::Real);
    argTypes.push_back(ty);
    llvm::FunctionType* ft = llvm::FunctionType::get(ty, argTypes, false);

    llvm::Constant* f = theModule->getOrInsertFunction(func, ft);

    if (a->getType()->getTypeID() == llvm::Type::IntegerTyID)
    {
	a = builder.CreateSIToFP(a, ty, "tofp");
    }
    if (a->getType()->getTypeID() == llvm::Type::DoubleTyID)
    {
	return builder.CreateCall(f, a, "calltmp");
    }
    return ErrorV("Expected type of real or integer for this function'");
}

static llvm::Value* CallBuiltinFunc(llvm::IRBuilder<>& builder, const std::string& func, 
				    const std::vector<ExprAST*>& args)
{
    std::string name = "llvm." + func + ".f64";
    return CallRuntimeFPFunc(builder, name, args);
}

static llvm::Value* SqrtCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to 'sqrt'");
    return CallBuiltinFunc(builder, "sqrt", args);
}

static llvm::Value* SinCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to sin");
    return CallBuiltinFunc(builder, "sin", args);
}

static llvm::Value* CosCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to cos");
    return CallBuiltinFunc(builder, "cos", args);
}

static llvm::Value* LnCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to ln");
    return CallBuiltinFunc(builder, "log", args);
}

static llvm::Value* ExpCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to exp");
    return CallBuiltinFunc(builder, "exp", args);
}

static llvm::Value* ArctanCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to atan");
    return CallRuntimeFPFunc(builder, "atan", args);
}

static llvm::Value* RoundCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to round");

    return CallBuiltinFunc(builder, "round", args);
}

static llvm::Value* TruncCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to trunc");

    return CallBuiltinFunc(builder, "trunc", args);
}

static llvm::Value* RandomCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 0 && "Expect no argument to random");
    std::vector<llvm::Type*> argTypes;
    llvm::Type* ty = Types::GetType(Types::Real);
    llvm::FunctionType* ft = llvm::FunctionType::get(ty, argTypes, false);

    llvm::Constant* f = theModule->getOrInsertFunction("__random", ft);

    return builder.CreateCall(f, "calltmp");
}

static llvm::Value* ChrCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to chr");

    llvm::Value* a = args[0]->CodeGen();
    assert(a && "Expected codegen to work for args[0]");
    if (a->getType()->getTypeID() == llvm::Type::IntegerTyID)
    {
	return builder.CreateTrunc(a, Types::GetType(Types::Char), "chr");
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
	return builder.CreateZExt(a, Types::GetType(Types::Integer), "ord");
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

static llvm::Value* NewCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to 'new'");

    llvm::Value* a = args[0]->CodeGen();
    if (a->getType()->isPointerTy())
    {
	llvm::Type* ty = Types::GetType(Types::Integer);
	std::vector<llvm::Type*> argTypes;
	argTypes.push_back(ty);

	std::string name = "__new";

	// Result is "void *"
	llvm::Type* resTy = Types::GetVoidPtrType();
	llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction(name, ft);
	
	ty = a->getType()->getContainedType(0);
	const llvm::DataLayout dl(theModule);
	llvm::Value* aSize = MakeIntegerConstant(dl.getTypeAllocSize(ty));
	
	llvm::Value* retVal = builder.CreateCall(f, aSize, "new");
	
	VariableExprAST* var = dynamic_cast<VariableExprAST*>(args[0]);
	if (!var)
	{
	    return ErrorV("Expected a variable expression");
	}
	retVal = builder.CreateBitCast(retVal, a->getType(), "cast");
	llvm::Value* pA = var->Address();
	return builder.CreateStore(retVal, pA);
    }
    return ErrorV("Expected pointer argument for 'new'");
}

static llvm::Value* DisposeCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 argument to 'dispos'");

    llvm::Value* a = args[0]->CodeGen();
    if (a->getType()->isPointerTy())
    {
	llvm::Type* ty = a->getType();
	std::vector<llvm::Type*> argTypes;
	argTypes.push_back(ty);

	std::string name = "__dispose";
	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetType(Types::Void), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction(name, ft);

	return builder.CreateCall(f, a);
    }
    return ErrorV("Expected pointer argument for 'new'");
}

static llvm::Value* AssignCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 2 && "Expect 2 args for 'assign'");

    // assign takes two arguments from the user (file and filename), and a third "recordsize" 
    // that we make up here, and a fourth for the "isText" argument. 

    // Arg1: address of the filestruct.
    VariableExprAST* fvar = dynamic_cast<VariableExprAST*>(args[0]);
    if (!fvar)
    {
	return ErrorV("Expected a variable expression");
    }
    llvm::Value* faddr = fvar->Address();
    std::vector<llvm::Type*> argTypes;
    argTypes.push_back(faddr->getType());

    llvm::Value* filename = args[1]->CodeGen();
    llvm::Type* ty = filename->getType();
    if (ty->isPointerTy() && ty->getContainedType(0) != Types::GetType(Types::Char))
    {
	return ErrorV("Argument for filename should be string type.");
    }
    argTypes.push_back(ty);
    argTypes.push_back(Types::GetType(Types::Integer));
    argTypes.push_back(Types::GetType(Types::Integer));

    bool textFile;    
    int  recSize;
    if (!FileInfo(faddr, recSize, textFile))
    {
	return ErrorV("Expected first argument to be of filetype for 'assign'");	
    }
    llvm::Value* isText = MakeIntegerConstant(textFile);


    llvm::Value* aSize = MakeIntegerConstant(recSize); 

    std::vector<llvm::Value*> argsV;
    argsV.push_back(faddr);
    argsV.push_back(filename);
    argsV.push_back(aSize);
    argsV.push_back(isText);

    std::string name = "__assign";
    llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetType(Types::Void), argTypes, false);
    llvm::Constant* f = theModule->getOrInsertFunction(name, ft);

    return builder.CreateCall(f, argsV, "");
}


static llvm::Value* FileCallCodeGen(llvm::IRBuilder<>& builder, 
				    const std::vector<ExprAST*>& args, 
				    const std::string func,
				    Types::SimpleTypes resTy = Types::Void)
{
    VariableExprAST* fvar;
    if (args.size() > 0)
    {
	fvar = dynamic_cast<VariableExprAST*>(args[0]);
	if (!fvar)
	{
	    return ErrorV("Expected a variable expression");
	}
    }
    else
    {
	fvar = 0;
    }
    llvm::Value* faddr = FileOrNull(fvar);
    std::vector<llvm::Type*> argTypes;
    argTypes.push_back(faddr->getType());

    llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetType(resTy), argTypes, false);
    llvm::Constant* f = theModule->getOrInsertFunction(func, ft);

    return builder.CreateCall(f, faddr, "");
}

static llvm::Value* ResetCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 arg for 'reset'");

    return FileCallCodeGen(builder, args, "__reset");
}

static llvm::Value* CloseCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 arg for 'close'");
    return FileCallCodeGen(builder, args, "__close");
}

static llvm::Value* RewriteCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 arg for 'rewrite'");
    return FileCallCodeGen(builder, args, "__rewrite");
}

static llvm::Value* AppendCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    assert(args.size() == 1 && "Expect 1 arg for 'append'");
    return FileCallCodeGen(builder, args, "__append");
}

static llvm::Value* EofCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    return FileCallCodeGen(builder, args, "__eof", Types::Boolean);
}

static llvm::Value* EolnCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    return FileCallCodeGen(builder, args, "__eoln", Types::Boolean);
}

static llvm::Value* GetCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    return FileCallCodeGen(builder, args, "__get");
}

static llvm::Value* PutCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
{
    return FileCallCodeGen(builder, args, "__put");
}

const static BuiltinFunction bifs[] =
{
    { "abs",     AbsCodeGen     },
    { "odd",     OddCodeGen     },
    { "trunc",   TruncCodeGen   },
    { "round",   RoundCodeGen   },
    { "sqr",     SqrCodeGen     },
    { "sqrt",    SqrtCodeGen    },
    { "sin",     SinCodeGen     },
    { "cos",     CosCodeGen     },
    { "arctan",  ArctanCodeGen  },
    { "ln",      LnCodeGen      },
    { "exp",     ExpCodeGen     },
    { "chr",     ChrCodeGen     },
    { "ord",     OrdCodeGen     },
    { "succ",    SuccCodeGen    },
    { "pred",    PredCodeGen    },
    { "new",     NewCodeGen     },
    { "dispose", DisposeCodeGen },
    { "assign",  AssignCodeGen  },
    { "reset",   ResetCodeGen   },
    { "close",   CloseCodeGen   },
    { "rewrite", RewriteCodeGen },
    { "append",  AppendCodeGen  },
    { "eof",     EofCodeGen     },
    { "eoln",    EolnCodeGen    },
    { "random",  RandomCodeGen  },
    { "get",     GetCodeGen     },
    { "put",     PutCodeGen     },
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
