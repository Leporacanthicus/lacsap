#include "builtin.h"
#include "expr.h"
#include "options.h"
#include <functional>
#include <llvm/IR/DataLayout.h>

extern llvm::Module* theModule;

namespace Builtin
{
    static bool CastIntegerToReal(ExprAST*& arg)
    {
	if (!llvm::isa<Types::RealDecl>(arg->Type()))
	{
	    // Implicit typecast.
	    if (!IsIntegral(arg->Type()))
	    {
		return false;
	    }
	    arg = Recast(arg, Types::Get<Types::RealDecl>());
	}
	return true;
    }

    using ArgList = const std::vector<ExprAST*>;
    using CreateBIFObject = std::function<FunctionBase*(const std::string&, ArgList&)>;

    std::map<std::string, CreateBIFObject> BIFMap;

    static llvm::Value* CallRuntimeFPFunc(llvm::IRBuilder<>& builder, const std::string& func, ArgList& args)
    {
	llvm::Value*         a = args[0]->CodeGen();
	llvm::Type*          ty = args[0]->Type()->LlvmType();
	llvm::FunctionCallee f = GetFunction(ty, { ty }, func);
	return builder.CreateCall(f, a, "calltmp");
    }

    static llvm::Value* CallRuntimeCplxFunc(llvm::IRBuilder<>& builder, const std::string& func,
                                            ArgList& args)
    {
	llvm::Value*         res = CreateTempAlloca(Types::Get<Types::ComplexDecl>());
	llvm::Value*         a = args[0]->CodeGen();
	llvm::Type*          ty = args[0]->Type()->LlvmType();
	llvm::Type*          pty = llvm::PointerType::getUnqual(ty);
	llvm::FunctionCallee f = GetFunction(Types::Get<Types::VoidDecl>()->LlvmType(), { pty, ty },
	                                     "__c" + func);
	builder.CreateCall(f, { res, a });
	return builder.CreateLoad(ty, res);
    }

    template<typename TY>
    class FunctionType : public FunctionBase
    {
    public:
	using FunctionBase::FunctionBase;
	Types::TypeDecl* Type() const override { return Types::Get<TY>(); }
    };

    using FunctionInt = FunctionType<Types::IntegerDecl>;
    using FunctionLongInt = FunctionType<Types::Int64Decl>;
    using FunctionBool = FunctionType<Types::BoolDecl>;
    using FunctionReal = FunctionType<Types::RealDecl>;
    using FunctionVoid = FunctionType<Types::VoidDecl>;
    using FunctionCplx = FunctionType<Types::ComplexDecl>;

    class FunctionString : public FunctionBase
    {
    public:
	using FunctionBase::FunctionBase;
	Types::TypeDecl* Type() const override { return Types::Get<Types::StringDecl>(255); }
    };

    class FunctionSameAsArg : public FunctionBase
    {
    public:
	using FunctionBase::FunctionBase;
	Types::TypeDecl* Type() const override { return args[0]->Type(); }
	ErrorType        Semantics() override;
    };

    class FunctionSameAsArg2 : public FunctionBase
    {
    public:
	using FunctionBase::FunctionBase;
	Types::TypeDecl* Type() const override { return args[0]->Type(); }
	ErrorType        Semantics() override;
    };

