#include "expr.h"
#include "builtin.h"
#include <llvm/IR/DataLayout.h>

extern llvm::Module* theModule;


namespace Builtin
{
    typedef BuiltinFunctionBase* (*CreateBIFObject)(const std::vector<ExprAST*>& a);
    std::map<std::string, CreateBIFObject> BIFMap;

    /* TODO: Remove this Old style functionalit */
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

    static llvm::Value* CallBuiltinFunc(llvm::IRBuilder<>& builder,
					const std::string& func,
					const std::vector<ExprAST*>& args,
					llvm::Type* resTy = 0)
    {
	std::string name = "llvm." + func + ".f64";
	return CallRuntimeFPFunc(builder, name, args, resTy);
    }


    class BuiltinFunctionAbs : public BuiltinFunctionBase
    {
    public:
	BuiltinFunctionAbs(const std::vector<ExprAST*>& a)
	    : BuiltinFunctionBase(a) {}
	llvm::Value* CodeGen(llvm::IRBuilder<>& builder) override;
	Types::TypeDecl* Type() const override;
	virtual bool Semantics() const override;
    };

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
	    return CallBuiltinFunc(builder, "fabs", args);
	}
	return ErrorV("Expected type of real or integer for 'abs'");
    }

    Types::TypeDecl* BuiltinFunctionAbs::Type() const
    {
	return args[0]->Type();
    }

    bool BuiltinFunctionAbs::Semantics() const
    {
	return false;
    }

    BuiltinFunctionBase* CreateAbs(const std::vector<ExprAST*>& a)
    {
	return new BuiltinFunctionAbs(a);
    }


    void AddBIFCreator(const std::string& name, CreateBIFObject createFunc)
    {
	assert(BIFMap.find(name) == BIFMap.end() && "Already registered function");
	BIFMap[name] = createFunc;
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

	llvm::Value* v = CallBuiltinFunc(builder, "round", args);
	return builder.CreateFPToSI(v, Types::GetType(Types::Integer), "to.int");
    }

    static llvm::Value* TruncCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
    {
	assert(args.size() == 1 && "Expect 1 argument to trunc");

	llvm::Value* v = args[0]->CodeGen();
	return builder.CreateFPToSI(v, Types::GetType(Types::Integer), "to.int");
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

    static llvm::Value* DisposeCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
    {
	assert(args.size() == 1 && "Expect 1 argument to 'dispos'");

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

    static llvm::Value* AssignCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
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


    static llvm::Value* FileCallCodeGen(llvm::IRBuilder<>& builder,
					const std::vector<ExprAST*>& args,
					const std::string func,
					Types::SimpleTypes resTy = Types::Void)
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
	llvm::Value* faddr =fvar->Address();
	std::vector<llvm::Type*> argTypes{faddr->getType()};

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

    static llvm::Value* CopyCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
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

    static llvm::Value* LengthCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
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

    static llvm::Value* ClockCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
    {
	std::vector<llvm::Type*> argTypes;

	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetType(Types::Int64), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("__Clock", ft);

	return builder.CreateCall(f, "clock");
    }

    static llvm::Value* PanicCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
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

    static llvm::Value* PopCntCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
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

    static llvm::Value* CyclesCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
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

    static llvm::Value* FmodCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
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
	llvm::Constant* f = theModule->getOrInsertFunction("__fmod", ft);
	
	return builder.CreateCall(f, arg);
    }

    static llvm::Value* Arctan2CodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
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
	llvm::Constant* f = theModule->getOrInsertFunction("__arctan2", ft);
	
	return builder.CreateCall(f, arg);
    }

    static llvm::Value* TanCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
    {
	if (args.size() != 1)
	{
	    return ErrorV("Cycles function takes no arguments");
	}

	llvm::Type* realTy = Types::GetType(Types::Real);
	llvm::Value* a = args[0]->CodeGen();

	if (a->getType()->getTypeID() == llvm::Type::IntegerTyID)
	{
	    a = builder.CreateSIToFP(a, realTy, "tofp");
	}

	std::vector<llvm::Value*> arg = {a};
	
	std::vector<llvm::Type*> argTypes = {realTy};

	llvm::FunctionType* ft = llvm::FunctionType::get(realTy, argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("__tan", ft);
	
	return builder.CreateCall(f, arg);
    }

    static llvm::Value* ParamStrCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
    {
	if (args.size() != 1)
	{
	    return ErrorV("ParamStr takes one argument");
	}
	if (args[0]->Type()->Type() != Types::Integer)
	{
	    return ErrorV("Arguments to copy should be (string, integer, integer)");
	}
	llvm::Value* n   = args[0]->CodeGen();

	std::vector<llvm::Type*> argTypes{n->getType()};

	llvm::FunctionType* ft = llvm::FunctionType::get(Types::GetStringType()->LlvmType(), argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction("__ParamStr", ft);

	return builder.CreateCall(f, n, "paramstr");
    }

    static llvm::Value* ParamCountCodeGen(llvm::IRBuilder<>& builder, const std::vector<ExprAST*>& args)
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

    enum ResultForm
    {
	RF_Input,
	RF_Boolean,
	RF_Integer,
	RF_LongInt,
	RF_Real,
	RF_Char,
	RF_Void,
	RF_String,
    };

    struct BuiltinFunction
    {
	const char *name;
	CodeGenFunc CodeGen;
	ResultForm  rf;
    };

    const static BuiltinFunction bifs[] =
    {
	{ "odd",        OddCodeGen,        RF_Boolean },
	{ "trunc",      TruncCodeGen,      RF_Integer },
	{ "round",      RoundCodeGen,      RF_Integer },
	{ "sqr",        SqrCodeGen,        RF_Input },
	{ "sqrt",       SqrtCodeGen,       RF_Real },
	{ "sin",        SinCodeGen,        RF_Real },
	{ "cos",        CosCodeGen,        RF_Real },
	{ "tan",        TanCodeGen,        RF_Real },
	{ "arctan",     ArctanCodeGen,     RF_Real },
	{ "ln",         LnCodeGen,         RF_Real },
	{ "exp",        ExpCodeGen,        RF_Real },
	{ "chr",        ChrCodeGen,        RF_Char },
	{ "ord",        OrdCodeGen,        RF_Integer },
	{ "succ",       SuccCodeGen,       RF_Input },
	{ "pred",       PredCodeGen,       RF_Input },
	{ "new",        NewCodeGen,        RF_Void },
	{ "dispose",    DisposeCodeGen,    RF_Void },
	{ "assign",     AssignCodeGen,     RF_Void },
	{ "reset",      ResetCodeGen,      RF_Void },
	{ "close",      CloseCodeGen,      RF_Void },
	{ "rewrite",    RewriteCodeGen,    RF_Void },
	{ "append",     AppendCodeGen,     RF_Void },
	{ "eof",        EofCodeGen,        RF_Boolean },
	{ "eoln",       EolnCodeGen,       RF_Boolean },
	{ "random",     RandomCodeGen,     RF_Real },
	{ "get",        GetCodeGen,        RF_Void },
	{ "put",        PutCodeGen,        RF_Void },
	{ "copy",       CopyCodeGen,       RF_String },
	{ "length",     LengthCodeGen,     RF_Integer },
	{ "clock",      ClockCodeGen,      RF_LongInt },
	{ "panic",      PanicCodeGen,      RF_Void },
	{ "popcnt",     PopCntCodeGen,     RF_Integer },
	{ "cycles",     CyclesCodeGen,     RF_LongInt },
	{ "fmod",       FmodCodeGen,       RF_Real },
	{ "arctan2",    Arctan2CodeGen,    RF_Real },
	{ "paramcount", ParamCountCodeGen, RF_Integer },
	{ "paramstr",   ParamStrCodeGen,   RF_String },
    };

    static const BuiltinFunction* find(const std::string& name)
    {
	std::string nmlower = name;
	std::transform(nmlower.begin(), nmlower.end(), nmlower.begin(), ::tolower);
    
	for(size_t i = 0; i < sizeof(bifs)/sizeof(bifs[0]); i++)
	{
	    if (nmlower == bifs[i].name)
	    {
		return &bifs[i];
	    }
	}
	return 0;
    }

    llvm::Value* CodeGen(llvm::IRBuilder<>& builder,
				  const std::string& name,
				  const std::vector<ExprAST*>& args)
    {
	const BuiltinFunction* b = find(name);
	assert(b && "Expected to find builtin function here!");

	return b->CodeGen(builder, args);
    }

    Types::TypeDecl* Type(Stack<NamedObject*>& ns,
			  const std::string& name,
			  const std::vector<ExprAST*>& args)
    {
	const BuiltinFunction* b = find(name);
	if (!b)
	{
	    return 0;
	}

	const char* type = 0;
	switch(b->rf)
	{
	case RF_Input:
	    return args[0]->Type();
	
	case RF_Boolean:
	    type = "boolean";
	    break;

	case RF_Integer:
	    type = "integer";
	    break;

	case RF_LongInt:
	    type = "longint";
	    break;

	case RF_Char:
	    type = "char";
	    break;

	case RF_Real:
	    type = "real";
	    break;

	case RF_String:
	    return Types::GetStringType();
	    break;

	case RF_Void:
	    return Types::GetVoidType();
	}
	TypeDef* td = llvm::dyn_cast_or_null<TypeDef>(ns.FindBottomLevel(type));
	assert(td && "TypeDef not found?");
	if (!td)
	{
	    return 0;
	}
	return td->Type();
    }


    /* New style interface */ 
    bool IsBuiltin(const std::string& name)
    {
	auto it = BIFMap.find(name);
	if (it != BIFMap.end())
	{
	    return true;
	}
	/* TODO: Remove this when done */
	const BuiltinFunction* b = find(name);
	return b != 0;
    }

    BuiltinFunctionBase* CreateBuiltinFunction(const std::string& name, std::vector<ExprAST*>& args)
    {
	auto it = BIFMap.find(name);
	if (it != BIFMap.end())
	{
	    return it->second(args);
	}
	return 0;
    }

    void InitBuiltins()
    {
	AddBIFCreator("abs", CreateAbs);
    }

} // namespace Builtin
