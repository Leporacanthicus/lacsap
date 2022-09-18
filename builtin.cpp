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
	if (arg->Type()->Type() != Types::TypeDecl::TK_Real)
	{
	    // Implicit typecast.
	    if (!arg->Type()->IsIntegral())
	    {
		return false;
	    }
	    arg = Recast(arg, Types::GetRealType());
	}
	return true;
    }

    using ArgList = const std::vector<ExprAST*>;
    using CreateBIFObject = std::function<FunctionBase*(const std::vector<ExprAST*>&)>;

    std::map<std::string, CreateBIFObject>                                    BIFMap;

    static llvm::Value* CallRuntimeFPFunc(llvm::IRBuilder<>& builder, const std::string& func,
                                          const std::vector<ExprAST*>& args)
    {
	llvm::Value*         a = args[0]->CodeGen();
	llvm::Type*          ty = Types::GetRealType()->LlvmType();
	llvm::FunctionCallee f = GetFunction(ty, { ty }, func);
	return builder.CreateCall(f, a, "calltmp");
    }

    class FunctionSameAsArg : public FunctionBase
    {
    public:
	using FunctionBase::FunctionBase;
	Types::TypeDecl* Type() const override { return args[0]->Type(); }
	bool             Semantics() override;
    };

    class FunctionSameAsArg2 : public FunctionBase
    {
    public:
	using FunctionBase::FunctionBase;
	Types::TypeDecl* Type() const override { return args[0]->Type(); }
	bool             Semantics() override;
    };

    class FunctionInt : public FunctionBase
    {
    public:
	using FunctionBase::FunctionBase;
	Types::TypeDecl* Type() const override { return Types::GetIntegerType(); }
    };

    class FunctionAbs : public FunctionSameAsArg
    {
    public:
	using FunctionSameAsArg::FunctionSameAsArg;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionSqr : public FunctionSameAsArg
    {
    public:
	using FunctionSameAsArg::FunctionSameAsArg;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	bool         Semantics() override;
    };

    class FunctionOdd : public FunctionBase
    {
    public:
	using FunctionBase::FunctionBase;
	llvm::Value*     CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return Types::GetBooleanType(); }
	bool             Semantics() override;
    };

    class FunctionRound : public FunctionInt
    {
    public:
	using FunctionInt::FunctionInt;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	bool         Semantics() override;
    };

    class FunctionTrunc : public FunctionRound
    {
    public:
	using FunctionRound::FunctionRound;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionRandom : public FunctionBase
    {
    public:
	using FunctionBase::FunctionBase;
	llvm::Value*     CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return Types::GetRealType(); }
	bool             Semantics() override;
    };

    class FunctionChr : public FunctionBase
    {
    public:
	using FunctionBase::FunctionBase;
	llvm::Value*     CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return Types::GetCharType(); }
	bool             Semantics() override;
    };

    class FunctionOrd : public FunctionInt
    {
    public:
	using FunctionInt::FunctionInt;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	bool         Semantics() override;
    };

    class FunctionLength : public FunctionInt
    {
    public:
	using FunctionInt::FunctionInt;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	bool         Semantics() override;
    };

    class FunctionPopcnt : public FunctionInt
    {
    public:
	using FunctionInt::FunctionInt;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	bool         Semantics() override;
    };

    class FunctionSucc : public FunctionSameAsArg
    {
    public:
	using FunctionSameAsArg::FunctionSameAsArg;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	bool         Semantics() override;
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
	FunctionFloat(const std::string& fn, const std::vector<ExprAST*>& a) : FunctionBase(a), funcname(fn)
	{
	}
	llvm::Value*     CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return Types::GetRealType(); }
	bool             Semantics() override;

    protected:
	std::string funcname;
    };

    class FunctionFloat2Arg : public FunctionFloat
    {
    public:
	using FunctionFloat::FunctionFloat;
	llvm::Value*     CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return Types::GetRealType(); }
	bool             Semantics() override;
    };

    class FunctionFloatIntrinsic : public FunctionFloat
    {
    public:
	FunctionFloatIntrinsic(const std::string& fn, const std::vector<ExprAST*>& a)
	    : FunctionFloat("llvm." + fn + ".f64", a)
	{
	}
    };

    class FunctionVoid : public FunctionBase
    {
    public:
	using FunctionBase::FunctionBase;
	Types::TypeDecl* Type() const override { return Types::GetVoidType(); }
    };

    class FunctionNew : public FunctionVoid
    {
    public:
	using FunctionVoid::FunctionVoid;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	bool         Semantics() override;
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
	bool         Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionInc : public FunctionVoid
    {
    public:
	using FunctionVoid::FunctionVoid;
	bool         Semantics() override;
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
	bool         Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionUnpack : public FunctionVoid
    {
    public:
	using FunctionVoid::FunctionVoid;
	bool         Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionVal : public FunctionVoid
    {
    public:
	using FunctionVoid::FunctionVoid;
	bool         Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionFile : public FunctionVoid
    {
    public:
	FunctionFile(const std::string& fn, const std::vector<ExprAST*>& a) : FunctionVoid(a), funcname(fn) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	bool         Semantics() override;

    protected:
	std::string funcname;
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
	bool             Semantics() override;
	Types::TypeDecl* Type() const override { return Types::GetBooleanType(); }
    };

    class FunctionAssign : public FunctionVoid
    {
    public:
	using FunctionVoid::FunctionVoid;
	bool         Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionPanic : public FunctionVoid
    {
    public:
	using FunctionVoid::FunctionVoid;
	bool         Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    class FunctionLongInt : public FunctionBase
    {
    public:
	using FunctionBase::FunctionBase;
	Types::TypeDecl* Type() const override { return Types::GetLongIntType(); }
    };

    class FunctionClock : public FunctionLongInt
    {
    public:
	using FunctionLongInt::FunctionLongInt;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	bool         Semantics() override;
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
	bool         Semantics() override;
    };

    class FunctionParamstr : public FunctionBase
    {
    public:
	using FunctionBase::FunctionBase;
	llvm::Value*     CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return Types::GetStringType(); }
	bool             Semantics() override;
    };

    class FunctionCopy : public FunctionBase
    {
    public:
	using FunctionBase::FunctionBase;
	llvm::Value*     CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override { return Types::GetStringType(); }
	bool             Semantics() override;
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
	bool         Semantics() override;
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
    };

    void FunctionBase::accept(ASTVisitor& v)
    {
	for (auto a : args)
	{
	    a->accept(v);
	}
    }

    bool FunctionSameAsArg::Semantics()
    {
	return (args.size() == 1) && (args[0]->Type()->Type() != Types::TypeDecl::TK_Char) &&
	       (args[0]->Type()->Type() != Types::TypeDecl::TK_Enum) &&
	       (args[0]->Type()->IsIntegral() || args[0]->Type()->Type() == Types::TypeDecl::TK_Real);
    }

    bool FunctionSameAsArg2::Semantics()
    {
	return (args.size() == 2) &&
	       ((args[0]->Type()->IsIntegral()) || args[0]->Type()->Type() == Types::TypeDecl::TK_Real) &&
	       ((args[1]->Type()->IsIntegral()) || args[1]->Type()->Type() == Types::TypeDecl::TK_Real);
    }

    llvm::Value* FunctionAbs::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	if (args[0]->Type()->IsUnsigned())
	{
	    return a;
	}
	if (args[0]->Type()->IsIntegral())
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
	return builder.CreateTrunc(v, Types::GetBooleanType()->LlvmType(), "odd");
    }

    bool FunctionOdd::Semantics() { return args.size() == 1 && args[0]->Type()->IsIntegral(); }

    llvm::Value* FunctionSqr::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	if (args[0]->Type()->IsIntegral())
	{
	    return builder.CreateMul(a, a, "sqr");
	}
	return builder.CreateFMul(a, a, "sqr");
    }

    bool FunctionSqr::Semantics()
    {
	return args.size() == 1 && args[0]->Type()->Type() != Types::TypeDecl::TK_Char &&
	       args[0]->Type()->Type() != Types::TypeDecl::TK_Enum &&
	       (args[0]->Type()->IsIntegral() || args[0]->Type()->Type() == Types::TypeDecl::TK_Real);
    }

    llvm::Value* FunctionFloat::CodeGen(llvm::IRBuilder<>& builder)
    {
	return CallRuntimeFPFunc(builder, funcname, args);
    }

    bool FunctionFloat::Semantics() { return args.size() == 1 && CastIntegerToReal(args[0]); }

    llvm::Value* FunctionRound::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* v = CallRuntimeFPFunc(builder, "llvm.round.f64", args);
	return builder.CreateFPToSI(v, Types::GetIntegerType()->LlvmType(), "to.int");
    }

    bool FunctionRound::Semantics()
    {
	return args.size() == 1 && args[0]->Type()->Type() == Types::TypeDecl::TK_Real;
    }

    llvm::Value* FunctionTrunc::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* v = args[0]->CodeGen();
	return builder.CreateFPToSI(v, Types::GetIntegerType()->LlvmType(), "to.int");
    }

    llvm::Value* FunctionRandom::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::FunctionCallee f = GetFunction(Types::GetRealType()->LlvmType(), {}, "__random");
	return builder.CreateCall(f, {}, "calltmp");
    }

    bool FunctionRandom::Semantics() { return args.size() == 0; }

    llvm::Value* FunctionChr::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	return builder.CreateTrunc(a, Types::GetCharType()->LlvmType(), "chr");
    }

    bool FunctionChr::Semantics() { return args.size() == 1 && args[0]->Type()->IsIntegral(); }

    llvm::Value* FunctionOrd::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	return builder.CreateZExt(a, Types::GetIntegerType()->LlvmType(), "ord");
    }

    bool FunctionOrd::Semantics() { return args.size() == 1 && args[0]->Type()->IsIntegral(); }

    llvm::Value* FunctionSucc::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	return builder.CreateAdd(a, MakeConstant(1, args[0]->Type()), "succ");
    }

    bool FunctionSucc::Semantics() { return args.size() == 1 && args[0]->Type()->IsIntegral(); }

    llvm::Value* FunctionPred::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* a = args[0]->CodeGen();
	return builder.CreateSub(a, MakeConstant(1, args[0]->Type()), "pred");
    }

    llvm::Value* FunctionNew::CodeGen(llvm::IRBuilder<>& builder)
    {
	Types::PointerDecl* pd = llvm::dyn_cast<Types::PointerDecl>(args[0]->Type());
	size_t              size = pd->SubType()->Size();
	llvm::Type*         ty = Types::GetIntegerType()->LlvmType();

	// Result is "void *"
	llvm::Type*          resTy = Types::GetVoidPtrType();
	llvm::FunctionCallee f = GetFunction(resTy, { ty }, "__new");

	llvm::Value* retVal = builder.CreateCall(f, { MakeIntegerConstant(size) }, "new");

	VariableExprAST* var = llvm::dyn_cast<VariableExprAST>(args[0]);
	// TODO: Fix this to be a proper TypeCast...
	retVal = builder.CreateBitCast(retVal, pd->LlvmType(), "cast");
	llvm::Value* pA = var->Address();
	return builder.CreateStore(retVal, pA);
    }

    bool FunctionNew::Semantics()
    {
	return args.size() == 1 && llvm::isa<Types::PointerDecl>(args[0]->Type()) &&
	       llvm::isa<VariableExprAST>(args[0]);
    }

    llvm::Value* FunctionDispose::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Type*          ty = args[0]->Type()->LlvmType();
	llvm::FunctionCallee f = GetFunction(Types::GetVoidType()->LlvmType(), { ty }, "__dispose");

	return builder.CreateCall(f, { args[0]->CodeGen() });
    }

    bool FunctionHalt::Semantics()
    {
	return args.size() <= 1 && (args.size() == 0 || args[0]->Type()->IsIntegral());
    }

    llvm::Value* FunctionHalt::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* v = MakeIntegerConstant(0);
	if (args.size() == 1)
	{
	    v = args[0]->CodeGen();
	}

	llvm::FunctionCallee f = GetFunction(Types::GetVoidType()->LlvmType(),
	                                     { Types::GetIntegerType()->LlvmType() }, "exit");

	return builder.CreateCall(f, { v });
    }

    bool FunctionInc::Semantics()
    {
	return args.size() == 1 && args[0]->Type()->IsIntegral() && llvm::isa<VariableExprAST>(args[0]);
    }

    llvm::Value* FunctionInc::CodeGen(llvm::IRBuilder<>& builder)
    {
	VariableExprAST* var = llvm::dyn_cast<VariableExprAST>(args[0]);
	assert(var && "Expected variable here... Semantics not working?");
	llvm::Value* pA = var->Address();
	llvm::Value* a = builder.CreateLoad(pA, "inc");
	a = builder.CreateAdd(a, MakeConstant(1, var->Type()), "inc");
	return builder.CreateStore(a, pA);
    }

    llvm::Value* FunctionDec::CodeGen(llvm::IRBuilder<>& builder)
    {
	VariableExprAST* var = llvm::dyn_cast<VariableExprAST>(args[0]);
	assert(var && "Expected variable here... Semantics not working?");
	llvm::Value* pA = var->Address();
	llvm::Value* a = builder.CreateLoad(pA, "dec");
	a = builder.CreateSub(a, MakeConstant(1, var->Type()), "dec");
	return builder.CreateStore(a, pA);
    }

    // Pack(a, start, apacked);
    bool FunctionPack::Semantics()
    {
	if (args.size() != 3)
	{
	    return false;
	}
	Types::ArrayDecl* t0 = llvm::dyn_cast<Types::ArrayDecl>(args[0]->Type());
	Types::ArrayDecl* t2 = llvm::dyn_cast<Types::ArrayDecl>(args[2]->Type());
	if (t0 && t2)
	{
	    if (args[1]->Type()->IsIntegral() && args[1]->Type()->AssignableType(t0->Ranges()[0]))
	    {
		return t0->SubType() == t2->SubType() && llvm::isa<VariableExprAST>(args[0]) &&
		       t0->Ranges().size() == 1 && llvm::isa<VariableExprAST>(args[2]) &&
		       t2->Ranges().size() == 1;
	    }
	}
	return false;
    }

    llvm::Value* FunctionPack::CodeGen(llvm::IRBuilder<>& builder)
    {
	// Pack(X, n, Y) -> copy X to Y, starting at offset n
	VariableExprAST* var0 = llvm::dyn_cast<VariableExprAST>(args[0]);
	VariableExprAST* var2 = llvm::dyn_cast<VariableExprAST>(args[2]);

	llvm::Value*      start = args[1]->CodeGen();
	Types::ArrayDecl* ty0 = llvm::dyn_cast<Types::ArrayDecl>(args[0]->Type());
	if (ty0->Ranges()[0]->Start())
	{
	    start = builder.CreateSub(start, MakeConstant(ty0->Ranges()[0]->Start(), args[1]->Type()));
	}

	llvm::Value* pA = var0->Address();
	llvm::Value* pB = var2->Address();

	std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), start };
	llvm::Value*              src = builder.CreateGEP(pA, ind, "dest");
	llvm::Align               dest_align{ std::max(AlignOfType(pB->getType()), MIN_ALIGN) };
	llvm::Align               src_align{ std::max(AlignOfType(src->getType()), MIN_ALIGN) };
	return builder.CreateMemCpy(pB, dest_align, src, src_align, args[2]->Type()->Size());
    }

    // Unpack(apacked, a, start);
    bool FunctionUnpack::Semantics()
    {
	if (args.size() != 3)
	{
	    return false;
	}
	Types::ArrayDecl* t0 = llvm::dyn_cast<Types::ArrayDecl>(args[0]->Type());
	Types::ArrayDecl* t1 = llvm::dyn_cast<Types::ArrayDecl>(args[1]->Type());
	if (t0 && t1)
	{
	    if (args[2]->Type()->IsIntegral() && args[2]->Type()->AssignableType(t1->Ranges()[0]))
	    {
		return t0->SubType() == t1->SubType() && llvm::isa<VariableExprAST>(args[0]) &&
		       t0->Ranges().size() == 1 && llvm::isa<VariableExprAST>(args[1]) &&
		       t1->Ranges().size() == 1 && args[2]->Type()->IsIntegral();
	    }
	}
	return false;
    }

    llvm::Value* FunctionUnpack::CodeGen(llvm::IRBuilder<>& builder)
    {
	// Unpack(X, Y, n) -> copy X to Y, starting at offset n
	VariableExprAST* var0 = llvm::dyn_cast<VariableExprAST>(args[0]);
	VariableExprAST* var1 = llvm::dyn_cast<VariableExprAST>(args[1]);

	llvm::Value*      start = args[2]->CodeGen();
	Types::ArrayDecl* ty1 = llvm::dyn_cast<Types::ArrayDecl>(args[1]->Type());
	if (ty1->Ranges()[0]->Start())
	{
	    start = builder.CreateSub(start, MakeConstant(ty1->Ranges()[0]->Start(), args[2]->Type()));
	}

	llvm::Value* pA = var0->Address();
	llvm::Value* pB = var1->Address();

	std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), start };
	llvm::Value*              dest = builder.CreateGEP(pB, ind, "dest");
	llvm::Align               dest_align{ std::max(AlignOfType(dest->getType()), MIN_ALIGN) };
	llvm::Align               src_align{ std::max(AlignOfType(pA->getType()), MIN_ALIGN) };
	return builder.CreateMemCpy(dest, dest_align, pA, src_align, args[0]->Type()->Size());
    }

    bool FunctionVal::Semantics()
    {
	return args.size() == 2 && args[0]->Type()->IsStringLike() && llvm::isa<VariableExprAST>(args[1]) &&
	       (args[1]->Type()->Type() == Types::TypeDecl::TK_Integer ||
	        args[1]->Type()->Type() == Types::TypeDecl::TK_LongInt);
    }

    llvm::Value* FunctionVal::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value*     str = MakeStringFromExpr(args[0], args[0]->Type());
	VariableExprAST* var1 = llvm::dyn_cast<VariableExprAST>(args[1]);
	std::string      name = "__Val_";
	switch (var1->Type()->Type())
	{
	case Types::TypeDecl::TK_Integer:
	    name += "int";
	    break;
	case Types::TypeDecl::TK_LongInt:
	    name += "long";
	    break;
	default:
	    assert(0 && "What happened here?");
	    return 0;
	}
	llvm::Value*         res = var1->Address();
	llvm::Type*          ty0 = str->getType();
	llvm::Type*          ty1 = res->getType();
	llvm::FunctionCallee f = GetFunction(Types::GetVoidType()->LlvmType(), { ty0, ty1 }, name);

	return builder.CreateCall(f, { str, res });
    }

    llvm::Value* FunctionFile::CodeGen(llvm::IRBuilder<>& builder)
    {
	VariableExprAST* fvar = llvm::dyn_cast<VariableExprAST>(args[0]);
	assert(fvar && "Should be a variable here");
	llvm::Value*         faddr = fvar->Address();
	llvm::FunctionCallee f = GetFunction(Type()->LlvmType(), { faddr->getType() }, "__" + funcname);

	return builder.CreateCall(f, { faddr });
    }

    bool FunctionFile::Semantics()
    {
	if (args.size() != 1 || !llvm::isa<Types::FileDecl>(args[0]->Type()))
	{
	    return false;
	}
	if (!llvm::isa<VariableExprAST>(args[0]))
	{
	    return false;
	}
	return true;
    }

    llvm::Value* FunctionFileInfo::CodeGen(llvm::IRBuilder<>& builder)
    {
	VariableExprAST* fvar = llvm::dyn_cast<VariableExprAST>(args[0]);
	assert(fvar && "Should be a variable here");
	llvm::Value*             faddr = fvar->Address();
	std::vector<llvm::Type*> argTypes = { faddr->getType(), Types::GetIntegerType()->LlvmType(),
	                                      Types::GetBooleanType()->LlvmType() };
	llvm::FunctionCallee     f = GetFunction(Type()->LlvmType(), argTypes, "__" + funcname);

	Types::FileDecl* fd = llvm::dyn_cast<Types::FileDecl>(fvar->Type());
	llvm::Value*     sz = MakeIntegerConstant(fd->SubType()->Size());
	llvm::Value*     isText = MakeBooleanConstant(fd->Type() == Types::TypeDecl::TK_Text);

	std::vector<llvm::Value*> argList = { faddr, sz, isText };

	return builder.CreateCall(f, argList);
    }

    // Used for eof/eoln, where no argument means the "input" file.
    bool FunctionFileBool::Semantics()
    {
	if (args.size() == 0)
	{
	    args.push_back(new VariableExprAST(Location("", 0, 0), "input", Types::GetTextType()));
	    return true;
	}
	return FunctionFile::Semantics();
    }

    llvm::Value* FunctionLength::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value*              v = MakeAddressable(args[0]);
	std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), MakeIntegerConstant(0) };
	v = builder.CreateGEP(v, ind, "str_0");
	v = builder.CreateLoad(v, "len");

	return builder.CreateZExt(v, Types::GetIntegerType()->LlvmType(), "extend");
    }

    bool FunctionLength::Semantics()
    {
	return args.size() == 1 && args[0]->Type()->Type() == Types::TypeDecl::TK_String;
    }

    llvm::Value* FunctionAssign::CodeGen(llvm::IRBuilder<>& builder)
    {
	// assign takes two arguments from the user (file and filename), and a third "recordsize"
	// that we make up here, and a fourth for the "isText" argument.

	// Arg1: address of the filestruct.
	VariableExprAST* fvar = llvm::dyn_cast<VariableExprAST>(args[0]);
	llvm::Value*     faddr = fvar->Address();

	llvm::Value*             filename = args[1]->CodeGen();
	llvm::Type*              ty = filename->getType();
	std::vector<llvm::Type*> argTypes = { faddr->getType(), ty };

	llvm::FunctionCallee f = GetFunction(Types::GetVoidType()->LlvmType(), argTypes, "__assign");

	std::vector<llvm::Value*> argsV = { faddr, filename };
	return builder.CreateCall(f, argsV);
    }

    bool FunctionAssign::Semantics()
    {
	if (args.size() != 2 || !llvm::isa<VariableExprAST>(args[0]))
	{
	    return false;
	}
	if (!llvm::isa<Types::FileDecl>(args[0]->Type()) || !args[1]->Type()->IsStringLike())
	{
	    return false;
	}
	return true;
    }

    llvm::Value* FunctionCopy::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* str = MakeAddressable(args[0]);
	llvm::Value* start = args[1]->CodeGen();
	llvm::Value* len = args[2]->CodeGen();

	std::vector<llvm::Type*> argTypes = { str->getType(), start->getType(), len->getType() };
	llvm::Type*              strTy = Types::GetStringType()->LlvmType();
	llvm::FunctionCallee     f = GetFunction(strTy, argTypes, "__StrCopy");

	std::vector<llvm::Value*> argsV = { str, start, len };

	return builder.CreateCall(f, argsV, "copy");
    }

    bool FunctionCopy::Semantics()
    {
	return args.size() == 3 && args[0]->Type()->Type() == Types::TypeDecl::TK_String &&
	       args[1]->Type()->Type() == Types::TypeDecl::TK_Integer &&
	       args[2]->Type()->Type() == Types::TypeDecl::TK_Integer;
    }

    llvm::Value* FunctionClock::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::FunctionCallee f = GetFunction(Types::GetLongIntType()->LlvmType(), {}, "__Clock");

	return builder.CreateCall(f, {}, "clock");
    }

    bool FunctionClock::Semantics() { return args.size() == 0; }

    llvm::Value* FunctionPanic::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value*         message = args[0]->CodeGen();
	llvm::Type*          ty = message->getType();
	llvm::FunctionCallee f = GetFunction(Types::GetVoidType()->LlvmType(), { ty }, "__Panic");

	return builder.CreateCall(f, { message });
    }

    bool FunctionPanic::Semantics() { return args.size() == 1 && args[0]->Type()->IsStringLike(); }

    llvm::Value* FunctionPopcnt::CodeGen(llvm::IRBuilder<>& builder)
    {
	Types::TypeDecl* type = args[0]->Type();
	std::string      name = "llvm.ctpop.i";
	if (type->IsIntegral())
	{
	    name += std::to_string(type->Size() * 8);
	    llvm::Type*          ty = type->LlvmType();
	    llvm::FunctionCallee f = GetFunction(ty, { ty }, name);
	    llvm::Value*         a = args[0]->CodeGen();
	    return builder.CreateCall(f, a, "popcnt");
	}

	name += std::to_string(Types::SetDecl::SetBits);
	llvm::Value*              v = MakeAddressable(args[0]);
	std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), MakeIntegerConstant(0) };
	llvm::Value*              addr = builder.CreateGEP(v, ind, "leftSet");
	llvm::Value*              val = builder.CreateLoad(addr);
	llvm::Type*               ty = val->getType();
	llvm::FunctionCallee      f = GetFunction(ty, { ty }, name);
	llvm::Value*              count = builder.CreateCall(f, val, "count");
	Types::SetDecl*           sd = llvm::dyn_cast<Types::SetDecl>(type);
	for (size_t i = 1; i < sd->SetWords(); i++)
	{
	    std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), MakeIntegerConstant(i) };
	    addr = builder.CreateGEP(v, ind, "leftSet");
	    val = builder.CreateLoad(addr);
	    llvm::Value* tmp = builder.CreateCall(f, val, "tmp");
	    count = builder.CreateAdd(count, tmp, "count");
	}
	return count;
    }

    bool FunctionPopcnt::Semantics()
    {
	return args.size() == 1 &&
	       (args[0]->Type()->IsIntegral() || llvm::isa<Types::SetDecl>(args[0]->Type()));
    }

    llvm::Value* FunctionCycles::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::FunctionCallee f = GetFunction(Types::GetLongIntType()->LlvmType(), {},
	                                     "llvm.readcyclecounter");

	return builder.CreateCall(f, {}, "cycles");
    }

    llvm::Value* FunctionFloat2Arg::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Type*  realTy = Types::GetRealType()->LlvmType();
	llvm::Value* a = args[0]->CodeGen();
	llvm::Value* b = args[1]->CodeGen();

	llvm::FunctionCallee f = GetFunction(realTy, { realTy, realTy }, funcname);

	return builder.CreateCall(f, { a, b });
    }

    bool FunctionFloat2Arg::Semantics()
    {
	return args.size() == 2 && CastIntegerToReal(args[0]) && CastIntegerToReal(args[1]);
    }

    llvm::Value* FunctionParamstr::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* n = args[0]->CodeGen();

	std::vector<llvm::Type*> argTypes = { n->getType() };

	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetStringType()->LlvmType(), argTypes, false);
	llvm::FunctionCallee f = theModule->getOrInsertFunction("__ParamStr", ft);

	return builder.CreateCall(f, n, "paramstr");
    }

    bool FunctionParamstr::Semantics() { return args.size() == 1 && args[0]->Type()->IsIntegral(); }

    llvm::Value* FunctionParamcount::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::FunctionCallee f = GetFunction(Types::GetIntegerType()->LlvmType(), {}, "__ParamCount");
	return builder.CreateCall(f, {}, "paramcount");
    }

    bool FunctionParamcount::Semantics() { return args.size() == 0; }

    llvm::Value* FunctionMax::CodeGen(llvm::IRBuilder<>& builder)
    {
	if (args[0]->Type()->Type() == Types::TypeDecl::TK_Real ||
	    args[1]->Type()->Type() == Types::TypeDecl::TK_Real)
	{
	    FunctionFloat2Arg max("llvm.maxnum.f64", args);
	    return max.CodeGen(builder);
	}

	llvm::Value* a = args[0]->CodeGen();
	llvm::Value* b = args[1]->CodeGen();
	llvm::Value* sel;
	if (args[0]->Type()->IsUnsigned() || args[1]->Type()->IsUnsigned())
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
	if (args[0]->Type()->Type() == Types::TypeDecl::TK_Real ||
	    args[1]->Type()->Type() == Types::TypeDecl::TK_Real)
	{
	    FunctionFloat2Arg min("llvm.minnum.f64", args);
	    return min.CodeGen(builder);
	}
	llvm::Value* a = args[0]->CodeGen();
	llvm::Value* b = args[1]->CodeGen();
	llvm::Value* sel;
	if (args[0]->Type()->IsUnsigned() || args[1]->Type()->IsUnsigned())
	{
	    sel = builder.CreateICmpULT(a, b, "sel");
	}
	else
	{
	    sel = builder.CreateICmpSLT(a, b, "sel");
	}
	return builder.CreateSelect(sel, a, b, "min");
    }

    bool FunctionSign::Semantics()
    {
	return args[0]->Type()->IsIntegral() || args[0]->Type()->Type() == Types::TypeDecl::TK_Real;
    }

    llvm::Value* FunctionSign::CodeGen(llvm::IRBuilder<>& builder)
    {
	llvm::Value* v = args[0]->CodeGen();
	llvm::Value* zero = MakeIntegerConstant(0);
	llvm::Value* one = MakeIntegerConstant(1);
	llvm::Value* mone = MakeIntegerConstant(-1);
	if (args[0]->Type()->Type() == Types::TypeDecl::TK_Real)
	{
	    llvm::Value* fzero = llvm::ConstantFP::get(theContext, llvm::APFloat(0.0));
	    llvm::Value* sel1 = builder.CreateFCmpOGT(v, fzero, "gt");
	    llvm::Value* sel2 = builder.CreateFCmpOLT(v, fzero, "lt");
	    llvm::Value* res = builder.CreateSelect(sel1, one, zero, "sgn1");
	    return builder.CreateSelect(sel2, mone, res, "sgn2");
	}
	if (args[0]->Type()->IsUnsigned())
	{
	    llvm::Value* sel1 = builder.CreateICmpUGT(v, zero, "gt");
	    return builder.CreateSelect(sel1, one, zero, "sgn1");
	}
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
	strlower(name);
	return BIFMap.find(name) != BIFMap.end();
    }

    FunctionBase* CreateBuiltinFunction(std::string name, std::vector<ExprAST*>& args)
    {
	strlower(name);
	auto it = BIFMap.find(name);
	if (it != BIFMap.end())
	{
	    return it->second(args);
	}
	return 0;
    }

