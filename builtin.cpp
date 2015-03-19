#include "expr.h"
#include "builtin.h"
#include <llvm/IR/DataLayout.h>

extern llvm::Module* theModule;

namespace Builtin
{
    static Types::TypeDecl* intTy = 0;
    static Types::TypeDecl* realTy = 0;
    static Types::TypeDecl* charTy = 0;
    static Types::TypeDecl* boolTy = 0;
    static Types::TypeDecl* longTy = 0;
    static Types::TypeDecl* strTy = 0;

    static Types::TypeDecl* IntType()
    {
	if (!intTy)
	{
	    intTy = new Types::IntegerDecl;
	}
	return intTy;
    }

    static Types::TypeDecl* RealType()
    {
	if (!realTy)
	{
	    realTy = new Types::RealDecl;
	}
	return realTy;
    }

    static Types::TypeDecl* BoolType()
    {
	if (!boolTy)
	{
	    boolTy = new Types::BoolDecl;
	}
	return boolTy;
    }

    static Types::TypeDecl* CharType()
    {
	if (!charTy)
	{
	    charTy = new Types::CharDecl;
	}
	return charTy;
    }

    static Types::TypeDecl* LongIntType()
    {
	if (!longTy)
	{
	    longTy = new Types::Int64Decl;
	}
	return longTy;
    }

    static Types::TypeDecl* StringType()
    {
	if (!strTy)
	{
	    strTy = new Types::StringDecl(255);
	}
	return strTy;
    }

    typedef BuiltinFunctionBase* (*CreateBIFObject)(const std::vector<ExprAST*>& a);
    std::map<std::string, CreateBIFObject> BIFMap;

    /* TODO: Remove this Old style functionality */
    typedef llvm::Value* (*CodeGenFunc)(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& expr);

    static llvm::Value* CallRuntimeFPFunc(llvm::IRBuilder<>& builder,
					  const std::string& func,
					  const std::vector<ExprAST*>& args,
					  llvm::Type* resTy = 0)
    {
	llvm::Value* a = args[0]->CodeGen();
	assert(a && "Expected codegen to work for args[0]");
	llvm::Type* ty = Types::GetType(Types::Real);
	if (!resTy)
	{
	    resTy = ty;
	}
	std::vector<llvm::Type*> argTypes = {ty};
	llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);

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

    class BuiltinFunctionSameAsArg : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionSameAsArg(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	Types::TypeDecl* Type() const override { return args[0]->Type(); }
    };

    class BuiltinFunctionInt : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionInt(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	Types::TypeDecl* Type() const override { return IntType(); }
    };

