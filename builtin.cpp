#include "expr.h"
#include "builtin.h"
#include <llvm/IR/DataLayout.h>
#include <functional>

extern llvm::Module* theModule;

namespace Builtin
{
    typedef const std::vector<ExprAST*> ArgList;


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

    typedef std::function<BuiltinFunctionBase*(const std::vector<ExprAST*>&)>  CreateBIFObject;
    std::map<std::string, CreateBIFObject> BIFMap;

    /* TODO: Remove this Old style functionality */
    typedef llvm::Value* (*CodeGenFunc)(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& expr);

    static llvm::Value* CallRuntimeFPFunc(llvm::IRBuilder<>& builder,
					  const std::string& func,
					  const std::vector<ExprAST*>& args)
    {
	llvm::Value* a = args[0]->CodeGen();
	llvm::Type* ty = Types::GetType(Types::Real);
	std::vector<llvm::Type*> argTypes = {ty};
	llvm::FunctionType* ft = llvm::FunctionType::get(ty, argTypes, false);

	llvm::Constant* f = theModule->getOrInsertFunction(func, ft);
	return builder.CreateCall(f, a, "calltmp");
    }

    class BuiltinFunctionSameAsArg : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionSameAsArg(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	Types::TypeDecl* Type() const override { return args[0]->Type(); }
	virtual bool Semantics() override;
    };

    class BuiltinFunctionSameAsArg2 : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionSameAsArg2(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	Types::TypeDecl* Type() const override { return args[0]->Type(); }
	virtual bool Semantics() override;
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
    };

    class BuiltinFunctionSqr : public BuiltinFunctionSameAsArg
    {
    public:
	BuiltinFunctionSqr(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionSameAsArg(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() override;
    };

    class BuiltinFunctionOdd : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionOdd(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return BoolType(); }
	virtual bool Semantics() override;
    };