#define NEW(name) [](ArgList& a) -> FunctionBase* { return new Function##name(a); }
#define NEW2(name, func) [](ArgList& a) -> FunctionBase* { return new Function##name(func, a); }

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
	AddBIFCreator("max", NEW(Max));
	AddBIFCreator("min", NEW(Min));
	AddBIFCreator("sign", NEW(Sign));
	AddBIFCreator("inc", NEW(Inc));
	AddBIFCreator("dec", NEW(Dec));
	AddBIFCreator("pack", NEW(Pack));
	AddBIFCreator("unpack", NEW(Unpack));
	AddBIFCreator("val", NEW(Val));
	AddBIFCreator("sqrt", NEW2(FloatIntrinsic, "sqrt"));
	AddBIFCreator("sin", NEW2(FloatIntrinsic, "sin"));
	AddBIFCreator("cos", NEW2(FloatIntrinsic, "cos"));
	AddBIFCreator("ln", NEW2(FloatIntrinsic, "log"));
	AddBIFCreator("exp", NEW2(FloatIntrinsic, "exp"));
	AddBIFCreator("arctan", NEW2(Float, "atan"));
	AddBIFCreator("tan", NEW2(Float, "tan"));
	AddBIFCreator("arctan2", NEW2(Float2Arg, "atan2"));
	AddBIFCreator("fmod", NEW2(Float2Arg, "fmod"));
	AddBIFCreator("reset", NEW2(FileInfo, "reset"));
	AddBIFCreator("rewrite", NEW2(FileInfo, "rewrite"));
	AddBIFCreator("append", NEW2(FileInfo, "append"));
	AddBIFCreator("close", NEW2(File, "close"));
	AddBIFCreator("get", NEW2(File, "get"));
	AddBIFCreator("put", NEW2(File, "put"));
	AddBIFCreator("eof", NEW2(FileBool, "eof"));
	AddBIFCreator("eoln", NEW2(FileBool, "eoln"));
    }
} // namespace Builtin