    class FunctionAbs : public FunctionSameAsArg
    {
    public:
	using FunctionSameAsArg::FunctionSameAsArg;
	Types::TypeDecl* Type() const override;
	llvm::Value*     CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionSqr : public FunctionSameAsArg
    {
    public:
	using FunctionSameAsArg::FunctionSameAsArg;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionOdd : public FunctionBool
    {
    public:
	using FunctionBool::FunctionBool;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionRound : public FunctionInt
    {
    public:
	using FunctionInt::FunctionInt;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionTrunc : public FunctionRound
    {
    public:
	using FunctionRound::FunctionRound;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionIntConvert : public FunctionReal
    {
    public:
	using FunctionReal::FunctionReal;
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionRandom : public FunctionReal
    {
    public:
	using FunctionReal::FunctionReal;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionChr : public FunctionBase
    {
    public:
	using FunctionBase::FunctionBase;
	llvm::Value*     CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return Types::Get<Types::CharDecl>(); }
	ErrorType        Semantics() override;
    };

    class FunctionOrd : public FunctionInt
    {
    public:
	using FunctionInt::FunctionInt;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionLength : public FunctionInt
    {
    public:
	using FunctionInt::FunctionInt;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionPopcnt : public FunctionInt
    {
    public:
	using FunctionInt::FunctionInt;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionSucc : public FunctionSameAsArg
    {
    public:
	using FunctionSameAsArg::FunctionSameAsArg;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionPred : public FunctionSucc
    {
    public:
	using FunctionSucc::FunctionSucc;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionFloat : public FunctionBase
    {
    public:
	FunctionFloat(const std::string& fn, ArgList& a) : FunctionBase(fn, a), func(fn) {}
	FunctionFloat(const std::string& fn, ArgList& a, const std::string& nn)
	    : FunctionBase(fn, a), func(nn)
	{
	}
	llvm::Value*     CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override;
	ErrorType        Semantics() override;

    protected:
	std::string func;
    };

    class FunctionFloat2Arg : public FunctionFloat
    {
    public:
	using FunctionFloat::FunctionFloat;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionFloatIntrinsic : public FunctionFloat
    {
    public:
	using FunctionFloat::FunctionFloat;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionNew : public FunctionVoid
    {
    public:
	using FunctionVoid::FunctionVoid;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionDispose : public FunctionNew
    {
    public:
	using FunctionNew::FunctionNew;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionHalt : public FunctionVoid
    {
    public:
	using FunctionVoid::FunctionVoid;
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionInc : public FunctionVoid
    {
    public:
	using FunctionVoid::FunctionVoid;
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionDec : public FunctionInc
    {
    public:
	using FunctionInc::FunctionInc;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionPack : public FunctionVoid
    {
    public:
	using FunctionVoid::FunctionVoid;
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionUnpack : public FunctionVoid
    {
    public:
	using FunctionVoid::FunctionVoid;
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionVal : public FunctionVoid
    {
    public:
	using FunctionVoid::FunctionVoid;
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionFile : public FunctionVoid
    {
    public:
	FunctionFile(const std::string& fn, ArgList& a) : FunctionVoid(fn, a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionFileInfo : public FunctionFile
    {
    public:
	using FunctionFile::FunctionFile;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionFileBool : public FunctionFile
    {
    public:
	using FunctionFile::FunctionFile;
	ErrorType        Semantics() override;
	Types::TypeDecl* Type() const override { return Types::Get<Types::BoolDecl>(); }
    };

    class FunctionFileLong : public FunctionFile
    {
    public:
	using FunctionFile::FunctionFile;
	Types::TypeDecl* Type() const override { return Types::Get<Types::Int64Decl>(); }
    };

    class FunctionAssign : public FunctionVoid
    {
    public:
	using FunctionVoid::FunctionVoid;
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionPanic : public FunctionVoid
    {
    public:
	using FunctionVoid::FunctionVoid;
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionClock : public FunctionLongInt
    {
    public:
	using FunctionLongInt::FunctionLongInt;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionCycles : public FunctionClock
    {
    public:
	using FunctionClock::FunctionClock;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionParamcount : public FunctionInt
    {
    public:
	using FunctionInt::FunctionInt;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionParamstr : public FunctionString
    {
    public:
	using FunctionString::FunctionString;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionCopy : public FunctionString
    {
    public:
	using FunctionString::FunctionString;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionTrim : public FunctionString
    {
    public:
	using FunctionString::FunctionString;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionIndex : public FunctionInt
    {
    public:
	using FunctionInt::FunctionInt;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	ErrorType    Semantics() override;
    };

    class FunctionMin : public FunctionSameAsArg2
    {
    public:
	using FunctionSameAsArg2::FunctionSameAsArg2;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionMax : public FunctionSameAsArg2
    {
    public:
	using FunctionSameAsArg2::FunctionSameAsArg2;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionSign : public FunctionInt
    {
    public:
	using FunctionInt::FunctionInt;
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionGetTimeStamp : public FunctionVoid
    {
    public:
	using FunctionVoid::FunctionVoid;
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionTime : public FunctionBase
    {
    public:
	FunctionTime(const std::string& fn, ArgList& a);
	ErrorType        Semantics() override;
	llvm::Value*     CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return arrayType; }

    private:
	Types::TypeDecl* arrayType;
    };

    class FunctionDate : public FunctionBase
    {
    public:
	FunctionDate(const std::string& fn, ArgList& a);
	ErrorType        Semantics() override;
	llvm::Value*     CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return arrayType; }

    private:
	Types::TypeDecl* arrayType;
    };

    class FunctionBinding : public FunctionFile
    {
    public:
	using FunctionFile::FunctionFile;
	Types::TypeDecl* Type() const override { return Types::GetBindingType(); }
    };

    class FunctionBind : public FunctionFile
    {
    public:
	using FunctionFile::FunctionFile;
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionStrCompOp : public FunctionBool
    {
    public:
	FunctionStrCompOp(const std::string& fn, ArgList& a, Token::TokenType o) : FunctionBool(fn, a), op(o)
	{
	}
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;

    private:
	Token::TokenType op;
    };

    class FunctionSeek : public FunctionFile
    {
    public:
	using FunctionFile::FunctionFile;
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionComplex : public FunctionCplx
    {
    public:
	using FunctionCplx::FunctionCplx;
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionPolar : public FunctionComplex
    {
    public:
	using FunctionComplex::FunctionComplex;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionReIm : public FunctionReal
    {
    public:
	FunctionReIm(const std::string& fn, ArgList& a, int idx) : FunctionReal(fn, a), index(idx) {}
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;

    private:
	int index;
    };

    class FunctionCmplxToReal : public FunctionReal
    {
    public:
	FunctionCmplxToReal(const std::string& fn, ArgList& a) : FunctionReal(fn, a) {}
	ErrorType    Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    void FunctionBase::accept(ASTVisitor& v)
    {
	for (auto a : args)
	{
	    a->accept(v);
	}
    }

    ErrorType FunctionSameAsArg::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!Types::IsNumeric(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    ErrorType FunctionSameAsArg2::Semantics()
    {
	if (args.size() != 2)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!Types::IsNumeric(args[0]->Type()) || !Types::IsNumeric(args[1]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    /* Note that abs returnes "real" for complex, otherwise int or real depending on input type */
    Types::TypeDecl* FunctionAbs::Type() const
    {
	if (llvm::isa<Types::ComplexDecl>(args[0]->Type()))
	{
	    return Types::Get<Types::RealDecl>();
	}
	return args[0]->Type();
    }

    llvm::Value* FunctionAbs::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (llvm::isa<Types::ComplexDecl>(args[0]->Type()))
	{
	    llvm::Constant* zero = MakeIntegerConstant(0);
	    llvm::Constant* one = MakeIntegerConstant(1);
	    auto            v = llvm::dyn_cast<AddressableAST>(args[0]);
	    llvm::Value*    a = v->Address();
	    llvm::Type*     ty = Types::Get<Types::RealDecl>()->LlvmType();

	    llvm::Value* re = builder.CreateLoad(ty, builder.CreateGEP(ty, a, zero, "re"));
	    llvm::Value* im = builder.CreateLoad(ty, builder.CreateGEP(ty, a, one, "im"));
	    llvm::Value* rexre = builder.CreateFMul(re, re, "rexre");
	    llvm::Value* imxim = builder.CreateFMul(im, im, "imxim");
	    llvm::Value* sum = builder.CreateFAdd(rexre, imxim, "addi");

	    llvm::FunctionCallee f = GetFunction(ty, { ty }, "llvm.sqrt.f64");
	    return builder.CreateCall(f, sum, "sqrt");
	}

	llvm::Value* a = args[0]->CodeGen();
	if (IsUnsigned(args[0]->Type()))
	{
	    return a;
	}
	if (IsIntegral(args[0]->Type()))
	{
	    llvm::Value* neg = builder.CreateNeg(a, "neg");
	    llvm::Value* cmp = builder.CreateICmpSGE(a, MakeIntegerConstant(0), "abscond");
	    llvm::Value* res = builder.CreateSelect(cmp, a, neg, "abs");
	    return res;
	}
	return CallRuntimeFPFunc(builder, "llvm.fabs.f64", args);
    }

    llvm::Value* FunctionOdd::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* v = args[0]->CodeGen();
	v = builder.CreateAnd(v, MakeIntegerConstant(1));
	return builder.CreateTrunc(v, Types::Get<Types::BoolDecl>()->LlvmType(), "odd");
    }

    ErrorType FunctionOdd::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!IsIntegral(args[0]->Type()) || llvm::isa<Types::CharDecl>(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionSqr::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (llvm::isa<Types::ComplexDecl>(args[0]->Type()))
	{
	    llvm::Constant* zero = MakeIntegerConstant(0);
	    llvm::Constant* one = MakeIntegerConstant(1);
	    auto            v = llvm::dyn_cast<AddressableAST>(args[0]);
	    llvm::Value*    a = v->Address();
	    llvm::Type*     ty = Types::Get<Types::RealDecl>()->LlvmType();
	    llvm::Type*     cplxTy = Types::Get<Types::ComplexDecl>()->LlvmType();

	    llvm::Value* res = CreateTempAlloca(Types::Get<Types::ComplexDecl>());
	    llvm::Value* re = builder.CreateLoad(ty, builder.CreateGEP(ty, a, zero, "re"));
	    llvm::Value* im = builder.CreateLoad(ty, builder.CreateGEP(ty, a, one, "im"));
	    llvm::Value* rexre = builder.CreateFMul(re, re, "rexre");
	    llvm::Value* imxim = builder.CreateFMul(im, im, "imxim");
	    llvm::Value* resr = builder.CreateFSub(rexre, imxim, "subr");
	    llvm::Value* rexim = builder.CreateFMul(re, im, "rexim");
	    llvm::Value* resi = builder.CreateFAdd(rexim, rexim, "addi");
	    builder.CreateStore(resr, builder.CreateGEP(ty, res, zero));
	    builder.CreateStore(resi, builder.CreateGEP(ty, res, one));
	    return builder.CreateLoad(cplxTy, res);
	}
	llvm::Value* a = args[0]->CodeGen();
	if (IsIntegral(args[0]->Type()))
	{
	    return builder.CreateMul(a, a, "sqr");
	}
	return builder.CreateFMul(a, a, "sqr");
    }

    llvm::Value* FunctionFloat::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (llvm::isa<Types::ComplexDecl>(args[0]->Type()))
	{
	    return CallRuntimeCplxFunc(builder, func, args);
	}
	return CallRuntimeFPFunc(builder, func, args);
    }

    Types::TypeDecl* FunctionFloat::Type() const
    {
	if (llvm::isa<Types::ComplexDecl>(args[0]->Type()))
	{
	    return Types::Get<Types::ComplexDecl>();
	}
	return Types::Get<Types::RealDecl>();
    }

    ErrorType FunctionFloat::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}

	if (llvm::isa<Types::ComplexDecl>(args[0]->Type()) || CastIntegerToReal(args[0]))
	{
	    return ErrorType::Ok;
	}
	return ErrorType::WrongArgType;
    }

    llvm::Value* FunctionFloatIntrinsic::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (llvm::isa<Types::ComplexDecl>(args[0]->Type()))
	{
	    return CallRuntimeCplxFunc(builder, func, args);
	}
	return CallRuntimeFPFunc(builder, "llvm." + func + ".f64", args);
    }

    llvm::Value* FunctionRound::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* v = CallRuntimeFPFunc(builder, "llvm.round.f64", args);
	return builder.CreateFPToSI(v, Types::Get<Types::IntegerDecl>()->LlvmType(), "to.int");
    }

    ErrorType FunctionRound::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!llvm::isa<Types::RealDecl>(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionTrunc::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* v = args[0]->CodeGen();
	return builder.CreateFPToSI(v, Types::Get<Types::IntegerDecl>()->LlvmType(), "to.int");
    }

    llvm::Value* FunctionRandom::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::FunctionCallee f = GetFunction(Types::Get<Types::RealDecl>()->LlvmType(), {}, "__random");
	return builder.CreateCall(f, {}, "calltmp");
    }

    ErrorType FunctionIntConvert::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!llvm::isa<Types::RealDecl>(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionIntConvert::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value*         orig = args[0]->CodeGen();
	llvm::Type*          ty = args[0]->Type()->LlvmType();
	llvm::FunctionCallee fnAbs = GetFunction(ty, { ty }, "llvm.fabs.f64");
	llvm::Value*         abs = builder.CreateCall(fnAbs, orig, "abs");
	llvm::FunctionCallee fnFloor = GetFunction(ty, { ty }, "llvm.floor.f64");
	llvm::Value*         floor = builder.CreateCall(fnFloor, abs, "floor");
	llvm::FunctionCallee fnCopySign = GetFunction(ty, { ty, ty }, "llvm.copysign.f64");
	return builder.CreateCall(fnCopySign, { floor, orig }, "int");
    }

    ErrorType FunctionRandom::Semantics()
    {
	if (args.size() != 0)
	{
	    return ErrorType::WrongArgCount;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionChr::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	return builder.CreateTrunc(a, Types::Get<Types::CharDecl>()->LlvmType(), "chr");
    }

    ErrorType FunctionChr::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!IsIntegral(args[0]->Type()) || llvm::isa<Types::CharDecl>(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionOrd::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	return builder.CreateZExt(a, Types::Get<Types::IntegerDecl>()->LlvmType(), "ord");
    }

    ErrorType FunctionOrd::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!IsIntegral(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionSucc::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	llvm::Value* b = (args.size() == 2) ? args[1]->CodeGen() : MakeConstant(1, args[0]->Type());

	return builder.CreateAdd(a, b, "succ");
    }

    ErrorType FunctionSucc::Semantics()
    {
	if (args.size() < 1 || args.size() > 2)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!IsIntegral(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	if (args.size() == 2 && !llvm::isa<Types::IntegerDecl>(args[1]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionPred::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	llvm::Value* b = (args.size() == 2) ? args[1]->CodeGen() : MakeConstant(1, args[0]->Type());

	return builder.CreateSub(a, b, "pred");
    }

    llvm::Value* FunctionNew::CodeGen(llvm::IRBuilder<>& builder)
    {
	auto pd = llvm::dyn_cast<Types::PointerDecl>(args[0]->Type());
	ICE_IF(!pd, "The argument to new should be a PointerDecl!");
	const Types::TypeDecl* elemTy = pd->SubType();
	size_t                 size = elemTy->Size();
	llvm::Type*            ty = Types::Get<Types::IntegerDecl>()->LlvmType();

	// Result is "void *"
	llvm::Type*          voidTy = Types::GetVoidPtrType();
	llvm::FunctionCallee f = GetFunction(voidTy, { ty }, "__new");

	llvm::Value* retVal = builder.CreateCall(f, { MakeIntegerConstant(size) }, "new");

	auto var = llvm::dyn_cast<AddressableAST>(args[0]);
	// TODO: Fix this to be a proper TypeCast...
	retVal = builder.CreateBitCast(retVal, pd->LlvmType(), "cast");
	llvm::Value* pA = var->Address();

	// TODO: We need to recursively process the type here, and construct vtables for all
	// of the elements that are classes (that have VTables).
	if (auto cd = llvm::dyn_cast<Types::ClassDecl>(elemTy))
	{
	    if (cd->VTableType(true))
	    {
		llvm::GlobalVariable* gv = theModule->getGlobalVariable("vtable_" + cd->Name(), true);
		llvm::Value* dest = builder.CreateGEP(voidTy, retVal, MakeIntegerConstant(0), "vtable");
		builder.CreateStore(gv, dest);
	    }
	}
	return builder.CreateStore(retVal, pA);
    }

    ErrorType FunctionNew::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!llvm::isa<Types::PointerDecl>(args[0]->Type()) || !llvm::isa<AddressableAST>(args[0]))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionDispose::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Type*          ty = args[0]->Type()->LlvmType();
	llvm::FunctionCallee f = GetFunction(Types::Get<Types::VoidDecl>()->LlvmType(), { ty }, "__dispose");

	return builder.CreateCall(f, { args[0]->CodeGen() });
    }

    ErrorType FunctionHalt::Semantics()
    {
	if (args.size() > 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (args.size() == 1 && (!IsIntegral(args[0]->Type()) || llvm::isa<Types::CharDecl>(args[0]->Type())))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionHalt::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* v = MakeIntegerConstant(0);
	if (args.size() == 1)
	{
	    v = args[0]->CodeGen();
	}

	llvm::FunctionCallee f = GetFunction(Types::Get<Types::VoidDecl>()->LlvmType(),
	                                     { Types::Get<Types::IntegerDecl>()->LlvmType() }, "exit");

	return builder.CreateCall(f, { v });
    }

    ErrorType FunctionInc::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!IsIntegral(args[0]->Type()) || !llvm::isa<AddressableAST>(args[0]))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionInc::CodeGen(llvm::IRBuilder<>& builder)
    {
	auto var = llvm::dyn_cast<AddressableAST>(args[0]);
	ICE_IF(!var, "Expected variable here... Semantics not working?");
	llvm::Value* pA = var->Address();
	llvm::Type*  ty = var->Type()->LlvmType();
	llvm::Value* a = builder.CreateLoad(ty, pA, "inc");
	a = builder.CreateAdd(a, MakeConstant(1, var->Type()), "inc");
	return builder.CreateStore(a, pA);
    }

    llvm::Value* FunctionDec::CodeGen(llvm::IRBuilder<>& builder)
    {
	auto var = llvm::dyn_cast<AddressableAST>(args[0]);
	ICE_IF(!var, "Expected variable here... Semantics not working?");
	llvm::Value* pA = var->Address();
	llvm::Type*  ty = var->Type()->LlvmType();
	llvm::Value* a = builder.CreateLoad(ty, pA, "dec");
	a = builder.CreateSub(a, MakeConstant(1, var->Type()), "dec");
	return builder.CreateStore(a, pA);
    }

    // Pack(a, start, apacked);
    ErrorType FunctionPack::Semantics()
    {
	if (args.size() != 3)
	{
	    return ErrorType::WrongArgCount;
	}
	auto* t0 = llvm::dyn_cast<Types::ArrayDecl>(args[0]->Type());
	auto* t2 = llvm::dyn_cast<Types::ArrayDecl>(args[2]->Type());
	if (t0 && t2)
	{
	    if (IsIntegral(args[1]->Type()) && args[1]->Type()->AssignableType(t0->Ranges()[0]))
	    {
		if (t0->SubType() != t2->SubType())
		{
		    return ErrorType::WrongArgType;
		}
		if (!llvm::isa<VariableExprAST>(args[0]) || !llvm::isa<VariableExprAST>(args[2]))
		{
		    return ErrorType::WrongArgType;
		}
		if (t0->Ranges().size() != 1 || t2->Ranges().size() != 1)
		{
		    return ErrorType::WrongArgType;
		}
		return ErrorType::Ok;
	    }
	}
	return ErrorType::WrongArgType;
    }

    llvm::Value* FunctionPack::CodeGen(llvm::IRBuilder<>& builder)
    {
	// Pack(X, n, Y) -> copy X to Y, starting at offset n
	auto* var0 = llvm::dyn_cast<VariableExprAST>(args[0]);
	auto* var2 = llvm::dyn_cast<VariableExprAST>(args[2]);

	llvm::Value* start = args[1]->CodeGen();
	auto*        ty0 = llvm::dyn_cast<Types::ArrayDecl>(args[0]->Type());
	auto         r = llvm::dyn_cast<Types::RangeDecl>(ty0->Ranges()[0]);
	if (r->Start())
	{
	    start = builder.CreateSub(start, MakeConstant(r->Start(), args[1]->Type()));
	}

	llvm::Value* pA = var0->Address();
	llvm::Value* pB = var2->Address();

	llvm::Type*  ptrTy = ty0->SubType()->LlvmType();
	llvm::Value* src = builder.CreateGEP(ptrTy, pA, start, "dest");
	llvm::Align  dest_align{ std::max(AlignOfType(pB->getType()), MIN_ALIGN) };
	llvm::Align  src_align{ std::max(AlignOfType(src->getType()), MIN_ALIGN) };
	return builder.CreateMemCpy(pB, dest_align, src, src_align, args[2]->Type()->Size());
    }

    // Unpack(apacked, a, start);
    ErrorType FunctionUnpack::Semantics()
    {
	if (args.size() != 3)
	{
	    return ErrorType::WrongArgCount;
	}
	auto t0 = llvm::dyn_cast<Types::ArrayDecl>(args[0]->Type());
	auto t1 = llvm::dyn_cast<Types::ArrayDecl>(args[1]->Type());
	if (t0 && t1)
	{
	    if (IsIntegral(args[2]->Type()) && args[2]->Type()->AssignableType(t1->Ranges()[0]))
	    {
		if (t0->SubType() != t1->SubType())
		{
		    return ErrorType::WrongArgType;
		}
		if (!llvm::isa<VariableExprAST>(args[0]) || !llvm::isa<VariableExprAST>(args[1]))
		{
		    return ErrorType::WrongArgType;
		}
		if (t0->Ranges().size() != 1 || t1->Ranges().size() != 1 || !IsIntegral(args[2]->Type()))
		{
		    return ErrorType::WrongArgType;
		}
		return ErrorType::Ok;
	    }
	}
	return ErrorType::WrongArgType;
    }

    llvm::Value* FunctionUnpack::CodeGen(llvm::IRBuilder<>& builder)
    {
	// Unpack(X, Y, n) -> copy X to Y, starting at offset n
	auto var0 = llvm::dyn_cast<VariableExprAST>(args[0]);
	auto var1 = llvm::dyn_cast<VariableExprAST>(args[1]);

	llvm::Value* start = args[2]->CodeGen();
	auto         ty1 = llvm::dyn_cast<Types::ArrayDecl>(args[1]->Type());
	auto         r = llvm::dyn_cast<Types::RangeDecl>(ty1->Ranges()[0]);
	if (r->Start())
	{
	    start = builder.CreateSub(start, MakeConstant(r->Start(), args[2]->Type()));
	}

	llvm::Value* pA = var0->Address();
	llvm::Value* pB = var1->Address();

	llvm::Type*  ptrTy = ty1->SubType()->LlvmType();
	llvm::Value* dest = builder.CreateGEP(ptrTy, pB, start, "dest");
	llvm::Align  dest_align{ std::max(AlignOfType(dest->getType()), MIN_ALIGN) };
	llvm::Align  src_align{ std::max(AlignOfType(pA->getType()), MIN_ALIGN) };
	return builder.CreateMemCpy(dest, dest_align, pA, src_align, args[0]->Type()->Size());
    }

    ErrorType FunctionVal::Semantics()
    {
	if (args.size() != 2)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!IsStringLike(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	if (!llvm::isa<VariableExprAST>(args[1]))
	{
	    return ErrorType::WrongArgType;
	}
	if (!Types::IsNumeric(args[1]->Type()) || llvm::isa<Types::ComplexDecl>(args[1]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionVal::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* str = MakeStringFromExpr(args[0], args[0]->Type());
	auto         var1 = llvm::dyn_cast<VariableExprAST>(args[1]);
	std::string  name = "__Val_";
	if (llvm::isa<Types::IntegerDecl>(var1->Type()))
	{
	    name += "int";
	}
	else if (llvm::isa<Types::Int64Decl>(var1->Type()))
	{
	    name += "long";
	}
	else if (llvm::isa<Types::RealDecl>(var1->Type()))
	{
	    name += "real";
	}
	else
	{
	    ICE("Unknown type for 'val'");
	}
	llvm::Value*         res = var1->Address();
	llvm::Type*          ty0 = str->getType();
	llvm::Type*          ty1 = res->getType();
	llvm::FunctionCallee f = GetFunction(Types::Get<Types::VoidDecl>()->LlvmType(), { ty0, ty1 }, name);

	return builder.CreateCall(f, { str, res });
    }

    llvm::Value* FunctionFile::CodeGen(llvm::IRBuilder<>& builder)
    {
	auto fvar = llvm::dyn_cast<AddressableAST>(args[0]);
	ICE_IF(!fvar, "Should be a variable here");
	llvm::Value*         faddr = fvar->Address();
	llvm::FunctionCallee f = GetFunction(Type()->LlvmType(), { faddr->getType() }, "__" + name);

	return builder.CreateCall(f, { faddr });
    }

    ErrorType FunctionFile::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!llvm::isa<Types::FileDecl>(args[0]->Type()) || !llvm::isa<AddressableAST>(args[0]))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionFileInfo::CodeGen(llvm::IRBuilder<>& builder)
    {
	auto fvar = llvm::dyn_cast<AddressableAST>(args[0]);
	ICE_IF(!fvar, "Should be a variable here");
	llvm::Value*             faddr = fvar->Address();
	std::vector<llvm::Type*> argTypes = { faddr->getType(), Types::Get<Types::IntegerDecl>()->LlvmType(),
	                                      Types::Get<Types::BoolDecl>()->LlvmType() };
	llvm::FunctionCallee     f = GetFunction(Type()->LlvmType(), argTypes, "__" + name);

	auto         fd = llvm::dyn_cast<Types::FileDecl>(fvar->Type());
	llvm::Value* sz = MakeIntegerConstant(fd->SubType()->Size());
	llvm::Value* isText = MakeBooleanConstant(llvm::isa<Types::TextDecl>(fd));

	std::vector<llvm::Value*> argList = { faddr, sz, isText };

	return builder.CreateCall(f, argList);
    }

    // Used for eof/eoln, where no argument means the "input" file.
    ErrorType FunctionFileBool::Semantics()
    {
	if (args.size() == 0)
	{
	    args.push_back(new VariableExprAST(Location(), "input", Types::Get<Types::TextDecl>()));
	    return ErrorType::Ok;
	}
	return FunctionFile::Semantics();
    }

    llvm::Value* FunctionLength::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* v = MakeAddressable(args[0]);
	llvm::Type*  charTy = Types::Get<Types::CharDecl>()->LlvmType();
	v = builder.CreateGEP(charTy, v, MakeIntegerConstant(0), "str_0");
	v = builder.CreateLoad(charTy, v, "len");

	return builder.CreateZExt(v, Types::Get<Types::IntegerDecl>()->LlvmType(), "extend");
    }

    ErrorType FunctionLength::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!llvm::isa<Types::StringDecl>(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionAssign::CodeGen(llvm::IRBuilder<>& builder)
    {
	// assign takes two arguments from the user (file and filename), and a third "recordsize"
	// that we make up here, and a fourth for the "isText" argument.

	// Arg1: address of the filestruct.
	auto         fvar = llvm::dyn_cast<AddressableAST>(args[0]);
	llvm::Value* faddr = fvar->Address();

	llvm::Value*             filename = args[1]->CodeGen();
	llvm::Type*              ty = filename->getType();
	std::vector<llvm::Type*> argTypes = { faddr->getType(), ty };

	llvm::FunctionCallee f = GetFunction(Types::Get<Types::VoidDecl>()->LlvmType(), argTypes, "__assign");

	std::vector<llvm::Value*> argsV = { faddr, filename };
	return builder.CreateCall(f, argsV);
    }

    ErrorType FunctionAssign::Semantics()
    {
	if (args.size() != 2)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!llvm::isa<AddressableAST>(args[0]))
	{
	    return ErrorType::WrongArgType;
	}
	if (!llvm::isa<Types::FileDecl>(args[0]->Type()) || !IsStringLike(args[1]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionCopy::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* str = MakeStringFromExpr(args[0], args[0]->Type());
	llvm::Value* start = args[1]->CodeGen();

	llvm::Value* len;
	if (args.size() == 2)
	{
	    len = MakeIntegerConstant(255);
	}
	else
	{
	    len = args[2]->CodeGen();
	    ;
	}

	std::vector<llvm::Type*> argTypes = { str->getType(), start->getType(), len->getType() };
	llvm::Type*              strTy = Type()->LlvmType();
	llvm::FunctionCallee     f = GetFunction(strTy, argTypes, "__StrCopy");

	std::vector<llvm::Value*> argsV = { str, start, len };

	return builder.CreateCall(f, argsV, "copy");
    }

    ErrorType FunctionCopy::Semantics()
    {
	if (args.size() < 2 || args.size() > 3)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!IsStringLike(args[0]->Type()) || !IsIntegral(args[1]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	if (args.size() == 3 && !IsIntegral(args[2]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionTrim::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* str = MakeStringFromExpr(args[0], args[0]->Type());

	llvm::Type*          strTy = Type()->LlvmType();
	llvm::FunctionCallee f = GetFunction(strTy, { str->getType() }, "__StrTrim");

	return builder.CreateCall(f, { str }, "trim");
    }

    ErrorType FunctionTrim::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!IsStringLike(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionIndex::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* str1 = MakeStringFromExpr(args[0], args[0]->Type());
	llvm::Value* str2 = MakeStringFromExpr(args[1], args[1]->Type());

	llvm::Type*          retTy = Types::Get<Types::IntegerDecl>()->LlvmType();
	llvm::FunctionCallee f = GetFunction(retTy, { str1->getType(), str2->getType() }, "__StrIndex");

	return builder.CreateCall(f, { str1, str2 }, "Index");
    }

    ErrorType FunctionIndex::Semantics()
    {
	if (args.size() != 2)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!IsStringLike(args[0]->Type()) || !IsStringLike(args[1]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionClock::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::FunctionCallee f = GetFunction(Types::Get<Types::Int64Decl>()->LlvmType(), {}, "__Clock");

	return builder.CreateCall(f, {}, "clock");
    }

    ErrorType FunctionClock::Semantics()
    {
	if (args.size() != 0)
	{
	    return ErrorType::WrongArgCount;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionPanic::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value*         message = args[0]->CodeGen();
	llvm::Type*          ty = message->getType();
	llvm::FunctionCallee f = GetFunction(Types::Get<Types::VoidDecl>()->LlvmType(), { ty }, "__Panic");

	return builder.CreateCall(f, { message });
    }

    ErrorType FunctionPanic::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!IsStringLike(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionPopcnt::CodeGen(llvm::IRBuilder<>& builder)
    {
	Types::TypeDecl* type = args[0]->Type();
	std::string      name = "llvm.ctpop.i";
	if (IsIntegral(type))
	{
	    name += std::to_string(type->Size() * 8);
	    llvm::Type*          ty = type->LlvmType();
	    llvm::FunctionCallee f = GetFunction(ty, { ty }, name);
	    llvm::Value*         a = args[0]->CodeGen();
	    return builder.CreateCall(f, a, "popcnt");
	}

	llvm::Type* intTy = Types::Get<Types::IntegerDecl>()->LlvmType();
	name += std::to_string(Types::SetDecl::SetBits);
	llvm::Value*         v = MakeAddressable(args[0]);
	llvm::Value*         addr = builder.CreateGEP(intTy, v, MakeIntegerConstant(0), "leftSet");
	llvm::Value*         val = builder.CreateLoad(intTy, addr);
	llvm::Type*          ty = val->getType();
	llvm::FunctionCallee f = GetFunction(ty, { ty }, name);
	llvm::Value*         count = builder.CreateCall(f, val, "count");
	auto                 sd = llvm::dyn_cast<Types::SetDecl>(type);
	for (size_t i = 1; i < sd->SetWords(); i++)
	{
	    addr = builder.CreateGEP(intTy, v, MakeIntegerConstant(i), "leftSet");
	    val = builder.CreateLoad(intTy, addr);
	    llvm::Value* tmp = builder.CreateCall(f, val, "tmp");
	    count = builder.CreateAdd(count, tmp, "count");
	}
	return count;
    }

    ErrorType FunctionPopcnt::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!IsIntegral(args[0]->Type()) && !llvm::isa<Types::SetDecl>(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionCycles::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::FunctionCallee f = GetFunction(Types::Get<Types::Int64Decl>()->LlvmType(), {},
	                                     "llvm.readcyclecounter");

	return builder.CreateCall(f, {}, "cycles");
    }

    llvm::Value* FunctionFloat2Arg::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Type*  realTy = Types::Get<Types::RealDecl>()->LlvmType();
	llvm::Value* a = args[0]->CodeGen();
	llvm::Value* b = args[1]->CodeGen();

	llvm::FunctionCallee f = GetFunction(realTy, { realTy, realTy }, func);

	return builder.CreateCall(f, { a, b });
    }

    ErrorType FunctionFloat2Arg::Semantics()
    {
	if (args.size() != 2)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!CastIntegerToReal(args[0]) || !CastIntegerToReal(args[1]))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionParamstr::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* n = args[0]->CodeGen();

	std::vector<llvm::Type*> argTypes = { n->getType() };

	llvm::FunctionType*  ft = llvm::FunctionType::get(Type()->LlvmType(), argTypes, false);
	llvm::FunctionCallee f = theModule->getOrInsertFunction("__ParamStr", ft);

	return builder.CreateCall(f, n, "paramstr");
    }

    ErrorType FunctionParamstr::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!IsIntegral(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionParamcount::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::FunctionCallee f = GetFunction(Types::Get<Types::IntegerDecl>()->LlvmType(), {},
	                                     "__ParamCount");
	return builder.CreateCall(f, {}, "paramcount");
    }

    ErrorType FunctionParamcount::Semantics()
    {
	if (args.size() != 0)
	{
	    return ErrorType::WrongArgCount;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionMax::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (llvm::isa<Types::RealDecl>(args[0]->Type()) || llvm::isa<Types::RealDecl>(args[1]->Type()))
	{
	    FunctionFloat2Arg max("llvm.maxnum.f64", args);
	    return max.CodeGen(builder);
	}

	llvm::Value* a = args[0]->CodeGen();
	llvm::Value* b = args[1]->CodeGen();
	llvm::Value* sel;
	if (IsUnsigned(args[0]->Type()) || IsUnsigned(args[1]->Type()))
	{
	    sel = builder.CreateICmpUGT(a, b, "sel");
	}
	else
	{
	    sel = builder.CreateICmpSGT(a, b, "sel");
	}
	return builder.CreateSelect(sel, a, b, "max");
    }

    llvm::Value* FunctionMin::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (llvm::isa<Types::RealDecl>(args[0]->Type()) || llvm::isa<Types::RealDecl>(args[1]->Type()))
	{
	    FunctionFloat2Arg min("llvm.minnum.f64", args);
	    return min.CodeGen(builder);
	}
	llvm::Value* a = args[0]->CodeGen();
	llvm::Value* b = args[1]->CodeGen();
	llvm::Value* sel;
	if (IsUnsigned(args[0]->Type()) || IsUnsigned(args[1]->Type()))
	{
	    sel = builder.CreateICmpULT(a, b, "sel");
	}
	else
	{
	    sel = builder.CreateICmpSLT(a, b, "sel");
	}
	return builder.CreateSelect(sel, a, b, "min");
    }

    ErrorType FunctionSign::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!Types::IsNumeric(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionSign::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* v = args[0]->CodeGen();
	llvm::Value* zero = MakeIntegerConstant(0);
	llvm::Value* one = MakeIntegerConstant(1);
	llvm::Value* mone = MakeIntegerConstant(-1);
	if (llvm::isa<Types::RealDecl>(args[0]->Type()))
	{
	    llvm::Value* fzero = llvm::ConstantFP::get(theContext, llvm::APFloat(0.0));
	    llvm::Value* sel1 = builder.CreateFCmpOGT(v, fzero, "gt");
	    llvm::Value* sel2 = builder.CreateFCmpOLT(v, fzero, "lt");
	    llvm::Value* res = builder.CreateSelect(sel1, one, zero, "sgn1");
	    return builder.CreateSelect(sel2, mone, res, "sgn2");
	}
	if (IsUnsigned(args[0]->Type()))
	{
	    llvm::Value* sel1 = builder.CreateICmpUGT(v, zero, "gt");
	    return builder.CreateSelect(sel1, one, zero, "sgn1");
	}
	llvm::Value* sel1 = builder.CreateICmpSGT(v, zero, "gt");
	llvm::Value* sel2 = builder.CreateICmpSLT(v, zero, "lt");
	llvm::Value* res = builder.CreateSelect(sel1, one, zero, "sgn1");
	return builder.CreateSelect(sel2, mone, res, "sgn2");
    }

    ErrorType FunctionGetTimeStamp::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (args[0]->Type() != Types::GetTimeStampType() || !llvm::isa<AddressableAST>(args[0]))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionGetTimeStamp::CodeGen(llvm::IRBuilder<>& builder)
    {
	auto         ts = llvm::dyn_cast<AddressableAST>(args[0]);
	llvm::Value* tsaddr = ts->Address();

	llvm::FunctionCallee f = GetFunction(Types::Get<Types::VoidDecl>()->LlvmType(), { tsaddr->getType() },
	                                     "__gettimestamp");
	return builder.CreateCall(f, { tsaddr });
    }

    FunctionTime::FunctionTime(const std::string& fn, ArgList& a) : FunctionBase(fn, a)
    {
	arrayType = new Types::ArrayDecl(
	    Types::Get<Types::CharDecl>(),
	    { new Types::RangeDecl(new Types::Range(1, 9), Types::Get<Types::IntegerDecl>()) });
    }

    ErrorType FunctionTime::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (args[0]->Type() != Types::GetTimeStampType() || !llvm::isa<AddressableAST>(args[0]))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionTime::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* storage = CreateTempAlloca(arrayType);
	auto         ts = llvm::dyn_cast<AddressableAST>(args[0]);
	llvm::Value* tsaddr = ts->Address();
	llvm::Type*  charTy = Types::Get<Types::CharDecl>()->LlvmType();

	llvm::FunctionCallee f = GetFunction(Types::Get<Types::VoidDecl>()->LlvmType(),
	                                     { tsaddr->getType(), storage->getType() }, "__Time");
	builder.CreateCall(f, { tsaddr, storage });
	llvm::Value* addr = builder.CreateGEP(charTy, storage, MakeIntegerConstant(0), "addr");

	return addr;
    }

    FunctionDate::FunctionDate(const std::string& fn, ArgList& a) : FunctionBase(fn, a)
    {
	arrayType = new Types::ArrayDecl(
	    Types::Get<Types::CharDecl>(),
	    { new Types::RangeDecl(new Types::Range(1, 11), Types::Get<Types::IntegerDecl>()) });
    }

    ErrorType FunctionDate::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (args[0]->Type() != Types::GetTimeStampType() || !llvm::isa<AddressableAST>(args[0]))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionDate::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* storage = CreateTempAlloca(arrayType);
	auto         ts = llvm::dyn_cast<AddressableAST>(args[0]);
	llvm::Value* tsaddr = ts->Address();
	llvm::Type*  charTy = Types::Get<Types::CharDecl>()->LlvmType();

	llvm::FunctionCallee f = GetFunction(Types::Get<Types::VoidDecl>()->LlvmType(),
	                                     { tsaddr->getType(), storage->getType() }, "__Date");
	builder.CreateCall(f, { tsaddr, storage });
	llvm::Value* addr = builder.CreateGEP(charTy, storage, MakeIntegerConstant(0), "addr");

	return addr;
    }

    ErrorType FunctionBind::Semantics()
    {
	if (args.size() != 2)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!llvm::isa<Types::FileDecl>(args[0]->Type()) || args[1]->Type() != Types::GetBindingType() ||
	    !llvm::isa<AddressableAST>(args[0]))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionBind::CodeGen(llvm::IRBuilder<>& builder)
    {
	auto fvar = llvm::dyn_cast<AddressableAST>(args[0]);
	ICE_IF(!fvar, "Should be a variable here");
	llvm::Value*             faddr = fvar->Address();
	llvm::Value*             binding = args[1]->CodeGen();
	std::vector<llvm::Type*> argTypes = { faddr->getType(), Types::GetBindingType()->LlvmType() };
	llvm::FunctionCallee     f = GetFunction(Type()->LlvmType(), argTypes, "__" + name);

	return builder.CreateCall(f, { faddr, binding });
    }

    ErrorType FunctionStrCompOp::Semantics()
    {
	if (args.size() != 2)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!IsStringLike(args[0]->Type()) || !IsStringLike(args[1]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionStrCompOp::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* v = CallStrFunc("Compare", args[0], args[1], Types::Get<Types::IntegerDecl>(), "cmp");
	return MakeStrCompare(op, v);
    }

    ErrorType FunctionSeek::Semantics()
    {
	if (args.size() != 2)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!llvm::isa<Types::FileDecl>(args[0]->Type()) || !IsIntegral(args[1]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionSeek::CodeGen(llvm::IRBuilder<>& builder)
    {
	auto fvar = llvm::dyn_cast<AddressableAST>(args[0]);
	ICE_IF(!fvar, "Should be a variable here");
	llvm::Value* faddr = fvar->Address();
	llvm::Value* pos = Recast(args[1], Types::Get<Types::Int64Decl>())->CodeGen();

	llvm::FunctionCallee f = GetFunction(Types::Get<Types::VoidDecl>()->LlvmType(),
	                                     { faddr->getType(), Types::Get<Types::Int64Decl>()->LlvmType() },
	                                     "__" + name);
	return builder.CreateCall(f, { faddr, pos });
    }

    ErrorType FunctionComplex::Semantics()
    {
	if (args.size() != 2)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!CastIntegerToReal(args[0]) || !CastIntegerToReal(args[1]))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionComplex::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* storage = CreateTempAlloca(Types::Get<Types::ComplexDecl>());
	llvm::Value* re = args[0]->CodeGen();
	llvm::Value* im = args[1]->CodeGen();
	llvm::Type*  cmplxTy = Types::Get<Types::ComplexDecl>()->LlvmType();

	llvm::Value* zero = MakeIntegerConstant(0);
	llvm::Value* one = MakeIntegerConstant(1);

	builder.CreateStore(re, builder.CreateGEP(cmplxTy, storage, { zero, zero }, "re"));
	builder.CreateStore(im, builder.CreateGEP(cmplxTy, storage, { zero, one }, "im"));

	return builder.CreateLoad(cmplxTy, storage, "cmplx");
    }

    llvm::Value* FunctionPolar::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* re = args[0]->CodeGen();
	llvm::Value* im = args[1]->CodeGen();
	llvm::Type*  realTy = Types::Get<Types::RealDecl>()->LlvmType();

	llvm::Type*  cplxTy = Types::Get<Types::ComplexDecl>()->LlvmType();
	llvm::Type*  pCplxTy = llvm::PointerType::getUnqual(cplxTy);
	llvm::Value* res = CreateTempAlloca(Types::Get<Types::ComplexDecl>());

	llvm::FunctionCallee f = GetFunction(Types::Get<Types::VoidDecl>()->LlvmType(),
	                                     { pCplxTy, realTy, realTy }, "__cpolar");

	builder.CreateCall(f, { res, re, im });
	return builder.CreateLoad(cplxTy, res);
    }

    ErrorType FunctionReIm::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!llvm::isa<Types::ComplexDecl>(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionReIm::CodeGen(llvm::IRBuilder<>& builder)
    {
	auto         cmplx = llvm::dyn_cast<AddressableAST>(args[0]);
	llvm::Value* caddr = cmplx->Address();
	llvm::Type*  realTy = Types::Get<Types::RealDecl>()->LlvmType();

	return builder.CreateLoad(realTy,
	                          builder.CreateGEP(realTy, caddr, MakeIntegerConstant(index), "reim"));
    }

    ErrorType FunctionCmplxToReal::Semantics()
    {
	if (args.size() != 1)
	{
	    return ErrorType::WrongArgCount;
	}
	if (!llvm::isa<Types::ComplexDecl>(args[0]->Type()))
	{
	    return ErrorType::WrongArgType;
	}
	return ErrorType::Ok;
    }

    llvm::Value* FunctionCmplxToReal::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value*         c = args[0]->CodeGen();
	llvm::FunctionCallee f = GetFunction(Type()->LlvmType(),
	                                     { Types::Get<Types::ComplexDecl>()->LlvmType() }, "__c" + name);
	return builder.CreateCall(f, { c });
    }

    void AddBIFCreator(const std::string& name, CreateBIFObject createFunc)
    {
	ICE_IF(BIFMap.find(name) != BIFMap.end(), "Already registered function");
	BIFMap[name] = createFunc;
    }

    bool IsBuiltin(std::string name)
    {
	strlower(name);
	return BIFMap.find(name) != BIFMap.end();
    }

    FunctionBase* CreateBuiltinFunction(std::string name, ArgList& args)
    {
	strlower(name);
	auto it = BIFMap.find(name);
	if (it != BIFMap.end())
	{
	    return it->second(name, args);
	}
	ICE("Builtin function should be found here");
    }

#define NEW(name)                                                                                            \
    [](const std::string& nm, ArgList& a) -> FunctionBase* { return new Function##name(nm, a); }
#define NEW2(name, extra)                                                                                    \
    [](const std::string& nm, ArgList& a) -> FunctionBase* { return new Function##name(nm, a, extra); }

    void InitBuiltins()
    {
	AddBIFCreator("abs", NEW(Abs));
	AddBIFCreator("odd", NEW(Odd));
	AddBIFCreator("sqr", NEW(Sqr));
	AddBIFCreator("round", NEW(Round));
	AddBIFCreator("trunc", NEW(Trunc));
	AddBIFCreator("random", NEW(Random));
	AddBIFCreator("chr", NEW(Chr));
	AddBIFCreator("ord", NEW(Ord));
	AddBIFCreator("succ", NEW(Succ));
	AddBIFCreator("pred", NEW(Pred));
	AddBIFCreator("new", NEW(New));
	AddBIFCreator("dispose", NEW(Dispose));
	AddBIFCreator("halt", NEW(Halt));
	AddBIFCreator("length", NEW(Length));
	AddBIFCreator("popcnt", NEW(Popcnt));
	AddBIFCreator("card", NEW(Popcnt));
	AddBIFCreator("assign", NEW(Assign));
	AddBIFCreator("panic", NEW(Panic));
	AddBIFCreator("clock", NEW(Clock));
	AddBIFCreator("cycles", NEW(Cycles));
	AddBIFCreator("paramcount", NEW(Paramcount));
	AddBIFCreator("paramstr", NEW(Paramstr));
	AddBIFCreator("copy", NEW(Copy));
	AddBIFCreator("substr", NEW(Copy));
	AddBIFCreator("trim", NEW(Trim));
	AddBIFCreator("index", NEW(Index));
	AddBIFCreator("max", NEW(Max));
	AddBIFCreator("min", NEW(Min));
	AddBIFCreator("sign", NEW(Sign));
	AddBIFCreator("inc", NEW(Inc));
	AddBIFCreator("dec", NEW(Dec));
	AddBIFCreator("pack", NEW(Pack));
	AddBIFCreator("unpack", NEW(Unpack));
	AddBIFCreator("val", NEW(Val));
	AddBIFCreator("sqrt", NEW(FloatIntrinsic));
	AddBIFCreator("sin", NEW(FloatIntrinsic));
	AddBIFCreator("cos", NEW(FloatIntrinsic));
	AddBIFCreator("ln", NEW2(FloatIntrinsic, "log"));
	AddBIFCreator("exp", NEW(FloatIntrinsic));
	AddBIFCreator("arctan", NEW2(Float, "atan"));
	AddBIFCreator("tan", NEW(Float));
	AddBIFCreator("arctan2", NEW2(Float2Arg, "atan2"));
	AddBIFCreator("fmod", NEW(Float2Arg));
	AddBIFCreator("reset", NEW(FileInfo));
	AddBIFCreator("page", NEW(File));
	AddBIFCreator("rewrite", NEW(FileInfo));
	AddBIFCreator("append", NEW(FileInfo));
	AddBIFCreator("close", NEW(File));
	AddBIFCreator("get", NEW(File));
	AddBIFCreator("put", NEW(File));
	AddBIFCreator("eof", NEW(FileBool));
	AddBIFCreator("eoln", NEW(FileBool));
	AddBIFCreator("gettimestamp", NEW(GetTimeStamp));
	AddBIFCreator("time", NEW(Time));
	AddBIFCreator("date", NEW(Date));
	AddBIFCreator("unbind", NEW(File));
	AddBIFCreator("binding", NEW(Binding));
	AddBIFCreator("bind", NEW(Bind));
	AddBIFCreator("seekwrite", NEW(Seek));
	AddBIFCreator("seekread", NEW(Seek));
	AddBIFCreator("seekupdate", NEW(Seek));
	AddBIFCreator("empty", NEW(FileBool));
	AddBIFCreator("position", NEW(FileLong));
	AddBIFCreator("lastposition", NEW(FileLong));
	AddBIFCreator("eq", NEW2(StrCompOp, Token::Equal));
	AddBIFCreator("ne", NEW2(StrCompOp, Token::NotEqual));
	AddBIFCreator("le", NEW2(StrCompOp, Token::LessOrEqual));
	AddBIFCreator("ge", NEW2(StrCompOp, Token::GreaterOrEqual));
	AddBIFCreator("lt", NEW2(StrCompOp, Token::LessThan));
	AddBIFCreator("gt", NEW2(StrCompOp, Token::GreaterThan));
	AddBIFCreator("cmplx", NEW(Complex));
	AddBIFCreator("re", NEW2(ReIm, 0));
	AddBIFCreator("im", NEW2(ReIm, 1));
	AddBIFCreator("arg", NEW(CmplxToReal));
	AddBIFCreator("polar", NEW(Polar));
	AddBIFCreator("frac", NEW2(Float, "__frac"));
	AddBIFCreator("int", NEW(IntConvert));
    }
} // namespace Builtin