    class BuiltinFunctionAbs : public BuiltinFunctionSameAsArg
    {
    public:
	BuiltinFunctionAbs(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionSameAsArg(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionSqr : public BuiltinFunctionSameAsArg
    {
    public:
	BuiltinFunctionSqr(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionSameAsArg(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionOdd : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionOdd(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return BoolType(); }
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionRound : public BuiltinFunctionInt
    {
    public:
	BuiltinFunctionRound(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionInt(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionTrunc : public BuiltinFunctionInt
    {
    public:
	BuiltinFunctionTrunc(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionInt(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionRandom : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionRandom(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return RealType(); }
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionChr : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionChr(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return CharType(); }
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionOrd : public BuiltinFunctionInt
    {
    public:
	BuiltinFunctionOrd(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionInt(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionLength : public BuiltinFunctionInt
    {
    public:
	BuiltinFunctionLength(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionInt(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionPopcnt : public BuiltinFunctionInt
    {
    public:
	BuiltinFunctionPopcnt(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionInt(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionSucc : public BuiltinFunctionSameAsArg
    {
    public:
	BuiltinFunctionSucc(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionSameAsArg(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionPred : public BuiltinFunctionSameAsArg
    {
    public:
	BuiltinFunctionPred(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionSameAsArg(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionFloat : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionFloat(const std::string& fn, const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a), funcname(fn) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return RealType(); }
	virtual bool Semantics() const override { return false; }
    protected:
	std::string funcname;
    };

    class BuiltinFunctionFloat2Arg : public BuiltinFunctionFloat
    {
    public:
	BuiltinFunctionFloat2Arg(const std::string& fn, const std::vector<ExprAST*>& a)
	    : BuiltinFunctionFloat(fn, a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return RealType(); }
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionFloatIntrinsic : public BuiltinFunctionFloat
    {
    public:
	BuiltinFunctionFloatIntrinsic(const std::string& fn, const std::vector<ExprAST*>& a)
	    : BuiltinFunctionFloat("llvm." + fn + ".f64", a) {}
    };

    class BuiltinFunctionVoid : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionVoid(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	Types::TypeDecl* Type() const override { return Types::GetVoidType(); }
    };

    class BuiltinFunctionNew : public BuiltinFunctionVoid
    {
    public:
	BuiltinFunctionNew(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionVoid(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionDispose : public BuiltinFunctionVoid
    {
    public:
	BuiltinFunctionDispose(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionVoid(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionFile : public BuiltinFunctionVoid
    {
    public:
	BuiltinFunctionFile(const std::string& fn, const std::vector<ExprAST*>& a)
	    : BuiltinFunctionVoid(a), funcname(fn) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    protected:
	std::string funcname;
    };

    class BuiltinFunctionFileBool : public BuiltinFunctionFile
    {
    public:
	BuiltinFunctionFileBool(const std::string& fn, const std::vector<ExprAST*>& a)
	    : BuiltinFunctionFile(fn, a) {}
	Types::TypeDecl* Type() const override { return BoolType(); }
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionAssign : public BuiltinFunctionVoid
    {
    public:
	BuiltinFunctionAssign(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionVoid(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionPanic : public BuiltinFunctionVoid
    {
    public:
	BuiltinFunctionPanic(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionVoid(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };


    class BuiltinFunctionLongInt : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionLongInt(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	Types::TypeDecl* Type() const override { return LongIntType(); }
    };

 
    class BuiltinFunctionClock : public BuiltinFunctionLongInt
    {
    public:
	BuiltinFunctionClock(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionLongInt(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionCycles : public BuiltinFunctionLongInt
    {
    public:
	BuiltinFunctionCycles(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionLongInt(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionParamcount : public BuiltinFunctionInt
    {
    public:
	BuiltinFunctionParamcount(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionInt(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionParamstr : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionParamstr(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return StringType(); }
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionCopy : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionCopy(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return StringType(); }
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionMin : public BuiltinFunctionSameAsArg
    {
    public:
	BuiltinFunctionMin(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionSameAsArg(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionMax : public BuiltinFunctionSameAsArg
    {
    public:
	BuiltinFunctionMax(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionSameAsArg(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    class BuiltinFunctionSign : public BuiltinFunctionSameAsArg
    {
    public:
	BuiltinFunctionSign(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionSameAsArg(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() const override { return false; }
    };

    void BuiltinFunctionBase::accept(Visitor& v)
    {
	for(auto a : args)
	{
	    a->accept(v);
	}
    }

    llvm::Value* BuiltinFunctionAbs::CodeGen(llvm::IRBuilder<>& builder)
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
	    return CallRuntimeFPFunc(builder, "llvm.fabs.f64", args);
	}
	return ErrorV("Expected type of real or integer for 'abs'");
    }

    llvm::Value* BuiltinFunctionOdd::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	if (a->getType()->getTypeID() == llvm::Type::IntegerTyID)
	{
	    llvm::Value* tmp = builder.CreateAnd(a, MakeIntegerConstant(1));
	    return builder.CreateBitCast(tmp, Types::GetType(Types::Boolean));
	}
	return ErrorV("Expected type of integer for 'odd'");
    }

    llvm::Value* BuiltinFunctionSqr::CodeGen(llvm::IRBuilder<>& builder)
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

    llvm::Value* BuiltinFunctionFloat::CodeGen(llvm::IRBuilder<>& builder)
    {
	assert(args.size() == 1 && "Expect 1 argument to 'sqrt'");
	return CallRuntimeFPFunc(builder, funcname, args);
    }

    llvm::Value* BuiltinFunctionRound::CodeGen(llvm::IRBuilder<>& builder)
    {
	assert(args.size() == 1 && "Expect 1 argument to round");

	llvm::Value* v = CallRuntimeFPFunc(builder, "llvm.round.f64", args);
	return builder.CreateFPToSI(v, Types::GetType(Types::Integer), "to.int");
    }

    llvm::Value* BuiltinFunctionTrunc::CodeGen(llvm::IRBuilder<>& builder)
    {
	assert(args.size() == 1 && "Expect 1 argument to trunc");
	
	llvm::Value* v = args[0]->CodeGen();
	return builder.CreateFPToSI(v, Types::GetType(Types::Integer), "to.int");
    }

    llvm::Value* BuiltinFunctionRandom::CodeGen(llvm::IRBuilder<>& builder)
    {
	assert(args.size() == 0 && "Expect 0 arguments to rand");
	std::vector<llvm::Type*> argTypes;
	llvm::Type* ty = Types::GetType(Types::Real);
	llvm::FunctionType* ft = llvm::FunctionType::get(ty, argTypes, false);

	llvm::Constant* f = theModule->getOrInsertFunction("__random", ft);

	return builder.CreateCall(f, "calltmp");
    }

    llvm::Value* BuiltinFunctionChr::CodeGen(llvm::IRBuilder<>& builder)
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

    llvm::Value* BuiltinFunctionOrd::CodeGen(llvm::IRBuilder<>& builder)
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

    llvm::Value* BuiltinFunctionSucc::CodeGen(llvm::IRBuilder<>& builder)
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

    llvm::Value* BuiltinFunctionPred::CodeGen(llvm::IRBuilder<>& builder)
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

    llvm::Value* BuiltinFunctionNew::CodeGen(llvm::IRBuilder<>& builder)
    {
	assert(args.size() == 1 && "Expect 1 argument to 'new'");

	// TODO: Move to semantics.
	Types::PointerDecl* pd = llvm::dyn_cast<Types::PointerDecl>(args[0]->Type());
	if (pd)
	{
	    size_t size = pd->SubType()->Size();
	    llvm::Type* ty = Types::GetType(Types::Integer);
	    std::vector<llvm::Type*> argTypes{ty};

	    // Result is "void *"
	    llvm::Type* resTy = Types::GetVoidPtrType();
	    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
	    llvm::Constant* f = theModule->getOrInsertFunction("__new", ft);
	
	    llvm::Value* aSize = MakeIntegerConstant(size);
	
	    llvm::Value* retVal = builder.CreateCall(f, aSize, "new");
	
	    VariableExprAST* var = llvm::dyn_cast<VariableExprAST>(args[0]);
	    if (!var)
	    {
		return ErrorV("Expected a variable expression");
	    }
	    retVal = builder.CreateBitCast(retVal, pd->LlvmType(), "cast");
	    llvm::Value* pA = var->Address();
	    return builder.CreateStore(retVal, pA);
	}
	return ErrorV("Expected pointer argument for 'new'");
    }

    llvm::Value* BuiltinFunctionDispose::CodeGen(llvm::IRBuilder<>& builder)
    {
	assert(args.size() == 1 && "Expect 1 argument to 'dispose'");

	llvm::Value* a = args[0]->CodeGen();
	if (a->getType()->isPointerTy())
	{
	    llvm::Type* ty = a->getType();
	    std::vector<llvm::Type*> argTypes{ty};

	    std::string name = "__dispose";
	    llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetType(Types::Void), argTypes, false);
	    llvm::Constant* f = theModule->getOrInsertFunction(name, ft);

	    return builder.CreateCall(f, a);
	}
	return ErrorV("Expected pointer argument for 'new'");
    }

    llvm::Value* BuiltinFunctionFile::CodeGen(llvm::IRBuilder<>& builder)
    {
	VariableExprAST* fvar;
	if (args.size() > 0)
	{
	    fvar = llvm::dyn_cast<VariableExprAST>(args[0]);
	    if (!fvar)
	    {
		return ErrorV("Expected a variable expression");
	    }
	}
	else
	{
	    fvar = new VariableExprAST(Location("",0,0), "input", Types::GetTextType());
	}
	llvm::Value* faddr = fvar->Address();
	std::vector<llvm::Type*> argTypes{faddr->getType()};

	llvm::FunctionType* ft = llvm::FunctionType::get(Type()->LlvmType(), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction(std::string("__") + funcname, ft);

	return builder.CreateCall(f, faddr, "");
    }

    llvm::Value* BuiltinFunctionLength::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (args[0]->Type()->Type() != Types::String)
	{
	    return ErrorV("Incorrect argument type - needs to be a string");
	}
	llvm::Value* v = MakeAddressable(args[0]);
	std::vector<llvm::Value*> ind{MakeIntegerConstant(0), MakeIntegerConstant(0)};
	llvm::Value* v1 = builder.CreateGEP(v, ind, "str_0");
	llvm::Value* v2 = builder.CreateLoad(v1, "len");

	return builder.CreateZExt(v2, Types::GetType(Types::Integer), "extend");
    }

    llvm::Value* BuiltinFunctionAssign::CodeGen(llvm::IRBuilder<>& builder)
    {
	assert(args.size() == 2 && "Expect 2 args for 'assign'");

	// assign takes two arguments from the user (file and filename), and a third "recordsize"
	// that we make up here, and a fourth for the "isText" argument.

	// Arg1: address of the filestruct.
	VariableExprAST* fvar = llvm::dyn_cast<VariableExprAST>(args[0]);
	if (!fvar)
	{
	    return ErrorV("Expected a variable expression");
	}
	llvm::Value* faddr = fvar->Address();

	llvm::Value* filename = args[1]->CodeGen();
	llvm::Type* ty = filename->getType();
	if (ty->isPointerTy() && ty->getContainedType(0) != Types::GetType(Types::Char))
	{
	    return ErrorV("Argument for filename should be string type.");
	}
	llvm::Type* intTy = Types::GetType(Types::Integer);
	std::vector<llvm::Type*> argTypes{faddr->getType(), ty, intTy, intTy};

	bool textFile;
	int  recSize;
	if (!FileInfo(faddr, recSize, textFile))
	{
	    return ErrorV("Expected first argument to be of filetype for 'assign'");
	}
	llvm::Value* isText = MakeIntegerConstant(textFile);
	llvm::Value* aSize = MakeIntegerConstant(recSize); 

	std::vector<llvm::Value*> argsV{faddr, filename, aSize, isText};

	std::string name = "__assign";
	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetType(Types::Void), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction(name, ft);

	return builder.CreateCall(f, argsV, "");
    }

    llvm::Value* BuiltinFunctionCopy::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (args.size() != 3)
	{
	    return ErrorV("Copy takes three arguments");
	}
	if (args[0]->Type()->Type() != Types::String ||
	    args[1]->Type()->Type() != Types::Integer ||
	    args[2]->Type()->Type() != Types::Integer)
	{
	    return ErrorV("Arguments to copy should be (string, integer, integer)");
	}
	llvm::Value* str = MakeAddressable(args[0]);
	assert(str && "Expect non-NULL return here...");

	llvm::Value* start = args[1]->CodeGen();
	llvm::Value* len   = args[2]->CodeGen();

	std::vector<llvm::Type*> argTypes{str->getType(), start->getType(), len->getType()};

	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetStringType()->LlvmType(), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("__StrCopy", ft);

	return builder.CreateCall3(f, str, start, len, "copy");
    }

    llvm::Value* BuiltinFunctionClock::CodeGen(llvm::IRBuilder<>& builder)
    {
	std::vector<llvm::Type*> argTypes;

	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetType(Types::Int64), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("__Clock", ft);

	return builder.CreateCall(f, "clock");
    }

    llvm::Value* BuiltinFunctionPanic::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* message = args[0]->CodeGen();
	llvm::Type* ty = message->getType();
	if (!ty->isPointerTy() || ty->getContainedType(0) != Types::GetType(Types::Char))
	{
	    return ErrorV("Argument for panic message should be string type.");
	}
	std::vector<llvm::Type*> argTypes{ty};
	std::vector<llvm::Value*> argsV{message};

	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetType(Types::Void), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("__Panic", ft);

	return builder.CreateCall(f,  argsV);
    }

    llvm::Value* BuiltinFunctionPopcnt::CodeGen(llvm::IRBuilder<>& builder)
    {
	Types::TypeDecl* type = args[0]->Type();
	std::string name = "llvm.ctpop.i";
	if (type->isIntegral())
	{
	    name += std::to_string(type->Size() * 8);
	    llvm::Type* ty = type->LlvmType();
	    std::vector<llvm::Type*> argTypes{ty};
	    llvm::FunctionType* ft = llvm::FunctionType::get(ty, argTypes, false);

	    llvm::Constant* f = theModule->getOrInsertFunction(name, ft);
	    llvm::Value* a = args[0]->CodeGen();
	    return builder.CreateCall(f, a, "popcnt");
	}
	if (Types::SetDecl* sd = llvm::dyn_cast<Types::SetDecl>(type))
	{
	    name += std::to_string(Types::SetDecl::SetBits);
	    llvm::Value *v = MakeAddressable(args[0]);
	    std::vector<llvm::Value*> ind{MakeIntegerConstant(0), MakeIntegerConstant(0)};
	    llvm::Value *addr = builder.CreateGEP(v, ind, "leftSet");
	    llvm::Value *val = builder.CreateLoad(addr);
	    llvm::Type* ty = val->getType();
	    std::vector<llvm::Type*> argTypes{ty};
	    llvm::FunctionType* ft = llvm::FunctionType::get(ty, argTypes, false);
	    llvm::Constant* f = theModule->getOrInsertFunction(name, ft);
	    llvm::Value *count = builder.CreateCall(f, val, "count");
	    for(size_t i = 1; i < sd->SetWords(); i++)
	    {
		std::vector<llvm::Value*> ind{MakeIntegerConstant(0), MakeIntegerConstant(i)};
		addr = builder.CreateGEP(v, ind, "leftSet");
		val = builder.CreateLoad(addr);
		llvm::Value *tmp = builder.CreateCall(f, val, "tmp");
		count = builder.CreateAdd(count, tmp, "count");
	    }
	    return count;
	}
	return ErrorV("Incorrect argument type - needs to be a integer or set");
    }

    llvm::Value* BuiltinFunctionCycles::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (args.size())
	{
	    return ErrorV("Cycles function takes no arguments");
	}
	std::vector<llvm::Type*> argTypes;

	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetType(Types::Int64), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("llvm.readcyclecounter", ft);

	return builder.CreateCall(f, "cycles");
    }

    llvm::Value* BuiltinFunctionFloat2Arg::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (args.size() != 2)
	{
	    return ErrorV("Cycles function takes no arguments");
	}

	llvm::Type* realTy = Types::GetType(Types::Real);
	llvm::Value* a = args[0]->CodeGen();
	llvm::Value* b = args[1]->CodeGen();

	if (a->getType()->getTypeID() == llvm::Type::IntegerTyID)
	{
	    a = builder.CreateSIToFP(a, realTy, "tofp");
	}
	if (b->getType()->getTypeID() == llvm::Type::IntegerTyID)
	{
	    b = builder.CreateSIToFP(b, realTy, "tofp");
	}

	std::vector<llvm::Value*> arg = {a, b};
	
	std::vector<llvm::Type*> argTypes = {realTy, realTy};

	llvm::FunctionType* ft = llvm::FunctionType::get(realTy, argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction(funcname, ft);
	
	return builder.CreateCall(f, arg);
    }

    llvm::Value* BuiltinFunctionParamstr::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (args.size() != 1)
	{
	    return ErrorV("ParamStr takes one argument");
	}
	if (args[0]->Type()->Type() != Types::Integer)
	{
	    return ErrorV("Arguments to paramstr should be (integer)");
	}
	llvm::Value* n   = args[0]->CodeGen();

	std::vector<llvm::Type*> argTypes{n->getType()};

	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetStringType()->LlvmType(), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("__ParamStr", ft);

	return builder.CreateCall(f, n, "paramstr");
    }

    llvm::Value* BuiltinFunctionParamcount::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (args.size() != 0)
	{
	    return ErrorV("ParamCount takes no arguments");
	}

	std::vector<llvm::Type*> argTypes{};

	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetType(Types::Integer), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("__ParamCount", ft);

	return builder.CreateCall(f, "paramcount");
    }

    llvm::Value* BuiltinFunctionMax::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (args.size() != 2)
	{
	    return ErrorV("'max' takes two arguments");
	}
	if (args[0]->Type()->Type() == Types::Real ||
	    args[1]->Type()->Type() == Types::Real)
	{
	    BuiltinFunctionFloat2Arg max("llvm.maxnum.f64", args);
	    return max.CodeGen(builder);
	}
	if (!args[0]->Type()->isIntegral() || !args[1]->Type()->isIntegral())
	{
	    return ErrorV("'max' takes numeric arguments");
	}
	    
	llvm::Value* a = args[0]->CodeGen();
	llvm::Value* b = args[1]->CodeGen();
	llvm::Value* sel;
	if (args[0]->Type()->isUnsigned() || args[1]->Type()->isUnsigned())
	{
	    sel = builder.CreateICmpUGT(a, b, "sel");
	}
	else
	{
	    sel = builder.CreateICmpSGT(a, b, "sel");
	}
	return builder.CreateSelect(sel, a, b, "max");
    }

    llvm::Value* BuiltinFunctionMin::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (args.size() != 2)
	{
	    return ErrorV("'min' takes two arguments");
	}
	if (args[0]->Type()->Type() == Types::Real ||
	    args[1]->Type()->Type() == Types::Real)
	{
	    BuiltinFunctionFloat2Arg min("llvm.minnum.f64", args);
	    return min.CodeGen(builder);
	}
	if (!args[0]->Type()->isIntegral() || !args[1]->Type()->isIntegral())
	{
	    return ErrorV("'min' takes numeric arguments");
	}
	llvm::Value* a = args[0]->CodeGen();
	llvm::Value* b = args[1]->CodeGen();
	llvm::Value* sel;
	if (args[0]->Type()->isUnsigned() || args[1]->Type()->isUnsigned())
	{
	    sel = builder.CreateICmpULT(a, b, "sel");
	}
	else
	{
	    sel = builder.CreateICmpSLT(a, b, "sel");
	}
	return builder.CreateSelect(sel, a, b, "min");
    }

    llvm::Value* BuiltinFunctionSign::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (args.size() != 1)
	{
	    return ErrorV("'sign' takes one argument");
	}
	llvm::Value* v = args[0]->CodeGen();
	if (args[0]->Type()->Type() == Types::Real)
	{
	    llvm::Value* zero = llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.0));
	    llvm::Value* one = llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(1.0));
	    llvm::Value* mone = llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(-1.0));
	    llvm::Value* sel1 = builder.CreateFCmpOGT(v, zero, "gt");
	    llvm::Value* sel2 = builder.CreateFCmpOLT(v, zero, "lt");
	    llvm::Value* res = builder.CreateSelect(sel1, one, zero, "sgn1");
	    return builder.CreateSelect(sel2, mone, res, "sgn2");
	}
	else if (args[0]->Type()->isUnsigned())
	{
	    llvm::Value* zero = MakeIntegerConstant(0);
	    llvm::Value* one = MakeIntegerConstant(1);
	    llvm::Value* sel1 = builder.CreateICmpUGT(v, zero, "gt");
	    return builder.CreateSelect(sel1, one, zero, "sgn1");
	} 
	else if (args[0]->Type()->isIntegral())
	{
	    llvm::Value* zero = MakeIntegerConstant(0);
	    llvm::Value* one = MakeIntegerConstant(1);
	    llvm::Value* mone = MakeIntegerConstant(-1);
	    llvm::Value* sel1 = builder.CreateICmpSGT(v, zero, "gt");
	    llvm::Value* sel2 = builder.CreateICmpSLT(v, zero, "lt");
	    llvm::Value* res = builder.CreateSelect(sel1, one, zero, "sgn1");
	    return builder.CreateSelect(sel2, mone, res, "sgn2");
	}
	return ErrorV("Invalid argument type for 'sign'");
    }

    /* New style interface */ 
    BuiltinFunctionBase* CreateAbs(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionAbs(args);
    }

    BuiltinFunctionBase* CreateOdd(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionOdd(args);
    }

    BuiltinFunctionBase* CreateSqr(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionSqr(args);
    }

    BuiltinFunctionBase* CreateSqrt(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFloatIntrinsic("sqrt", args);
    }

    BuiltinFunctionBase* CreateSin(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFloatIntrinsic("sin", args);
    }

    BuiltinFunctionBase* CreateCos(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFloatIntrinsic("cos", args);
    }

    BuiltinFunctionBase* CreateTan(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFloat("tan", args);
    }

    BuiltinFunctionBase* CreateArctan(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFloat("atan", args);
    }

    BuiltinFunctionBase* CreateArctan2(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFloat("atan2", args);
    }

    BuiltinFunctionBase* CreateFmod(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFloat("fmod", args);
    }

    BuiltinFunctionBase* CreateLn(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFloatIntrinsic("log", args);
    }

    BuiltinFunctionBase* CreateExp(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFloatIntrinsic("exp", args);
    }

    BuiltinFunctionBase* CreateRound(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionRound(args);
    }

    BuiltinFunctionBase* CreateTrunc(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionTrunc(args);
    }

    BuiltinFunctionBase* CreateRandom(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionRandom(args);
    }

    BuiltinFunctionBase* CreateChr(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionChr(args);
    }

    BuiltinFunctionBase* CreateOrd(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionOrd(args);
    }

    BuiltinFunctionBase* CreateSucc(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionSucc(args);
    }

    BuiltinFunctionBase* CreatePred(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionPred(args);
    }

    BuiltinFunctionBase* CreateNew(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionNew(args);
    }

    BuiltinFunctionBase* CreateDispose(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionDispose(args);
    }

    BuiltinFunctionBase* CreateAppend(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFile("append", args);
    }

    BuiltinFunctionBase* CreateReset(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFile("reset", args);
    }

    BuiltinFunctionBase* CreateRewrite(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFile("rewrite", args);
    }

    BuiltinFunctionBase* CreateClose(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFile("close", args);
    }

    BuiltinFunctionBase* CreatePut(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFile("put", args);
    }

    BuiltinFunctionBase* CreateGet(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFile("get", args);
    }

    BuiltinFunctionBase* CreateEof(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFileBool("eof", args);
    }

    BuiltinFunctionBase* CreateEoln(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionFileBool("eoln", args);
    }

    BuiltinFunctionBase* CreateLength(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionLength(args);
    }

    BuiltinFunctionBase* CreatePopcnt(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionPopcnt(args);
    }

    BuiltinFunctionBase* CreateAssign(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionAssign(args);
    }

    BuiltinFunctionBase* CreatePanic(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionPanic(args);
    }

    BuiltinFunctionBase* CreateClock(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionClock(args);
    }

    BuiltinFunctionBase* CreateCycles(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionCycles(args);
    }

    BuiltinFunctionBase* CreateParamcount(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionParamcount(args);
    }

    BuiltinFunctionBase* CreateParamstr(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionParamstr(args);
    }

    BuiltinFunctionBase* CreateCopy(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionCopy(args);
    }

    BuiltinFunctionBase* CreateMax(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionMax(args);
    }

    BuiltinFunctionBase* CreateMin(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionMin(args);
    }

    BuiltinFunctionBase* CreateSign(const std::vector<ExprAST*>& args)
    {
	return new BuiltinFunctionSign(args);
    }

    void AddBIFCreator(const std::string& name, CreateBIFObject createFunc)
    {
	assert(BIFMap.find(name) == BIFMap.end() && "Already registered function");
	BIFMap[name] = createFunc;
    }

    bool IsBuiltin(std::string name)
    {
	std::transform(name.begin(), name.end(), name.begin(), ::tolower);
	return BIFMap.find(name) != BIFMap.end();
    }

    BuiltinFunctionBase* CreateBuiltinFunction(std::string name, std::vector<ExprAST*>& args)
    {
	std::transform(name.begin(), name.end(), name.begin(), ::tolower);
	auto it = BIFMap.find(name);
	if (it != BIFMap.end())
	{
	    return it->second(args);
	}
	return 0;
    }

    void InitBuiltins()
    {
	AddBIFCreator("abs",        CreateAbs);
	AddBIFCreator("odd",        CreateOdd);
	AddBIFCreator("sqr",        CreateSqr);
	AddBIFCreator("sqrt",       CreateSqrt);
	AddBIFCreator("sin",        CreateSin);
	AddBIFCreator("cos",        CreateCos);
	AddBIFCreator("tan",        CreateTan);
	AddBIFCreator("ln",         CreateLn);
	AddBIFCreator("exp",        CreateExp);
	AddBIFCreator("arctan",     CreateArctan);
	AddBIFCreator("round",      CreateRound);
	AddBIFCreator("trunc",      CreateTrunc);
	AddBIFCreator("random",     CreateRandom);
	AddBIFCreator("chr",        CreateChr);
	AddBIFCreator("ord",        CreateOrd);
	AddBIFCreator("succ",       CreateSucc);
	AddBIFCreator("pred",       CreatePred);
	AddBIFCreator("new",        CreateNew);
	AddBIFCreator("dispose",    CreateDispose);
	AddBIFCreator("reset",      CreateReset);
	AddBIFCreator("rewrite",    CreateRewrite);
	AddBIFCreator("append",     CreateAppend);
	AddBIFCreator("close",      CreateClose);
	AddBIFCreator("get",        CreateGet);
	AddBIFCreator("put",        CreatePut);
	AddBIFCreator("eof",        CreateEof);
	AddBIFCreator("eoln",       CreateEoln);
	AddBIFCreator("length",     CreateLength);
	AddBIFCreator("arctan2",    CreateArctan2);
	AddBIFCreator("fmod",       CreateFmod);
	AddBIFCreator("popcnt",     CreatePopcnt);
	AddBIFCreator("assign",     CreateAssign);
	AddBIFCreator("panic",      CreatePanic);
	AddBIFCreator("clock",      CreateClock);
	AddBIFCreator("cycles",     CreateCycles);
	AddBIFCreator("paramcount", CreateParamcount);
	AddBIFCreator("paramstr",   CreateParamstr);
	AddBIFCreator("copy",       CreateCopy);
	AddBIFCreator("max",        CreateMax);
	AddBIFCreator("min",        CreateMin);
	AddBIFCreator("sign",       CreateSign);
    }
} // namespace Builtin