    class BuiltinFunctionRound : public BuiltinFunctionInt
    {
    public:
	BuiltinFunctionRound(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionInt(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() override;
    };

    class BuiltinFunctionTrunc : public BuiltinFunctionRound
    {
    public:
	BuiltinFunctionTrunc(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionRound(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class BuiltinFunctionRandom : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionRandom(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return RealType(); }
	virtual bool Semantics() override;
    };

    class BuiltinFunctionChr : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionChr(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return CharType(); }
	virtual bool Semantics() override;
    };

    class BuiltinFunctionOrd : public BuiltinFunctionInt
    {
    public:
	BuiltinFunctionOrd(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionInt(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() override;
    };

    class BuiltinFunctionLength : public BuiltinFunctionInt
    {
    public:
	BuiltinFunctionLength(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionInt(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() override;
    };

    class BuiltinFunctionPopcnt : public BuiltinFunctionInt
    {
    public:
	BuiltinFunctionPopcnt(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionInt(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() override;
    };

    class BuiltinFunctionSucc : public BuiltinFunctionSameAsArg
    {
    public:
	BuiltinFunctionSucc(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionSameAsArg(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() override;
    };

    class BuiltinFunctionPred : public BuiltinFunctionSucc
    {
    public:
	BuiltinFunctionPred(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionSucc(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class BuiltinFunctionFloat : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionFloat(const std::string& fn, const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a), funcname(fn) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return RealType(); }
	virtual bool Semantics() override;
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
	virtual bool Semantics() override;
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
	virtual bool Semantics() override;
    };

    class BuiltinFunctionDispose : public BuiltinFunctionNew
    {
    public:
	BuiltinFunctionDispose(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionNew(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class BuiltinFunctionFile : public BuiltinFunctionVoid
    {
    public:
	BuiltinFunctionFile(const std::string& fn, const std::vector<ExprAST*>& a)
	    : BuiltinFunctionVoid(a), funcname(fn) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() override;
    protected:
	std::string funcname;
    };

    class BuiltinFunctionFileBool : public BuiltinFunctionFile
    {
    public:
	BuiltinFunctionFileBool(const std::string& fn, const std::vector<ExprAST*>& a)
	    : BuiltinFunctionFile(fn, a) {}
	virtual bool Semantics() override;
	Types::TypeDecl* Type() const override { return BoolType(); }
    };

    class BuiltinFunctionAssign : public BuiltinFunctionVoid
    {
    public:
	BuiltinFunctionAssign(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionVoid(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() override;
    };

    class BuiltinFunctionPanic : public BuiltinFunctionVoid
    {
    public:
	BuiltinFunctionPanic(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionVoid(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() override;
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
	virtual bool Semantics() override;
    };

    class BuiltinFunctionCycles : public BuiltinFunctionClock
    {
    public:
	BuiltinFunctionCycles(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionClock(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class BuiltinFunctionParamcount : public BuiltinFunctionInt
    {
    public:
	BuiltinFunctionParamcount(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionInt(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	virtual bool Semantics() override;
    };

    class BuiltinFunctionParamstr : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionParamstr(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return StringType(); }
	virtual bool Semantics() override;
    };

    class BuiltinFunctionCopy : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionCopy(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return StringType(); }
	virtual bool Semantics() override;
    };

    class BuiltinFunctionMin : public BuiltinFunctionSameAsArg2
    {
    public:
	BuiltinFunctionMin(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionSameAsArg2(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class BuiltinFunctionMax : public BuiltinFunctionSameAsArg2
    {
    public:
	BuiltinFunctionMax(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionSameAsArg2(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class BuiltinFunctionSign : public BuiltinFunctionSameAsArg
    {
    public:
	BuiltinFunctionSign(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionSameAsArg(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    void BuiltinFunctionBase::accept(Visitor& v)
    {
	for(auto a : args)
	{
	    a->accept(v);
	}
    }

    bool BuiltinFunctionSameAsArg::Semantics()
    {
	if (args.size() != 1)
	{
	    return false;
	}
	if (args[0]->Type()->Type() == Types::Char || args[0]->Type()->Type() == Types::Enum)
	{
	    return false;
	}
	if (!(args[0]->Type()->isIntegral()) && args[0]->Type()->Type() != Types::Real)
	{
	    return false;
	}
	return true;
    }

    bool BuiltinFunctionSameAsArg2::Semantics()
    {
	if (args.size() != 2)
	{
	    return false;
	}
	if (!(args[0]->Type()->isIntegral()) && args[0]->Type()->Type() != Types::Real)
	{
	    return false;
	}
	if (!(args[1]->Type()->isIntegral()) && args[1]->Type()->Type() != Types::Real)
	{
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionAbs::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	if (args[0]->Type()->isUnsigned())
	{
	    return a;
	}
	if (args[0]->Type()->isIntegral())
	{
	    llvm::Value* neg = builder.CreateNeg(a, "neg");
	    llvm::Value* cmp = builder.CreateICmpSGE(a, MakeIntegerConstant(0), "abscond");
	    llvm::Value* res = builder.CreateSelect(cmp, a, neg, "abs");
	    return res;
	}
	return CallRuntimeFPFunc(builder, "llvm.fabs.f64", args);
    }

    llvm::Value* BuiltinFunctionOdd::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* v = args[0]->CodeGen();
	v = builder.CreateAnd(v, MakeIntegerConstant(1));
	return builder.CreateBitCast(v, Types::GetType(Types::Boolean));
    }

    bool BuiltinFunctionOdd::Semantics()
    {
	if (args.size() != 1)
	{
	    return false;
	}
	if (!args[0]->Type()->isIntegral())
	{
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionSqr::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	if (a->getType()->getTypeID() == llvm::Type::IntegerTyID)
	{
	    return builder.CreateMul(a, a, "sqr");
	}
	return builder.CreateFMul(a, a, "sqr");
    }

    bool BuiltinFunctionSqr::Semantics()
    {
	if (args.size() != 1)
	{
	    return false;
	}
	if (args[0]->Type()->Type() == Types::Char || args[0]->Type()->Type() == Types::Enum)
	{
	    return false;
	}
	if (!args[0]->Type()->isIntegral() && args[0]->Type()->Type() != Types::Real)
	{
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionFloat::CodeGen(llvm::IRBuilder<>& builder)
    {
	return CallRuntimeFPFunc(builder, funcname, args);
    }

    bool BuiltinFunctionFloat::Semantics()
    {
	if (args.size() != 1)
	{
	    return false;
	}
	if (args[0]->Type()->Type() != Types::Real)
	{
	    // Implicit typecast.
	    if (args[0]->Type()->isIntegral())
	    {
		ExprAST* e = args[0];
		args[0] = new TypeCastAST(Location("",0,0), e, RealType());
		return true;
	    }
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionRound::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* v = CallRuntimeFPFunc(builder, "llvm.round.f64", args);
	return builder.CreateFPToSI(v, Types::GetType(Types::Integer), "to.int");
    }

    bool BuiltinFunctionRound::Semantics()
    {
	if (args.size() != 1)
	{
	    return false;
	}
	if (args[0]->Type()->Type() != Types::Real)
	{
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionTrunc::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* v = args[0]->CodeGen();
	return builder.CreateFPToSI(v, Types::GetType(Types::Integer), "to.int");
    }

    llvm::Value* BuiltinFunctionRandom::CodeGen(llvm::IRBuilder<>& builder)
    {
	std::vector<llvm::Type*> argTypes;
	llvm::Type* ty = Types::GetType(Types::Real);
	llvm::FunctionType* ft = llvm::FunctionType::get(ty, argTypes, false);

	llvm::Constant* f = theModule->getOrInsertFunction("__random", ft);

	return builder.CreateCall(f, "calltmp");
    }

    bool BuiltinFunctionRandom::Semantics()
    {
	return args.size() == 0;
    }


    llvm::Value* BuiltinFunctionChr::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	return builder.CreateTrunc(a, Types::GetType(Types::Char), "chr");
    }

    bool BuiltinFunctionChr::Semantics()
    {
	if (args.size() != 1)
	{
	    return false;
	}
	if (!args[0]->Type()->isIntegral())
	{
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionOrd::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	return builder.CreateZExt(a, Types::GetType(Types::Integer), "ord");
    }

    bool BuiltinFunctionOrd::Semantics()
    {
	if (args.size() != 1)
	{
	    return false;
	}
	if (!args[0]->Type()->isIntegral())
	{
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionSucc::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	return builder.CreateAdd(a, MakeConstant(1, a->getType()), "succ");
    }

    bool BuiltinFunctionSucc::Semantics()
    {
	if (args.size() != 1)
	{
	    return false;
	}
	if (!args[0]->Type()->isIntegral())
	{
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionPred::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	return builder.CreateSub(a, MakeConstant(1, a->getType()), "pred");
    }

    llvm::Value* BuiltinFunctionNew::CodeGen(llvm::IRBuilder<>& builder)
    {
	Types::PointerDecl* pd = llvm::dyn_cast<Types::PointerDecl>(args[0]->Type());
	size_t size = pd->SubType()->Size();
	llvm::Type* ty = Types::GetType(Types::Integer);
	std::vector<llvm::Type*> argTypes = {ty};

	// Result is "void *"
	llvm::Type* resTy = Types::GetVoidPtrType();
	llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("__new", ft);

	llvm::Value* aSize = MakeIntegerConstant(size);

	llvm::Value* retVal = builder.CreateCall(f, aSize, "new");

	VariableExprAST* var = llvm::dyn_cast<VariableExprAST>(args[0]);
	retVal = builder.CreateBitCast(retVal, pd->LlvmType(), "cast");
	llvm::Value* pA = var->Address();
	return builder.CreateStore(retVal, pA);
    }

    bool BuiltinFunctionNew::Semantics()
    {
	if (args.size() != 1)
	{
	    return false;
	}
	if (!llvm::isa<Types::PointerDecl>(args[0]->Type()))
	{
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionDispose::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	llvm::Type* ty = a->getType();
	std::vector<llvm::Type*> argTypes = {ty};
	
	std::string name = "__dispose";
	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetType(Types::Void), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction(name, ft);

	return builder.CreateCall(f, a);
    }

    llvm::Value* BuiltinFunctionFile::CodeGen(llvm::IRBuilder<>& builder)
    {
	VariableExprAST* fvar;
	fvar = llvm::dyn_cast<VariableExprAST>(args[0]);
	llvm::Value* faddr = fvar->Address();
	std::vector<llvm::Type*> argTypes = {faddr->getType()};

	llvm::FunctionType* ft = llvm::FunctionType::get(Type()->LlvmType(), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction(std::string("__") + funcname, ft);

	return builder.CreateCall(f, faddr, "");
    }

    bool BuiltinFunctionFile::Semantics()
    {
	if (args.size() != 1)
	{
	    return false;
	}
	if (!llvm::isa<Types::FileDecl>(args[0]->Type()))
	{
	    return false;
	}
	if (!llvm::isa<VariableExprAST>(args[0]))
	{
	    return false;
	}
	return true;
    }

    // Used for eof/eoln, where no argument means the "input" file.
    bool BuiltinFunctionFileBool::Semantics()
    {
	if (args.size() == 0)
	{
	    args.push_back(new VariableExprAST(Location("",0,0), "input", Types::GetTextType()));
	    return true;
	}
	return BuiltinFunctionFile::Semantics();
    }

    llvm::Value* BuiltinFunctionLength::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* v = MakeAddressable(args[0]);
	std::vector<llvm::Value*> ind = {MakeIntegerConstant(0), MakeIntegerConstant(0)};
	v = builder.CreateGEP(v, ind, "str_0");
	v = builder.CreateLoad(v, "len");

	return builder.CreateZExt(v, Types::GetType(Types::Integer), "extend");
    }

    bool BuiltinFunctionLength::Semantics()
    {
	if (args.size() != 1)
	{
	    return false;
	}
	if (args[0]->Type()->Type() != Types::String)
	{
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionAssign::CodeGen(llvm::IRBuilder<>& builder)
    {
	// assign takes two arguments from the user (file and filename), and a third "recordsize"
	// that we make up here, and a fourth for the "isText" argument.

	// Arg1: address of the filestruct.
	VariableExprAST* fvar = llvm::dyn_cast<VariableExprAST>(args[0]);
	llvm::Value* faddr = fvar->Address();

	llvm::Value* filename = args[1]->CodeGen();
	llvm::Type* ty = filename->getType();
	llvm::Type* intTy = Types::GetType(Types::Integer);
	std::vector<llvm::Type*> argTypes = {faddr->getType(), ty, intTy, intTy};

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

    bool BuiltinFunctionAssign::Semantics()
    {
	if (args.size() != 2)
	{
	    return false;
	}
	if (!llvm::isa<VariableExprAST>(args[0]))
	{
	    return false;
	}
	if (!llvm::isa<Types::FileDecl>(args[0]->Type()) || !args[1]->Type()->isStringLike())
	{
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionCopy::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* str = MakeAddressable(args[0]);
	llvm::Value* start = args[1]->CodeGen();
	llvm::Value* len   = args[2]->CodeGen();

	std::vector<llvm::Type*> argTypes = {str->getType(), start->getType(), len->getType()};

	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetStringType()->LlvmType(), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("__StrCopy", ft);

	return builder.CreateCall3(f, str, start, len, "copy");
    }

    bool BuiltinFunctionCopy::Semantics()
    {
	if (args.size() != 3)
	{
	    return false;
	}
	if (args[0]->Type()->Type() != Types::String ||
	    args[1]->Type()->Type() != Types::Integer ||
	    args[2]->Type()->Type() != Types::Integer)
	{
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionClock::CodeGen(llvm::IRBuilder<>& builder)
    {
	std::vector<llvm::Type*> argTypes;

	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetType(Types::Int64), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("__Clock", ft);

	return builder.CreateCall(f, "clock");
    }

    bool BuiltinFunctionClock::Semantics()
    {
	return args.size() == 0;
    }

    llvm::Value* BuiltinFunctionPanic::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* message = args[0]->CodeGen();
	llvm::Type* ty = message->getType();
	std::vector<llvm::Type*> argTypes = {ty};
	std::vector<llvm::Value*> argsV = {message};

	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetType(Types::Void), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("__Panic", ft);

	return builder.CreateCall(f,  argsV);
    }

    bool BuiltinFunctionPanic::Semantics()
    {
	if (args.size() != 1)
	{
	    return false;
	}
	if (!args[0]->Type()->isStringLike())
	{
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionPopcnt::CodeGen(llvm::IRBuilder<>& builder)
    {
	Types::TypeDecl* type = args[0]->Type();
	std::string name = "llvm.ctpop.i";
	if (type->isIntegral())
	{
	    name += std::to_string(type->Size() * 8);
	    llvm::Type* ty = type->LlvmType();
	    std::vector<llvm::Type*> argTypes = {ty};
	    llvm::FunctionType* ft = llvm::FunctionType::get(ty, argTypes, false);

	    llvm::Constant* f = theModule->getOrInsertFunction(name, ft);
	    llvm::Value* a = args[0]->CodeGen();
	    return builder.CreateCall(f, a, "popcnt");
	}

	name += std::to_string(Types::SetDecl::SetBits);
	llvm::Value *v = MakeAddressable(args[0]);
	std::vector<llvm::Value*> ind{MakeIntegerConstant(0), MakeIntegerConstant(0)};
	llvm::Value *addr = builder.CreateGEP(v, ind, "leftSet");
	llvm::Value *val = builder.CreateLoad(addr);
	llvm::Type* ty = val->getType();
	std::vector<llvm::Type*> argTypes = {ty};
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

    bool BuiltinFunctionPopcnt::Semantics()
    {
	if (args.size() != 1)
	{
	    return false;
	}
	if (!args[0]->Type()->isIntegral() && !llvm::isa<Types::SetDecl>(args[0]->Type()))
	{
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionCycles::CodeGen(llvm::IRBuilder<>& builder)
    {
	std::vector<llvm::Type*> argTypes;

	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetType(Types::Int64), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("llvm.readcyclecounter", ft);

	return builder.CreateCall(f, "cycles");
    }

    llvm::Value* BuiltinFunctionFloat2Arg::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Type* realTy = Types::GetType(Types::Real);
	llvm::Value* a = args[0]->CodeGen();
	llvm::Value* b = args[1]->CodeGen();

	std::vector<llvm::Value*> arg = {a, b};
	std::vector<llvm::Type*> argTypes = {realTy, realTy};

	llvm::FunctionType* ft = llvm::FunctionType::get(realTy, argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction(funcname, ft);
	
	return builder.CreateCall(f, arg);
    }

    bool BuiltinFunctionFloat2Arg::Semantics()
    {
	if (args.size() != 2)
	{
	    return false;
	}
	if (args[0]->Type()->Type() != Types::Real)
	{
	    if (args[0]->Type()->isIntegral())
	    {
		ExprAST* e = args[0];
		args[0] = new TypeCastAST(Location("",0,0), e, RealType());
	    }
	    else
	    {
		return false;
	    }
	}
	if (!(args[1]->Type()->isIntegral()) && args[1]->Type()->Type() != Types::Real)
	{
	    if (args[0]->Type()->isIntegral())
	    {
		ExprAST* e = args[1];
		args[1] = new TypeCastAST(Location("",0,0), e, RealType());
		return true;
	    }
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionParamstr::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* n   = args[0]->CodeGen();

	std::vector<llvm::Type*> argTypes = {n->getType()};

	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetStringType()->LlvmType(), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("__ParamStr", ft);

	return builder.CreateCall(f, n, "paramstr");
    }

    bool BuiltinFunctionParamstr::Semantics()
    {
	if (args.size() != 1)
	{
	    return false;
	}
	if (!args[0]->Type()->isIntegral())
	{
	    return false;
	}
	return true;
    }

    llvm::Value* BuiltinFunctionParamcount::CodeGen(llvm::IRBuilder<>& builder)
    {
	std::vector<llvm::Type*> argTypes = {};

	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetType(Types::Integer), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("__ParamCount", ft);

	return builder.CreateCall(f, "paramcount");
    }

    bool BuiltinFunctionParamcount::Semantics()
    {
	return args.size() == 0;
    }

    llvm::Value* BuiltinFunctionMax::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (args[0]->Type()->Type() == Types::Real ||
	    args[1]->Type()->Type() == Types::Real)
	{
	    BuiltinFunctionFloat2Arg max("llvm.maxnum.f64", args);
	    return max.CodeGen(builder);
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
	if (args[0]->Type()->Type() == Types::Real ||
	    args[1]->Type()->Type() == Types::Real)
	{
	    BuiltinFunctionFloat2Arg min("llvm.minnum.f64", args);
	    return min.CodeGen(builder);
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
	if (args[0]->Type()->isUnsigned())
	{
	    llvm::Value* zero = MakeIntegerConstant(0);
	    llvm::Value* one = MakeIntegerConstant(1);
	    llvm::Value* sel1 = builder.CreateICmpUGT(v, zero, "gt");
	    return builder.CreateSelect(sel1, one, zero, "sgn1");
	} 
	llvm::Value* zero = MakeIntegerConstant(0);
	llvm::Value* one = MakeIntegerConstant(1);
	llvm::Value* mone = MakeIntegerConstant(-1);
	llvm::Value* sel1 = builder.CreateICmpSGT(v, zero, "gt");
	llvm::Value* sel2 = builder.CreateICmpSLT(v, zero, "lt");
	llvm::Value* res = builder.CreateSelect(sel1, one, zero, "sgn1");
	return builder.CreateSelect(sel2, mone, res, "sgn2");
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

#define NEW(name) [](ArgList& a)->BuiltinFunctionBase*{return new BuiltinFunction##name(a);}
#define NEW2(name, func) [](ArgList& a)->BuiltinFunctionBase*{return new BuiltinFunction##name(func, a);}

    void InitBuiltins()
    {
	AddBIFCreator("abs",        NEW(Abs));
	AddBIFCreator("odd",        NEW(Odd));
	AddBIFCreator("sqr",        NEW(Sqr));
	AddBIFCreator("round",      NEW(Round));
	AddBIFCreator("trunc",      NEW(Trunc));
	AddBIFCreator("random",     NEW(Random));
	AddBIFCreator("chr",        NEW(Chr));
	AddBIFCreator("ord",        NEW(Ord));
	AddBIFCreator("succ",       NEW(Succ));
	AddBIFCreator("pred",       NEW(Pred));
	AddBIFCreator("new",        NEW(New));
	AddBIFCreator("dispose",    NEW(Dispose));
	AddBIFCreator("length",     NEW(Length));
	AddBIFCreator("popcnt",     NEW(Popcnt));
	AddBIFCreator("assign",     NEW(Assign));
	AddBIFCreator("panic",      NEW(Panic));
	AddBIFCreator("clock",      NEW(Clock));
	AddBIFCreator("cycles",     NEW(Cycles));
	AddBIFCreator("paramcount", NEW(Paramcount));
	AddBIFCreator("paramstr",   NEW(Paramstr));
	AddBIFCreator("copy",       NEW(Copy));
	AddBIFCreator("max",        NEW(Max));
	AddBIFCreator("min",        NEW(Min));
	AddBIFCreator("sign",       NEW(Sign));
	AddBIFCreator("sqrt",       NEW2(FloatIntrinsic, "sqrt"));
	AddBIFCreator("sin",        NEW2(FloatIntrinsic, "sin"));
	AddBIFCreator("cos",        NEW2(FloatIntrinsic, "cos"));
	AddBIFCreator("ln",         NEW2(FloatIntrinsic, "log"));
	AddBIFCreator("exp",        NEW2(FloatIntrinsic, "exp"));
	AddBIFCreator("arctan",     NEW2(Float, "atan"));
	AddBIFCreator("tan",        NEW2(Float, "tan"));
	AddBIFCreator("arctan2",    NEW2(Float2Arg, "atan2"));
	AddBIFCreator("fmod",       NEW2(Float2Arg, "fmod"));
	AddBIFCreator("reset",      NEW2(File, "reset"));
	AddBIFCreator("rewrite",    NEW2(File, "rewrite"));
	AddBIFCreator("append",     NEW2(File, "append"));
	AddBIFCreator("close",      NEW2(File, "close"));
	AddBIFCreator("get",        NEW2(File, "get"));
	AddBIFCreator("put",        NEW2(File, "put"));
	AddBIFCreator("eof",        NEW2(FileBool, "eof"));
	AddBIFCreator("eoln",       NEW2(FileBool, "eoln"));
    }
} // namespace Builtin
