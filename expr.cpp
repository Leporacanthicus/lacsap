#include "expr.h"
#include "stack.h"
#include "builtin.h"
#include "options.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/ADT/APSInt.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/DataLayout.h>

#include <iostream>
#include <sstream>
#include <map>

extern llvm::FunctionPassManager* fpm;
extern llvm::Module* theModule;

typedef Stack<llvm::Value *> VarStack;
typedef StackWrapper<llvm::Value *> VarStackWrapper;

class MangleMap
{
public:
    MangleMap(const std::string& name)
	: actualName(name) {}
    void dump() 
    { 
	std::cerr << "Name: " << actualName << std::endl; 
    }
    const std::string& Name() { return actualName; }
private:
    std::string actualName;
};

typedef Stack<MangleMap*> MangleStack;
typedef StackWrapper<MangleMap*> MangleWrapper;

VarStack variables;
MangleStack mangles;
static llvm::IRBuilder<> builder(llvm::getGlobalContext());
static int errCnt;

#define TRACE() do { if (verbosity) trace(__FILE__, __LINE__, __PRETTY_FUNCTION__); } while(0)

void trace(const char *file, int line, const char *func)
{
    std::cerr << file << ":" << line << "::" << func << std::endl;
}

bool FileInfo(llvm::Value* f, int& recSize, bool& isText)
{
    if (!f->getType()->isPointerTy())
    {
	return false;
    }
    llvm::Type* ty = f->getType()->getContainedType(0);
    llvm::StructType* st = llvm::dyn_cast<llvm::StructType>(ty);
    ty = 0;
    if (st)
    {
	ty = st->getElementType(Types::FileDecl::Buffer);
	if (ty->isPointerTy())
	{
	    ty = ty->getContainedType(0);
	    const llvm::DataLayout dl(theModule);
	    recSize = dl.getTypeAllocSize(ty);
	}
	else
	{
	    return false;
	}
    }
    isText = st->getName().substr(0, 4) == "text";
    return true;
}

bool FileIsText(llvm::Value* f)
{
    bool isText;
    int  recSize;

    if (FileInfo(f, recSize, isText))
    {
	return isText;
    }
    return false;
}

static llvm::AllocaInst* CreateAlloca(llvm::Function* fn, const VarDef& var)
{
    TRACE();
    llvm::IRBuilder<> bld(&fn->getEntryBlock(), fn->getEntryBlock().end());
    Types::FieldCollection* fc = llvm::dyn_cast<Types::FieldCollection>(var.Type());
    if (fc)
    {
	fc->EnsureSized();
    }
    llvm::Type* ty = var.Type()->LlvmType();
    if (!ty)
    {
	assert(0 && "Can't find type");
	return 0;
    }
    return bld.CreateAlloca(ty, 0, var.Name());
}

static llvm::AllocaInst* CreateTempAlloca(llvm::Type* ty)
{
   /* Save where we were... */
    llvm::BasicBlock* bb = builder.GetInsertBlock();
    llvm::BasicBlock::iterator ip = builder.GetInsertPoint();
    
    /* Get the "entry" block */
    llvm::Function *fn = builder.GetInsertBlock()->getParent();
    
    llvm::IRBuilder<> bld(&fn->getEntryBlock(), fn->getEntryBlock().begin());

    llvm::AllocaInst* tmp = bld.CreateAlloca(ty, 0, "tmp");
    
    builder.SetInsertPoint(bb, ip);
    
    return tmp;
}

llvm::Value* MakeAddressable(ExprAST* e, Types::TypeDecl* type)
{
    AddressableAST* ea = llvm::dyn_cast<AddressableAST>(e);
    llvm::Value* v;
    if (ea)
    {
	v = ea->Address();
	assert(v && "Expect address to be non-zero");
    }
    else
    {
	llvm::Type* ty = type->LlvmType();
	v = CreateTempAlloca(ty);
	builder.CreateStore(e->CodeGen(), v);
	assert(v && "Expect address to be non-zero");
    }
    return v;
}

void ExprAST::dump(std::ostream& out) const
{
    out << "Node=" << reinterpret_cast<const void *>(this) << ": "; 
    DoDump(out); 
    out << std::endl;
}	

void ExprAST::dump(void) const
{ 
    dump(std::cerr);
}

llvm::Value *ErrorV(const std::string& msg)
{
    std::cerr << msg << std::endl;
    errCnt++;
    return 0;
}

static llvm::Function *ErrorF(const std::string& msg)
{
    ErrorV(msg);
    return 0;
}

llvm::Value* MakeConstant(long val, llvm::Type* ty)
{
    return llvm::ConstantInt::get(ty, val);
}

llvm::Value* MakeIntegerConstant(int val)
{
    return MakeConstant(val, Types::GetType(Types::Integer));
}

llvm::Value* MakeInt64Constant(long val)
{
    return MakeConstant(val, Types::GetType(Types::Int64));
}

static llvm::Value* MakeBooleanConstant(int val)
{
    return MakeConstant(val, Types::GetType(Types::Boolean));
}

static llvm::Value* MakeCharConstant(int val)
{
    return MakeConstant(val, Types::GetType(Types::Char));
}

static llvm::Constant* GetMemCpyFn()
{
    static llvm::Constant* fnmemcpy;
    if (!fnmemcpy)
    {
	std::string func = "llvm.memcpy.p0i8.p0i8.i32";
	std::vector<llvm::Type*> argTypes;
	llvm::Type* ty = Types::GetVoidPtrType();
	llvm::Type* resTy = Types::GetType(Types::Void);
	argTypes.push_back(ty);
	argTypes.push_back(ty);
	argTypes.push_back(Types::GetType(Types::Integer));
	argTypes.push_back(Types::GetType(Types::Integer));
	argTypes.push_back(Types::GetType(Types::Boolean));
	
	llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
	fnmemcpy = theModule->getOrInsertFunction(func, ft);
    }
    return fnmemcpy;
}

static llvm::Value* TempStringFromStringExpr(llvm::Value* dest, StringExprAST* rhs)
{
    TRACE();
    std::vector<llvm::Value*> ind;
    ind.push_back(MakeIntegerConstant(0));
    ind.push_back(MakeIntegerConstant(0));
    llvm::Value* dest1 = builder.CreateGEP(dest, ind, "str_0");
    
    ind[1] = MakeIntegerConstant(1);
    llvm::Value* dest2 = builder.CreateGEP(dest, ind, "str_1");

    llvm::Constant* fnmemcpy = GetMemCpyFn();
    llvm::Value* v = rhs->CodeGen();
    builder.CreateStore(MakeCharConstant(rhs->Str().size()), dest1);
    
    return builder.CreateCall5(fnmemcpy, dest2, v, MakeIntegerConstant(rhs->Str().size()), 
			       MakeIntegerConstant(1), MakeBooleanConstant(false));
}

static llvm::Value* TempStringFromChar(llvm::Value* dest, ExprAST* rhs)
{
    TRACE();
    if (rhs->Type()->Type() != Types::Char)
    {
	return ErrorV("Expected char value");
    }
    std::vector<llvm::Value*> ind;
    ind.push_back(MakeIntegerConstant(0));
    ind.push_back(MakeIntegerConstant(0));
    llvm::Value* dest1 = builder.CreateGEP(dest, ind, "str_0");
    
    ind[1] = MakeIntegerConstant(1);
    llvm::Value* dest2 = builder.CreateGEP(dest, ind, "str_1");

    builder.CreateStore(MakeCharConstant(1), dest1);
    return builder.CreateStore(rhs->CodeGen(), dest2);
}

std::string ExprAST::ToString()
{
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

void ExprAST::EnsureSized() const
{
    TRACE();
    if (Type())
    {
	Types::FieldCollection* fc = llvm::dyn_cast<Types::FieldCollection>(Type());
	if (fc)
	{
	    fc->EnsureSized();
	}
    }
}

void RealExprAST::DoDump(std::ostream& out) const
{ 
    out << "Real: " << val;
}

llvm::Value* RealExprAST::CodeGen()
{
    TRACE();
    return llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(val));
}

void IntegerExprAST::DoDump(std::ostream& out) const
{ 
    out << "Integer: " << val;
}

llvm::Value* IntegerExprAST::CodeGen()
{
    TRACE();
    return MakeConstant(val, type->LlvmType());
}

void NilExprAST::DoDump(std::ostream& out) const
{ 
    out << "Nil: '" << "'";
}

llvm::Value* NilExprAST::CodeGen()
{
    TRACE();
    return llvm::ConstantPointerNull::get(llvm::dyn_cast<llvm::PointerType>(Types::GetVoidPtrType()));
}

void CharExprAST::DoDump(std::ostream& out) const
{ 
    out << "Char: '" << val << "'";
}

llvm::Value* CharExprAST::CodeGen()
{
    TRACE();
    llvm::Value *v = MakeCharConstant(val);
    return v;
}

void VariableExprAST::DoDump(std::ostream& out) const
{ 
    out << "Variable: " << name;
}

llvm::Value* VariableExprAST::CodeGen()
{ 
    // Look this variable up in the function.
    TRACE();
    llvm::Value* v = Address();
    if (!v)
    {
	return 0;
    }
    return builder.CreateLoad(v, name.c_str()); 
}

llvm::Value* VariableExprAST::Address()
{
    TRACE();
    llvm::Value* v = variables.Find(name);
    EnsureSized();
    if (!v)
    {
	return ErrorV(std::string("Unknown variable name '") + name + "'");
    }
    return v;
}

void ArrayExprAST::DoDump(std::ostream& out) const
{ 
    out << "Array: " << name;
    out << "[";
    bool first = true;
    for(auto i : indices)
    {
	if (!first)
	{
	    out << ", ";
	}
	first = false;
	i->dump(out);
    }
}

llvm::Value* ArrayExprAST::Address()
{
    TRACE();
    llvm::Value* v = expr->Address();
    EnsureSized();
    if (!v)
    {
	return ErrorV(std::string("Unknown variable name '") + name + "'");
    }
    llvm::Value* index; 
    for(size_t i = 0; i < indices.size(); i++)
    {
	/* TODO: Add range checking? */
	index = indices[i]->CodeGen();
	if (!index)
	{
	    return ErrorV("Expression failed for index");
	}
	if (!index->getType()->isIntegerTy())
	{
	    return ErrorV("Index is supposed to be integral type");
	}
	llvm::Type* ty = index->getType();
	index = builder.CreateSub(index, MakeConstant(ranges[i]->GetStart(), ty));
	index = builder.CreateMul(index, MakeConstant(indexmul[i], ty));
    }
    std::vector<llvm::Value*> ind;
    ind.push_back(MakeIntegerConstant(0));
    ind.push_back(index);
    v = builder.CreateGEP(v, ind, "valueindex");
    return v;
}

void FieldExprAST::DoDump(std::ostream& out) const
{
    out << "Field " << element << std::endl;
    expr->DoDump(out);
}

llvm::Value* FieldExprAST::Address()
{
    TRACE();
    EnsureSized();
    llvm::Value* v = expr->Address();
    if (!v) 
    {
	return ErrorV("Expression did not form an address");
    }
    std::vector<llvm::Value*> ind;
    ind.push_back(MakeIntegerConstant(0));
    ind.push_back(MakeIntegerConstant(element));
    v = builder.CreateGEP(v, ind, "valueindex");
    return v;
}


void VariantFieldExprAST::DoDump(std::ostream& out) const
{
    out << "Variant " << element << std::endl;
    expr->DoDump(out);
}

llvm::Value* VariantFieldExprAST::Address()
{
    TRACE();
    EnsureSized();
    //TODO: This needs fixing up.
    llvm::Value* v = expr->Address();
    std::vector<llvm::Value*> ind;
    ind.push_back(MakeIntegerConstant(0));
    ind.push_back(MakeIntegerConstant(element));
    v = builder.CreateGEP(v, ind, "valueindex");
    v = builder.CreateBitCast(v, llvm::PointerType::getUnqual(Type()->LlvmType()));

    return v;
}

void PointerExprAST::DoDump(std::ostream& out) const
{
    out << "Pointer:";
    pointer->dump(out);
}

llvm::Value* PointerExprAST::CodeGen()
{ 
    // Look this variable up in the function.
    TRACE();
    llvm::Value* v = Address();
    if (!v)
    {
	return 0;
    }
    if (!v->getType()->isPointerTy())
    {
	return ErrorV("Expected pointer type.");
    }
    return builder.CreateLoad(v, "ptr"); 
}

llvm::Value* PointerExprAST::Address()
{
    TRACE();
    EnsureSized();
    llvm::Value* v = pointer->CodeGen();
    if (!v)
    {
	return 0;
    }
    return v;
}

void FilePointerExprAST::DoDump(std::ostream& out) const
{
    out << "FilePointer:";
    pointer->dump(out);
}

llvm::Value* FilePointerExprAST::CodeGen()
{ 
    // Look this variable up in the function.
    TRACE();
    llvm::Value* v = Address();
    if (!v)
    {
	return 0;
    }
    if (!v->getType()->isPointerTy())
    {
	return ErrorV("Expected pointer type.");
    }
    return builder.CreateLoad(v, "ptr"); 
}

llvm::Value* FilePointerExprAST::Address()
{
    TRACE();
    VariableExprAST* vptr = llvm::dyn_cast<VariableExprAST>(pointer);
    llvm::Value* v = vptr->Address();
    std::vector<llvm::Value*> ind;
    ind.push_back(MakeIntegerConstant(0));
    ind.push_back(MakeIntegerConstant(Types::FileDecl::Buffer));
    v = builder.CreateGEP(v, ind, "bufptr");
    v = builder.CreateLoad(v, "buffer");
    if (!v)
    {
	return 0;
    }
    return v;
}

void FunctionExprAST::DoDump(std::ostream& out) const
{
    out << "Function " << name;
}

llvm::Value* FunctionExprAST::CodeGen()
{
    MangleMap* mm = mangles.Find(name); 
    if (!mm)
    {
	return ErrorV(std::string("Name ") + name + " could not be found...");
    }
    std::string actualName = mm->Name();
    return theModule->getFunction(actualName);
}

llvm::Value* FunctionExprAST::Address()
{
    assert(0 && "Don't expect this to be called...");
    return 0;
}

static int StringishScore(ExprAST* e)
{
    if (llvm::isa<CharExprAST>(e))
    {
	return 1;
    }
    if (llvm::isa<StringExprAST>(e))
    {
	return 2;
    }
    if (llvm::isa<Types::StringDecl>(e->Type()))
    {
	return 3;
    }
    if (e->Type()->Type() == Types::Char)
    {
	return 1;
    }
    return 0;
}

static bool BothStringish(ExprAST* lhs, ExprAST* rhs)
{
    int lScore = StringishScore(lhs);
    int rScore = StringishScore(rhs);

    return lScore && rScore && (lScore + rScore) > 2;
}

void BinaryExprAST::DoDump(std::ostream& out) const
{ 
    out << "BinaryOp: ";
    lhs->dump(out);
    oper.dump(out);
    rhs->dump(out); 
}

llvm::Value* BinaryExprAST::CallSetFunc(const std::string& name, bool resTyIsSet)
{
    TRACE();
    std::string func = std::string("__Set") + name;
    std::vector<llvm::Type*> argTypes;
    Types::TypeDecl* type = Types::TypeForSet();

    assert(type && "Expect to get a type from Types::TypeForSet");

    llvm::Type* resTy;
    if (resTyIsSet)
    {
	resTy = type->LlvmType();
    }
    else
    {
	resTy = Types::GetType(Types::Boolean);
    }

    llvm::Value* rV = MakeAddressable(rhs, type);
    llvm::Value* lV = MakeAddressable(lhs, type);

    if (!rV || !lV)
    {
	return 0;
    }

    llvm::Type* pty = llvm::PointerType::getUnqual(type->LlvmType());
    argTypes.push_back(pty);
    argTypes.push_back(pty);

    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
    llvm::Constant* f = theModule->getOrInsertFunction(func, ft);

    return builder.CreateCall2(f, lV, rV, "calltmp");
}


static llvm::Value* CallStrFunc(const std::string& name, ExprAST* lhs, ExprAST* rhs, llvm::Type* resTy, 
				const std::string& twine)
{
    TRACE();
    std::string func = std::string("__Str") + name;
    std::vector<llvm::Type*> argTypes;

    llvm::Value* rV;
    llvm::Value* lV;
    llvm::Type* ty;
    if (rhs->Type()->Type() == Types::Char)
    {
	Types::StringDecl* sd = Types::GetStringType();
	ty = sd->LlvmType();
	rV = CreateTempAlloca(ty);
	TempStringFromChar(rV, rhs);
    }
    else if (StringExprAST* srhs = llvm::dyn_cast<StringExprAST>(rhs))
    {
	Types::StringDecl* sd = Types::GetStringType();
	ty = sd->LlvmType();
	rV = CreateTempAlloca(ty);
	TempStringFromStringExpr(rV, srhs);
    }
    else 
    {
	ty = rhs->Type()->LlvmType();
	rV = MakeAddressable(rhs, rhs->Type());
    }

    if (lhs->Type()->Type() == Types::Char)
    {
	lV = CreateTempAlloca(ty);
	TempStringFromChar(lV, lhs);
    }
    else if (StringExprAST* slhs = llvm::dyn_cast<StringExprAST>(lhs))
    {
	lV = CreateTempAlloca(ty);
	TempStringFromStringExpr(lV, slhs);
    }
    else
    {
	lV = MakeAddressable(lhs, lhs->Type());
    }
	
    if (!rV || !lV)
    {
	return 0;
    }

    llvm::Type* pty = llvm::PointerType::getUnqual(ty);
    argTypes.push_back(pty);
    argTypes.push_back(pty);

    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
    llvm::Constant* f = theModule->getOrInsertFunction(func, ft);

    return builder.CreateCall2(f, lV, rV, twine);
}

llvm::Value* BinaryExprAST::CallStrFunc(const std::string& name, bool resTyIsStr)
{
    TRACE();

    llvm::Type* resTy;
    if (resTyIsStr)
    {
	Types::StringDecl* sd = Types::GetStringType();
	resTy = sd->LlvmType();
    }
    else
    {
	resTy = Types::GetType(Types::Integer);
    }

    return ::CallStrFunc(name, lhs, rhs, resTy, "calltmp");
}


llvm::Value* BinaryExprAST::CallArrFunc(const std::string& name, size_t size)
{
    std::string func = std::string("__Arr") + name;
    std::vector<llvm::Type*> argTypes;

    llvm::Value* rV;
    llvm::Value* lV;

    rV = MakeAddressable(rhs, rhs->Type());
    lV = MakeAddressable(rhs, rhs->Type());

    if (!rV || !lV)
    {
	return 0;
    }
    // Result is integer.
    llvm::Type* resTy = Types::GetType(Types::Integer);

    llvm::Type* pty = llvm::PointerType::getUnqual(Types::GetType(Types::Char));
    argTypes.push_back(pty);
    argTypes.push_back(pty);
    // Reuse result for size.
    argTypes.push_back(resTy);

    lV = builder.CreateBitCast(lV, pty);
    rV = builder.CreateBitCast(rV, pty);

    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
    llvm::Constant* f = theModule->getOrInsertFunction(func, ft);

    return builder.CreateCall3(f, lV, rV, MakeIntegerConstant(size), "calltmp");
}

Types::TypeDecl* BinaryExprAST::Type() const 
{
    Token::TokenType tt = oper.GetToken();
    if (tt >= Token::FirstComparison && tt <= Token::LastComparison)
    {
	return new Types::TypeDecl(Types::Boolean);
    }
    
    Types::TypeDecl* lType = lhs->Type();
    Types::TypeDecl* rType = rhs->Type();

    // If both are the same, then return this type.
    if (rType->Type() == lType->Type())
    {
	return rType;
    }

    // If either side is string, then make it string!
    if (rType->Type() == Types::String)
    {
	return rType;
    }
    if (lType->Type() == Types::String)
    {
	return lType;
    }

    // If either side is real, return that type, no matter what the other side is.
    if (rType->Type() == Types::Real)
    {
	return rType;
    }
    if (lType->Type() == Types::Real)
    {
	return lType;
    }

    // Last resort, return left type.
    return lType;
}

llvm::Value* BinaryExprAST::CodeGen()
{
    TRACE();

    if (!lhs->Type() || !rhs->Type())
    {
	assert(0 && "Huh? Both sides of expression should have type");
	return ErrorV("One or both sides of binary expression does not have a type...");
    }

    llvm::Value* tmp = 0;
    if (BothStringish(lhs, rhs))
    {
	switch(oper.GetToken())
	{
	case Token::Plus:
	    return CallStrFunc("Concat", true);

 	case Token::Equal:
	    tmp = CallStrFunc("Compare", false);
	    return builder.CreateICmpEQ(tmp, MakeIntegerConstant(0), "eq");
	    
	case Token::NotEqual:
 	    tmp = CallStrFunc("Compare", false);
	    return builder.CreateICmpNE(tmp, MakeIntegerConstant(0), "ne");

	case Token::GreaterOrEqual:
 	    tmp = CallStrFunc("Compare", false);
	    return builder.CreateICmpSGE(tmp, MakeIntegerConstant(0), "ge");
	    
	case Token::LessOrEqual:
 	    tmp = CallStrFunc("Compare", false);
	    return builder.CreateICmpSLE(tmp, MakeIntegerConstant(0), "le");

	case Token::GreaterThan:
 	    tmp = CallStrFunc("Compare", false);
	    return builder.CreateICmpSGT(tmp, MakeIntegerConstant(0), "gt");
	    
	case Token::LessThan:
 	    tmp = CallStrFunc("Compare", false);
	    return builder.CreateICmpSLT(tmp, MakeIntegerConstant(0), "lt");

	default:
	    return ErrorV("Invalid operand for strings");
	}
    }
    else if (llvm::isa<Types::ArrayDecl>(rhs->Type()) && llvm::isa<Types::ArrayDecl>(lhs->Type()))
    {
	// Comparison operators are allowed for old style Pascal strings (char arrays)
	// But only if they are the same size.
	if (lhs->Type()->SubType()->Type() == Types::Char &&
	    rhs->Type()->SubType()->Type() == Types::Char)
	{
	    Types::ArrayDecl* ar = llvm::dyn_cast<Types::ArrayDecl>(rhs->Type());
	    Types::ArrayDecl* al = llvm::dyn_cast<Types::ArrayDecl>(lhs->Type());

	    std::vector<Types::Range*> rr = ar->Ranges();
	    std::vector<Types::Range*> rl = al->Ranges();

	    if (rr.size() == 1 && 
                rl.size() == 1 && 
		rr[0]->Size() == rl[0]->Size())
	    {
		size_t size = rr[0]->Size();
		tmp = CallArrFunc("Compare", size);
		switch (oper.GetToken())
		{
		case Token::Equal:
		    return builder.CreateICmpEQ(tmp, MakeIntegerConstant(0), "eq");
		    
		case Token::NotEqual:
		    return builder.CreateICmpNE(tmp, MakeIntegerConstant(0), "ne");
		    
		case Token::GreaterOrEqual:
		    return builder.CreateICmpSGE(tmp, MakeIntegerConstant(0), "ge");
		    
		case Token::LessOrEqual:
		    return builder.CreateICmpSLE(tmp, MakeIntegerConstant(0), "le");
		    
		case Token::GreaterThan:
		    return builder.CreateICmpSGT(tmp, MakeIntegerConstant(0), "gt");
		    
		case Token::LessThan:
		    return builder.CreateICmpSLT(tmp, MakeIntegerConstant(0), "lt");
		    
		default:
		    return ErrorV("Invalid operand for char arrays");
		}
	    }
	}
    }
    else if (llvm::isa<Types::SetDecl>(rhs->Type()))
    {
	if (lhs->Type()->isIntegral() &&
	    oper.GetToken() == Token::In)
	{
	    llvm::Value* l = lhs->CodeGen();
	    std::vector<llvm::Value*> ind;
	    AddressableAST* rhsA = llvm::dyn_cast<AddressableAST>(rhs);
	    if (!rhsA)
	    {
		return ErrorV("Set value should be addressable!");
	    }
	    llvm::Value* setV = rhsA->Address();
	    l = builder.CreateZExt(l, Types::GetType(Types::Integer), "zext.l");
	    llvm::Value* index = builder.CreateLShr(l, MakeIntegerConstant(5));
	    llvm::Value* offset = builder.CreateAnd(l, MakeIntegerConstant(31));
	    ind.push_back(MakeIntegerConstant(0));
	    ind.push_back(index);
	    llvm::Value *bitsetAddr = builder.CreateGEP(setV, ind, "valueindex");
	    
	    llvm::Value *bitset = builder.CreateLoad(bitsetAddr);
	    llvm::Value* bit = builder.CreateLShr(bitset, offset);
	    return builder.CreateTrunc(bit, Types::GetType(Types::Boolean));
	}
	else if (llvm::isa<Types::SetDecl>(lhs->Type()))
	{
	    switch(oper.GetToken())
	    {
	    case Token::Minus:
		return CallSetFunc("Diff", true);
		
	    case Token::Plus:
		return CallSetFunc("Union", true);
		
	    case Token::Multiply:
		return CallSetFunc("Intersect", true);
	    
	    case Token::Equal:
		return CallSetFunc("Equal", false);

	    case Token::NotEqual:
		return builder.CreateNot(CallSetFunc("Equal", false), "notEqual");

	    case Token::LessOrEqual:
		return CallSetFunc("Contains", false);

	    case Token::GreaterOrEqual:
	    {
		ExprAST* tmp = rhs;
		rhs = lhs;
		lhs = tmp;
		llvm::Value* v = CallSetFunc("Contains", false);
		// Set it back
		lhs = rhs;
		rhs = tmp;
		return v;
	    }

	    default:
		return ErrorV("Unknown operator on set");
	    }
	}
	else
	{
	    return ErrorV("Invalid arguments in set operation");
	}
    }

    llvm::Value *l = lhs->CodeGen();
    llvm::Value *r = rhs->CodeGen();
    
    if (l == 0 || r == 0) 
    {
	return 0;
    }

    llvm::Type* rty = r->getType();
    llvm::Type* lty = l->getType();

    bool rToFloat = false;
    bool lToFloat = false;

    if (rty->isIntegerTy() && lty->isIntegerTy())
    {
	unsigned lsize = lty->getPrimitiveSizeInBits();
	unsigned rsize = rty->getPrimitiveSizeInBits();
	if (lsize > 1 && rsize > 1 && lsize != rsize)
	{
	    if (rsize > lsize)
	    {
		l = builder.CreateSExt(l, rty);
		lty = rty;
	    }
	    else
	    {
		r = builder.CreateSExt(r, lty);
		rty = lty;
	    }
	}
    }

    /* Convert right hand side to double if left is double, and right is integer */
    if (rty->isIntegerTy())
    {
	if (lty->isDoubleTy() || oper.GetToken() == Token::Divide)
	{
	    rToFloat = true;
	}
    }
    if (lty->isIntegerTy())
    {
	if (rty->isDoubleTy() || oper.GetToken() == Token::Divide)
	{
	    lToFloat = true;
	}
    }

    if (rToFloat)
    {
	r = builder.CreateSIToFP(r, Types::GetType(Types::Real), "tofp");
	rty = r->getType();
    }

    if (lToFloat)
    {
	l = builder.CreateSIToFP(l, Types::GetType(Types::Real), "tofp");
	lty = r->getType();
    }


    if (lty->getTypeID() == llvm::Type::PointerTyID && llvm::isa<NilExprAST>(rhs))
    {
	r = builder.CreateBitCast(r, lty);
	rty = lty;
    }



    if (rty != lty)
    {
	std::cout << "Different types..." << std::endl;
	l->dump();
	r->dump();
	assert(0 && "Different types...");
	return 0;
    }

    // Can compare for (un)equality with pointers and integers
    if (rty->isIntegerTy() || rty->isPointerTy())
    {
	switch(oper.GetToken())
	{
	case Token::Equal:
	    return builder.CreateICmpEQ(l, r, "eq");
	case Token::NotEqual:
	    return builder.CreateICmpNE(l, r, "ne");
	default:
	    break;
	}
    }

    if (rty->isIntegerTy())
    {
	llvm::IntegerType* ity = llvm::dyn_cast<llvm::IntegerType>(rty);
	assert(ity && "Expected to make rty into an integer type!");
	// In future, we may need further "unsigned" variants... 
	bool isUnsigned = ity->getBitWidth() == 1;
	switch(oper.GetToken())
	{
	case Token::Plus:
	    return builder.CreateAdd(l, r, "addtmp");
	case Token::Minus:
	    return builder.CreateSub(l, r, "subtmp");
	case Token::Multiply:
	    return builder.CreateMul(l, r, "multmp");
	case Token::Div:
	    return builder.CreateSDiv(l, r, "divtmp");
	case Token::Mod:
	    return builder.CreateSRem(l, r, "modtmp");
	case Token::Shr:
	    return builder.CreateLShr(l, r, "shrtmp");
	case Token::Shl:
	    return builder.CreateShl(l, r, "shltmp");
	case Token::Xor:
	    return builder.CreateXor(l, r, "xortmp");

	case Token::LessThan:
	    if (isUnsigned)
	    {
		return builder.CreateICmpULT(l, r, "lt");
	    }
	    return builder.CreateICmpSLT(l, r, "lt");
	case Token::LessOrEqual:
	    if (isUnsigned)
	    {
		return builder.CreateICmpULE(l, r, "le");
	    }
	    return builder.CreateICmpSLE(l, r, "le");
	case Token::GreaterThan:
	    if (isUnsigned)
	    {
		return builder.CreateICmpUGT(l, r, "gt");
	    }
	    return builder.CreateICmpSGT(l, r, "gt");
	case Token::GreaterOrEqual:
	    if (isUnsigned)
	    {
		return builder.CreateICmpUGE(l, r, "ge");
	    }
	    return builder.CreateICmpSGE(l, r, "ge");

	case Token::And:
	    return builder.CreateAnd(l, r, "and");

	case Token::Or:
	    return builder.CreateOr(l, r, "or");
	    
	default:
	    return ErrorV(std::string("Unknown token: ") + oper.ToString());
	}
    }
    else if (rty->isDoubleTy())
    {
	switch(oper.GetToken())
	{
	case Token::Plus:
	    return builder.CreateFAdd(l, r, "addtmp");
	case Token::Minus:
	    return builder.CreateFSub(l, r, "subtmp");
	case Token::Multiply:
	    return builder.CreateFMul(l, r, "multmp");
	case Token::Divide:
	    return builder.CreateFDiv(l, r, "divtmp");

	case Token::Equal:
	    return builder.CreateFCmpOEQ(l, r, "eq");
	case Token::NotEqual:
	    return builder.CreateFCmpONE(l, r, "ne");
	case Token::LessThan:
	    return builder.CreateFCmpOLT(l, r, "lt");
	case Token::LessOrEqual:
	    return builder.CreateFCmpOLE(l, r, "le");
	case Token::GreaterThan:
	    return builder.CreateFCmpOGT(l, r, "gt");
	case Token::GreaterOrEqual:
	    return builder.CreateFCmpOGE(l, r, "ge");

	default:
	    return ErrorV(std::string("Unknown token: ") + oper.ToString());
	}
    }
    else
    {
	l->dump();
	oper.dump(std::cout);
	r->dump();
	return ErrorV("Huh?");
    }
}

void UnaryExprAST::DoDump(std::ostream& out) const
{ 
    out << "Unary: " << oper.ToString();
    rhs->dump(out);
}

Types::TypeDecl* UnaryExprAST::Type() const
{
    return rhs->Type();
}

llvm::Value* UnaryExprAST::CodeGen()
{
    llvm::Value* r = rhs->CodeGen();
    llvm::Type::TypeID rty = r->getType()->getTypeID();
    if (rty == llvm::Type::IntegerTyID)
    {
	switch(oper.GetToken())
	{
	case Token::Minus:
	    return builder.CreateNeg(r, "minus");
	case Token::Not:
	    return builder.CreateNot(r, "not");
	default:
	    return ErrorV(std::string("Unknown token: ") + oper.ToString());
	}
    }
    else if (rty == llvm::Type::DoubleTyID)
    {
	switch(oper.GetToken())
	{
	case Token::Minus:
	    return builder.CreateFNeg(r, "minus");
	default:
	    return ErrorV(std::string("Unknown token: ") + oper.ToString());
	}
    }
    return ErrorV(std::string("Unknown type: ") + oper.ToString());
}

void CallExprAST::DoDump(std::ostream& out) const
{ 
    out << "call: " << proto->Name() << "(";
    for(auto i : args)
    {
	i->dump(out);
    }
    out << ")";
}

llvm::Value* CallExprAST::CodeGen()
{
    TRACE();
    assert(proto && "Function prototype should be set in this case!");

    llvm::Value* calleF = callee->CodeGen();
    if (!calleF)
    {
	return ErrorV(std::string("Unknown function ") + proto->Name() + " referenced");
    }	

    const std::vector<VarDef>& vdef = proto->Args();
    if (vdef.size() != args.size())
    {
	return ErrorV(std::string("Incorrect number of arguments for ") + proto->Name() + ".");
    }

    std::vector<llvm::Value*> argsV;
    std::vector<VarDef>::const_iterator viter = vdef.begin();
    for(auto i : args)
    {
	llvm::Value* v;
	if (viter->IsRef())
	{
	    VariableExprAST* vi = llvm::dyn_cast<VariableExprAST>(i);
	    if (!vi)
	    {
		return ErrorV("Args declared with 'var' must be a variable!");
	    }
	    v = vi->Address();
	}
	else
	{
	    v = i->CodeGen();
	    if (!v)
	    {
		return 0;
	    }
	    /* Do we need to convert to float? */
	    if (v->getType()->isIntegerTy() && viter->Type()->Type() == Types::Real)
	    {
		v = builder.CreateSIToFP(v, Types::GetType(Types::Real), "tofp");
	    }
	}
	if (!v)
	{
	    return ErrorV("Invalid argument for " + proto->Name() + " (" + i->ToString() + ")");
	}
	
	argsV.push_back(v);
	viter++;
    }
    if (proto->Type()->Type() == Types::Void) 
    {
	return builder.CreateCall(calleF, argsV, "");
    }
    return builder.CreateCall(calleF, argsV, "calltmp");
}

void BuiltinExprAST::DoDump(std::ostream& out) const
{ 
    out << " builtin call: " << name << "(";
    for(auto i : args)
    {
	i->dump(out);
    }
    out << ")";
}

llvm::Value* BuiltinExprAST::CodeGen()
{
    return Builtin::CodeGen(builder, name, args);
}    

void BlockAST::DoDump(std::ostream& out) const
{
    out << "Block: Begin " << std::endl;
    for(auto p : content) 
    {
	p->dump(out);
    }
    out << "Block End;" << std::endl;
}

llvm::Value* BlockAST::CodeGen()
{
    TRACE();
    llvm::Value *v = 0;
    for(auto e : content)
    {
	v = e->CodeGen();
	assert(v && "Expect codegen to work!");
    }
    return v;
}

void PrototypeAST::DoDump(std::ostream& out) const
{ 
    out << "Prototype: name: " << name << "(" << std::endl;
    for(auto i : args)
    {
	i.dump(); 
	out << std::endl;
    }
    out << ")";
}

llvm::Function* PrototypeAST::CodeGen(const std::string& namePrefix)
{
    TRACE();
    assert(namePrefix != "" && "Prefix should never be empty");
    std::vector<llvm::Type*> argTypes;
    for(auto i : args)
    {
	llvm::Type* ty = i.Type()->LlvmType();
	if (!ty)
	{
	    return ErrorF(std::string("Invalid type for argument") + i.Name() + "...");
	}
	if (i.IsRef())
	{
	    ty = llvm::PointerType::getUnqual(ty);
	}
	argTypes.push_back(ty);
    }
    llvm::Type* resTy = resultType->LlvmType();
    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
    std::string actualName;
    /* Don't mangle our 'main' functions name, as we call that from C */
    if (name == "__PascalMain")
    {
	actualName = name;
    }
    else
    {
	actualName = namePrefix + "." + name;
    }

    if (!mangles.FindTopLevel(name))
    {
	if (!mangles.Add(name, new MangleMap(actualName)))
	{
	    return ErrorF(std::string("Name ") + name + " already in use?");
	}
    }

    llvm::Constant* cf = theModule->getOrInsertFunction(actualName, ft);
    llvm::Function* f = llvm::dyn_cast<llvm::Function>(cf);
    if (!f->empty())
    {
	return ErrorF(std::string("redefinition of function: ") + name);
    }
    
    if (f->arg_size() != args.size())
    {
	return ErrorF(std::string("Change in number of arguemts for function: ") + name);
    }	    

    return f;
}

llvm::Function* PrototypeAST::CodeGen()
{
    return CodeGen("P");
}

void PrototypeAST::CreateArgumentAlloca(llvm::Function* fn)
{
    unsigned idx = 0;
    for(llvm::Function::arg_iterator ai = fn->arg_begin(); 
	idx < args.size();
	idx++, ai++)
    {
	llvm::Value* a;
	if (args[idx].IsRef())
	{
	    a = ai; 
	}
	else
	{
	    a=CreateAlloca(fn, args[idx]);
	    builder.CreateStore(ai, a);
	}
	if (!variables.Add(args[idx].Name(), a))
	{
	    ErrorF(std::string("Duplicate variable name ") + args[idx].Name());
	}
    }
    if (resultType->Type() != Types::Void)
    {
	llvm::AllocaInst* a=CreateAlloca(fn, VarDef(name, resultType));
	if(!variables.Add(name, a))
	{
	    ErrorF(std::string("Duplicate variable name ") + name);
	}
    }
}

void PrototypeAST::AddExtraArgs(const std::vector<VarDef>& extra)
{
    for(auto v : extra)
    {
	VarDef tmp = VarDef(v.Name(), v.Type(), true);
	args.push_back(tmp);
    }
}

void FunctionAST::DoDump(std::ostream& out) const
{ 
    out << "Function: " << std::endl;
    proto->dump(out);
    out << "Function body:" << std::endl;
    body->dump(out);
}

llvm::Function* FunctionAST::CodeGen(const std::string& namePrefix)
{
    VarStackWrapper w(variables);
    TRACE();
    assert(namePrefix != "" && "Prefix should not be empty");
    llvm::Function* theFunction = proto->CodeGen(namePrefix);
    if (!theFunction)
    {
	return 0;
    }
    if (proto->IsForward())
    {
	return theFunction;
    }

    llvm::BasicBlock *bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", theFunction);
    builder.SetInsertPoint(bb);

    proto->CreateArgumentAlloca(theFunction);

    if (varDecls)
    {
	varDecls->SetFunction(theFunction);
	varDecls->CodeGen();
    }

    llvm::BasicBlock::iterator ip = builder.GetInsertPoint();

    MangleWrapper   m(mangles);

    if (subFunctions.size())
    {
	std::string newPrefix;
	if (namePrefix != "")
	{
	    newPrefix = namePrefix + "." + proto->Name();
	}
	else
	{
	    newPrefix = proto->Name();
	}

	for(auto fn : subFunctions)
	{
	    
	    fn->CodeGen(newPrefix);
	}
    }

    if (verbosity > 1)
    {
	variables.dump();
	mangles.dump();
    }

    builder.SetInsertPoint(bb, ip);
    llvm::Value *block = body->CodeGen();
    if (!block && !body->IsEmpty())
    {
	return 0;
    }

    if (proto->Type()->Type() == Types::Void)
    {
	builder.CreateRetVoid();
    }
    else
    {
	llvm::Value* v = variables.Find(proto->Name());
	assert(v);
	llvm::Value* retVal = builder.CreateLoad(v);
	builder.CreateRet(retVal);
    }

    TRACE();
    verifyFunction(*theFunction);
    fpm->run(*theFunction);
    return theFunction;
}

llvm::Function* FunctionAST::CodeGen()
{
    return CodeGen("P");
}

void FunctionAST::SetUsedVars(const std::vector<NamedObject*>& varsUsed, 
			      const std::vector<NamedObject*>& localVars,
			      const std::vector<NamedObject*>& globalVars)
{
    std::map<std::string, NamedObject*> nonLocal;
    for(auto v : varsUsed)
    {
	nonLocal[v->Name()] = v;
    }
    // Now add those of the subfunctions.
    for(auto fn : subFunctions)
    {
	for(auto v : fn->UsedVars())
	{
	    nonLocal[v.Name()] = new VarDef(v);
	}
    }

    // Remove variables inside the function.
    for(auto l : localVars)
    {
	nonLocal.erase(l->Name());
    }

    // Remove variables that are global.
    for(auto g : globalVars)
    {
	nonLocal.erase(g->Name());
    }

    for(auto n : nonLocal)
    {
	VarDef* v = llvm::dyn_cast<VarDef>(n.second);
	if (v)
	{
	    usedVariables.push_back(*v);
	    if (verbosity)
	    {
		v->dump();
	    }
	}
    }
}

void StringExprAST::DoDump(std::ostream& out) const
{
    out << "String: '" << val << "'" << std::endl;
}

llvm::Value* StringExprAST::CodeGen()
{
    TRACE();
    return builder.CreateGlobalStringPtr(val.c_str(), "_string");
}

void AssignExprAST::DoDump(std::ostream& out) const
{
    out << "Assign: " << std::endl;
    lhs->dump(out);
    out << ":=";
    rhs->dump(out);
}

llvm::Value* AssignExprAST::AssignStr()
{
    TRACE();
    VariableExprAST* lhsv = llvm::dyn_cast<VariableExprAST>(lhs);
    assert(lhsv && "Expect variable in lhs");
    Types::StringDecl* sty = llvm::dyn_cast<Types::StringDecl>(lhsv->Type()); 
    assert(sty && "Expect  string type in lhsv->Type()");

    llvm::Value *dest = lhsv->Address();

    if (rhs->Type()->Type() == Types::Char)
    {
	return TempStringFromChar(dest, rhs);
    }
    else if (StringExprAST* srhs = llvm::dyn_cast<StringExprAST>(rhs))
    {
	return TempStringFromStringExpr(dest, srhs);
    }
    else if (llvm::isa<Types::StringDecl>(rhs->Type()))
    {
	return CallStrFunc("Assign", lhs, rhs, Types::GetVoidType()->LlvmType(), "");
    }

    assert(0 && "Unknown type");
    return 0;
}

llvm::Value* AssignExprAST::CodeGen()
{
    TRACE();    

    VariableExprAST* lhsv = llvm::dyn_cast<VariableExprAST>(lhs);
    if (!lhsv)
    {
	lhs->dump(std::cerr);
	return ErrorV("Left hand side of assignment must be a variable");
    }

    const Types::StringDecl* sty = llvm::dyn_cast<const Types::StringDecl>(lhsv->Type()); 
    if (sty)
    {
	return AssignStr();
    }

    if (llvm::isa<StringExprAST>(rhs) && 
	lhs->Type()->Type() == Types::Array)
    {
	Types::ArrayDecl* arr = llvm::dyn_cast<Types::ArrayDecl>(lhs->Type());
	if (arr->SubType()->Type() == Types::Char)
	{
	    StringExprAST* str = llvm::dyn_cast<StringExprAST>(rhs);
	    assert(rhs && "Expected string to convert correctly");
	    llvm::Value* dest = lhsv->Address();
	    std::vector<llvm::Value*> ind;
	    ind.push_back(MakeIntegerConstant(0));
	    ind.push_back(MakeIntegerConstant(0));
	    llvm::Value* dest1 = builder.CreateGEP(dest, ind, "str_0");
	    llvm::Value* v = rhs->CodeGen();
	    llvm::Constant* fnmemcpy = GetMemCpyFn();
	    
	    return builder.CreateCall5(fnmemcpy, dest1, v, MakeIntegerConstant(str->Str().size()), 
				       MakeIntegerConstant(1), MakeBooleanConstant(false));
	}
    }

    llvm::Value* v = rhs->CodeGen();
    llvm::Value* dest = lhsv->Address();

    if (!dest)
    {
	return ErrorV(std::string("Unknown variable name ") + lhsv->Name());
    }
    if (!v)
    {
	return ErrorV("Could not produce expression for assignment");
    }
    llvm::Type::TypeID lty = dest->getType()->getContainedType(0)->getTypeID();
    llvm::Type::TypeID rty = v->getType()->getTypeID();

    if (rty == llvm::Type::IntegerTyID &&
	lty == llvm::Type::DoubleTyID)
    {
	v = builder.CreateSIToFP(v, Types::GetType(Types::Real), "tofp");
	rty = v->getType()->getTypeID();
    }

    if (lty == llvm::Type::PointerTyID && llvm::isa<NilExprAST>(rhs))
    {
	v = builder.CreateBitCast(v, dest->getType()->getContainedType(0));
    }

    assert(rty == lty && 
	   "Types must be the same in assignment.");
    
    builder.CreateStore(v, dest);
    
    return v;
}

void IfExprAST::DoDump(std::ostream& out) const
{
    out << "if: " << std::endl;
    cond->dump(out);
    out << "then: ";
    if (then)
    {
	then->dump(out);
    }
    if (other)
    {
	out << " else::";
	other->dump(out);
    }
}

llvm::Value* IfExprAST::CodeGen()
{
    TRACE();
    llvm::Value *condV = cond->CodeGen();
    if (!condV)
    {
	return 0;
    }

    if (condV->getType() !=  Types::GetType(Types::Boolean))
    {
	assert(0 && "Only boolean expressions allowed in if-statement");
    }
    llvm::Function *theFunction = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "then", theFunction);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "ifcont");

    llvm::BasicBlock* elseBB = mergeBB;
    if (other)
    {
	elseBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "else");
    }

    builder.CreateCondBr(condV, thenBB, elseBB);
    builder.SetInsertPoint(thenBB);

    if (then)
    {
	llvm::Value* thenV = then->CodeGen();
	if (!thenV)
	{
	    return 0;
	}
    }

    builder.CreateBr(mergeBB);

    if (other)
    {
	assert(elseBB != mergeBB && "ElseBB should be different from MergeBB");
	theFunction->getBasicBlockList().push_back(elseBB);
	builder.SetInsertPoint(elseBB);
	
	llvm::Value* elseV =  other->CodeGen();
	if (!elseV)
	{
	    return 0;
	}
	builder.CreateBr(mergeBB);
    }
    theFunction->getBasicBlockList().push_back(mergeBB);
    builder.SetInsertPoint(mergeBB);

    return reinterpret_cast<llvm::Value*>(1);
}

void ForExprAST::DoDump(std::ostream& out) const
{
    out << "for: " << std::endl;
    start->dump(out);
    if (stepDown)
    {
	out << " downto ";
    }
    else
    {
	out << " to ";
    }
    end->dump(out);
    out << " do ";
    body->dump(out);
}

llvm::Value* ForExprAST::CodeGen()
{
    TRACE();

    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();
    llvm::Value* var = variables.Find(varName);
    if (!var)
    {
	return 0;
    }
    llvm::Value* startV = start->CodeGen();
    if (!startV)
    {
	return 0;
    }

    llvm::Value* stepVal =MakeConstant((stepDown)?-1:1, startV->getType());

    builder.CreateStore(startV, var, "loopvar"); 

    llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "loop", theFunction);    
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "afterloop", 
							 theFunction);
    
    llvm::Value* curVar = builder.CreateLoad(var, varName.c_str(), "temp");
    llvm::Value* endV = end->CodeGen();
    llvm::Value* endCond;

    if (stepDown) 
    {
	endCond = builder.CreateICmpSGE(curVar, endV, "loopcond");
    }
    else
    {
	endCond = builder.CreateICmpSLE(curVar, endV, "loopcond");
    }

    builder.CreateCondBr(endCond, loopBB, afterBB);

    builder.SetInsertPoint(loopBB);

    if (!body->CodeGen())
    {
	return 0;
    }
    curVar = builder.CreateLoad(var, varName.c_str(), "temp");
    curVar = builder.CreateAdd(curVar, stepVal, "nextvar");

    builder.CreateStore(curVar, var);
    
    if (stepDown) 
    {
	endCond = builder.CreateICmpSGE(curVar, endV, "endcond");
    }
    else
    {
	endCond = builder.CreateICmpSLE(curVar, endV, "endcond");
    }

    builder.CreateCondBr(endCond, loopBB, afterBB);
    
    builder.SetInsertPoint(afterBB);

    return afterBB;
}

void WhileExprAST::DoDump(std::ostream& out) const
{
    out << "While: ";
    cond->dump(out);
    out << " Do: ";
    body->dump(out);
}

llvm::Value* WhileExprAST::CodeGen()
{
    TRACE();
    llvm::Function *theFunction = builder.GetInsertBlock()->getParent();

    /* We will need a "prebody" before the loop, a "body" and an "after" basic block  */
    llvm::BasicBlock* preBodyBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "prebody", 
							   theFunction);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "body", 
							 theFunction);
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "after", 
							 theFunction);

    builder.CreateBr(preBodyBB);
    builder.SetInsertPoint(preBodyBB);
    
    llvm::Value* condv = cond->CodeGen();

    llvm::Value* endCond = builder.CreateICmpEQ(condv, MakeBooleanConstant(0), "whilecond");
    builder.CreateCondBr(endCond, afterBB, bodyBB); 

    builder.SetInsertPoint(bodyBB);
    if (!body->CodeGen())
    {
	return 0;
    }
    builder.CreateBr(preBodyBB);
    builder.SetInsertPoint(afterBB);
    
    return afterBB;
}

void RepeatExprAST::DoDump(std::ostream& out) const
{
    out << "Repeat: ";
    body->dump(out);
    out << " until: ";
    cond->dump(out);
}

llvm::Value* RepeatExprAST::CodeGen()
{
    llvm::Function *theFunction = builder.GetInsertBlock()->getParent();

    /* We will need a "body" and an "after" basic block  */
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "body", 
							 theFunction);
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "after", 
							 theFunction);

    builder.CreateBr(bodyBB);
    builder.SetInsertPoint(bodyBB);
    if (!body->CodeGen())
    {
	return 0;
    }
    llvm::Value* condv = cond->CodeGen();
    llvm::Value* endCond = builder.CreateICmpNE(condv, MakeBooleanConstant(0), "untilcond");
    builder.CreateCondBr(endCond, afterBB, bodyBB); 

    builder.SetInsertPoint(afterBB);
    
    return afterBB;
}

llvm::Value* FileOrNull(VariableExprAST* file)
{
    if (file)
    {
	return file->Address();
    }

    Types::TypeDecl ty(Types::Char);
    llvm::Type *fty = Types::GetFileType("text", &ty);
    fty = llvm::PointerType::getUnqual(fty);
    return llvm::Constant::getNullValue(fty);
}

void WriteAST::DoDump(std::ostream& out) const
{
    if (isWriteln)
    {
	out << "Writeln(";
    }
    else
    {
	out << "Write(";
    }
    bool first = true;
    for(auto a : args)
    {
	if (!first)
	{
	    out << ", ";
	}
	first = false;
	a.expr->dump(out);
	if (a.width)
	{
	    out << ":";
	    a.width->dump(out);
	}
	if (a.precision)
	{
	    out << ":";
	    a.precision->dump(out);
	}
    }
    out << ")";
}

static llvm::Constant *CreateWriteFunc(Types::TypeDecl* ty, llvm::Type* fty)
{
    std::string suffix;
    std::vector<llvm::Type*> argTypes;
    llvm::Type* resTy = Types::GetType(Types::Void);
    argTypes.push_back(fty);
    if (ty)
    {
	switch(ty->Type())
	{
	case Types::Char:
	    argTypes.push_back(ty->LlvmType());
	    argTypes.push_back(Types::GetType(Types::Integer));
	    suffix = "char";
	    break;

	case Types::Boolean:
	{
	    argTypes.push_back(ty->LlvmType());
	    argTypes.push_back(Types::GetType(Types::Integer));
	    suffix = "bool";
	    break;
	}

	case Types::Integer:
	    // Make args of two integers. 
	    argTypes.push_back(ty->LlvmType());
	    argTypes.push_back(ty->LlvmType());
	    suffix = "int";
	    break;

	case Types::Int64:
	    // Make args of two integers. 
	    argTypes.push_back(ty->LlvmType());
	    argTypes.push_back(Types::GetType(Types::Integer));
	    suffix = "int64";
	    break;

	case Types::Real:
	{
	    // Args: double, int, int
	    argTypes.push_back(ty->LlvmType());
	    llvm::Type* t = Types::GetType(Types::Integer); 
	    argTypes.push_back(t);
	    argTypes.push_back(t);
	    suffix = "real";
	    break;
	}
	case Types::String:
	{
	    llvm::Type* pty = llvm::PointerType::getUnqual(Types::GetType(Types::Char));
	    argTypes.push_back(pty);
	    argTypes.push_back(Types::GetType(Types::Integer));
	    suffix = "str";
	    break;
	}
	case Types::Array:
	{
	    Types::ArrayDecl* ad = llvm::dyn_cast<Types::ArrayDecl>(ty);
	    assert(ad && "Expect array declaration to expand!");
	    if (ad->SubType()->Type() != Types::Char)
	    {
		return ErrorF("Invalid type argument for write");
	    }
	    llvm::Type* pty = llvm::PointerType::getUnqual(Types::GetType(Types::Char));
	    argTypes.push_back(pty);
	    llvm::Type* t = Types::GetType(Types::Integer); 
	    argTypes.push_back(t);
	    suffix = "chars";
	    break;
	}
	default:
	    ty->dump();
	    assert(0);
	    return ErrorF("Invalid type argument for write");
	}
    }
    else
    {
	suffix = "nl";
    }
    std::string name = std::string("__write_") + suffix;
    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
    llvm::Constant* f = theModule->getOrInsertFunction(name, ft);

    return f;
}

static llvm::Constant *CreateWriteBinFunc(llvm::Type* ty, llvm::Type* fty)
{
    std::vector<llvm::Type*> argTypes;
    llvm::Type* resTy = Types::GetType(Types::Void);
    argTypes.push_back(fty);
    assert(ty && "Type should not be NULL!");
    if (!ty->isPointerTy())
    {
	return ErrorF("Write argument is not a variable type!");
    }
    llvm::Type* voidPtrTy = Types::GetVoidPtrType();
    argTypes.push_back(voidPtrTy);

    std::string name = std::string("__write_bin");
    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
    llvm::Constant* f = theModule->getOrInsertFunction(name, ft);

    return f;
}

llvm::Value* WriteAST::CodeGen()
{
    TRACE();
    llvm::Type* voidPtrTy = Types::GetVoidPtrType();
    llvm::Value* f = FileOrNull(file);

    bool isText = FileIsText(f);
    for(auto arg: args)
    {
	std::vector<llvm::Value*> argsV;
	llvm::Value*    v;
	llvm::Constant* fn;
	argsV.push_back(f);
	if (isText)
	{
	    Types::TypeDecl* type = arg.expr->Type();
	    fn = CreateWriteFunc(type, f->getType());
	    if (type->Type() == Types::String)
	    {
		AddressableAST* a = llvm::dyn_cast<AddressableAST>(arg.expr);
		assert(a && "Expected addressable value");
		v = a->Address();
		std::vector<llvm::Value*> ind;
		ind.push_back(MakeIntegerConstant(0));
		ind.push_back(MakeIntegerConstant(0));
		v = builder.CreateGEP(v, ind, "str_addr");
	    }
	    else if (type->Type() == Types::Array && 
		     type->SubType()->Type() == Types::Char &&
		     !llvm::isa<StringExprAST>(arg.expr))
	    {
		AddressableAST* a = llvm::dyn_cast<AddressableAST>(arg.expr);
		assert(a && "Expected addressable value");
		v = a->Address();
		std::vector<llvm::Value*> ind;
		ind.push_back(MakeIntegerConstant(0));
		ind.push_back(MakeIntegerConstant(0));
		v = builder.CreateGEP(v, ind, "str_addr");
	    }
	    else
	    {
		v = arg.expr->CodeGen();
	    }
	    if (!v)
	    {
		return ErrorV("Argument codegen failed");
	    }
	    argsV.push_back(v);
	    llvm::Value*    w;
	    if (!arg.width)
	    {
		if (type->Type() == Types::Integer)
		{
		    w = MakeIntegerConstant(13);
		}
		else if (type->Type() == Types::Real)
		{
		    w = MakeIntegerConstant(15);
		}
		else
		{
		    w = MakeIntegerConstant(0);
		}
	    }   
	    else
	    {
		w = arg.width->CodeGen();
		assert(w && "Expect width expression to generate code ok");
	    }

	    if (!w->getType()->isIntegerTy())
	    {
		return ErrorV("Expected width to be integer value");
	    }
	    argsV.push_back(w);
	    if (type->Type() == Types::Real)
	    {
		llvm::Value* p;
		if (arg.precision)
		{
		    p = arg.precision->CodeGen();
		    if (!p->getType()->isIntegerTy())
		    {
			return ErrorV("Expected precision to be integer value");
		    }
		}
		else
		{
		    p = MakeIntegerConstant(-1);
		}
		argsV.push_back(p);
	    }
	}
	else
	{
	    VariableExprAST* vexpr = llvm::dyn_cast<VariableExprAST>(arg.expr);
	    if (!vexpr)
	    {
		return ErrorV("Argument for write should be a variable");
	    }
	    v = vexpr->Address();
	    v = builder.CreateBitCast(v, voidPtrTy);
	    argsV.push_back(v);
	    llvm::Type *ty = v->getType();
	    fn = CreateWriteBinFunc(ty, f->getType());
	}
	builder.CreateCall(fn, argsV, "");
    }
    if (isWriteln)
    {
	llvm::Constant* fn = CreateWriteFunc(0, f->getType());
	builder.CreateCall(fn, f, "");
    }
    return MakeIntegerConstant(0);
}

void ReadAST::DoDump(std::ostream& out) const
{
    if (isReadln)
    {
	out << "Readln(";
    }
    else
    {
	out << "Read(";
    }
    bool first = true;
    for(auto a : args)
    {
	if (!first)
	{
	    out << ", ";
	}
	first = false;
	a->dump(out);
    }
    out << ")";
}

static llvm::Constant *CreateReadFunc(llvm::Type* ty, llvm::Type* fty)
{
    std::string suffix;
    std::vector<llvm::Type*> argTypes;
    llvm::Type* resTy = Types::GetType(Types::Void);
    argTypes.push_back(fty);
    if (ty)
    {
	if (!ty->isPointerTy())
	{
	    return ErrorF("Read argument is not a variable type!");
	}
	llvm::Type* innerTy = ty->getContainedType(0);
	if (innerTy == Types::GetType(Types::Char))
	{
	    argTypes.push_back(ty);
	    suffix = "chr";
	}
	else if (innerTy->isIntegerTy())
	{
	    // Make args of two integers. 
	    argTypes.push_back(ty);
	    suffix = "int";
	}
	else if (innerTy->isDoubleTy())
	{
	    // Args: double, int, int
	    argTypes.push_back(ty);
	    suffix = "real";
	}
	else
	{
	    return ErrorF("Invalid type argument for read");
	}
    }
    else
    {
	suffix = "nl";
    }
    std::string name = std::string("__read_") + suffix;
    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
    llvm::Constant* f = theModule->getOrInsertFunction(name, ft);

    return f;
}

static llvm::Constant *CreateReadBinFunc(llvm::Type* ty, llvm::Type* fty)
{
    std::vector<llvm::Type*> argTypes;
    llvm::Type* resTy = Types::GetType(Types::Void);
    argTypes.push_back(fty);
    assert(ty && "Type should not be NULL!");
    if (!ty->isPointerTy())
    {
	return ErrorF("Read argument is not a variable type!");
    }
    llvm::Type* voidPtrTy = Types::GetVoidPtrType();
    argTypes.push_back(voidPtrTy);

    std::string name = std::string("__read_bin");
    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
    llvm::Constant* f = theModule->getOrInsertFunction(name, ft);

    return f;
}

llvm::Value* ReadAST::CodeGen()
{
    TRACE();
    llvm::Type* voidPtrTy = Types::GetVoidPtrType();
    llvm::Value* f = FileOrNull(file);

    bool isText = FileIsText(f);
    for(auto arg: args)
    {
	std::vector<llvm::Value*> argsV;
	argsV.push_back(f);
	VariableExprAST* vexpr = llvm::dyn_cast<VariableExprAST>(arg);
	if (!vexpr)
	{
	    return ErrorV("Argument for read/readln should be a variable");
	}
	
	llvm::Value* v = vexpr->Address();
	if (!v)
	{
	    return 0;
	}
	if (!isText)
	{
	    v = builder.CreateBitCast(v, voidPtrTy);
	}
	argsV.push_back(v);
	llvm::Type *ty = v->getType();
	llvm::Constant* fn;
	if (isText)
	{
	    fn = CreateReadFunc(ty, f->getType());
	}
	else
	{
	    fn = CreateReadBinFunc(ty, f->getType());
	}
	
	builder.CreateCall(fn, argsV, "");
    }
    if (isReadln)
    {
	if (!isText)
	{
	    return ErrorV("File is not text for readln");
	}
	llvm::Constant* fn = CreateReadFunc(0, f->getType());
	builder.CreateCall(fn, f, "");
    }
    return MakeIntegerConstant(0);
}

void VarDeclAST::DoDump(std::ostream& out) const
{
    out << "Var ";
    for(auto v : vars)
    {
	v.dump();
	out << std::endl;
    }
}

llvm::Value* VarDeclAST::CodeGen()
{
    TRACE();
    // Are we declaring global variables  - no function!
    llvm::Value *v = 0;
    for(auto var : vars)
    {
	if (!func)
	{
	    llvm::Type *ty = var.Type()->LlvmType();
	    assert(ty && "Type should have a value");
	    llvm::Constant *init = llvm::Constant::getNullValue(ty);
	    v = new llvm::GlobalVariable(*theModule, ty, false, 
					 llvm::Function::InternalLinkage, init, var.Name().c_str());
	}
	else
	{
	    v = CreateAlloca(func, var);
	}
	if (!variables.Add(var.Name(), v))
	{
	    return ErrorV(std::string("Duplicate name ") + var.Name() + "!");
	}
    }
    return v;
}

void LabelExprAST::DoDump(std::ostream& out) const
{
    bool first = false;
    for(auto l : labelValues)
    {
	if (!first)
	{
	    out << ", ";
	}
	out << l;
    }
    out << ": ";
    stmt->dump(out);
}

llvm::Value* LabelExprAST::CodeGen(llvm::SwitchInst* sw, llvm::BasicBlock* afterBB, llvm::Type* ty)
{
    TRACE();
    llvm::Function *theFunction = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "case", theFunction);
    
    builder.SetInsertPoint(caseBB);
    stmt->CodeGen();
    builder.CreateBr(afterBB);
    for(auto l : labelValues)
    {
	llvm::IntegerType* intTy = llvm::dyn_cast<llvm::IntegerType>(ty);
	sw->addCase(llvm::ConstantInt::get(intTy, l), caseBB);
    }
    return caseBB;
}

void CaseExprAST::DoDump(std::ostream& out) const
{
    out << "Case ";
    expr->dump(out);
    out << " of " << std::endl;
    for(auto l : labels)
    {
	l->dump(out);
    }
    if (otherwise)
    {
	out << "otherwise: ";
	otherwise->dump();
    }
}

llvm::Value* CaseExprAST::CodeGen()
{
    TRACE();
    llvm::Value* v  = expr->CodeGen();
    llvm::Type*  ty = v->getType();
    if (!v->getType()->isIntegerTy())
    {
	return ErrorV("Case selection must be integral type");
    }

    llvm::Function *theFunction = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "after", theFunction);    
    llvm::BasicBlock* defaultBB = afterBB;
    if (otherwise)
    {
	defaultBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "default", theFunction);    
	
    }	
    llvm::SwitchInst* sw = builder.CreateSwitch(v, defaultBB, labels.size());
    for(auto ll : labels)
    {
	ll->CodeGen(sw, afterBB, ty);
    }

    if (otherwise)
    {
	builder.SetInsertPoint(defaultBB);
	otherwise->CodeGen();
	builder.CreateBr(afterBB);
    }

    builder.SetInsertPoint(afterBB);
    
    return afterBB;
}

void RangeExprAST::DoDump(std::ostream& out) const
{
    out << "Range:";
    low->dump(out); 
    out << "..";
    high->dump(out);
}

llvm::Value* RangeExprAST::CodeGen()
{
    TRACE();
    return 0;
}

void SetExprAST::DoDump(std::ostream& out) const
{
    out << "Set :[";
    bool first = true;
    for(auto v : values)
    {
	if (!first)
	{
	    out << ", ";
	}
	first = false;
	v->dump(out);
    }
    out << "]";
}

llvm::Value* SetExprAST::Address()
{
    TRACE();

    llvm::Type* ty = Types::TypeForSet()->LlvmType();
    assert(ty && "Expect type for set to work");

    llvm::Value* setV = CreateTempAlloca(ty);
    assert(setV && "Expect CreateTempAlloca() to work");

    llvm::Value* tmp = builder.CreateBitCast(setV, Types::GetVoidPtrType());

    builder.CreateMemSet(tmp, MakeConstant(0, Types::GetType(Types::Char)), 
			 Types::SetDecl::MaxSetWords * 4, 0);

    // TODO: For optimisation, we may want to pass through the vector and see if the values 
    // are constants, and if so, be clever about it.
    // Also, we should combine stores to the same word!
    for(auto v : values)
    {
	// If we have a "range", then make a loop. 
	RangeExprAST* r = llvm::dyn_cast<RangeExprAST>(v);
	if (r)
	{
	    std::vector<llvm::Value*> ind;
	    llvm::Value* low = r->Low();
	    llvm::Value* high = r->High();
	    llvm::Function *fn = builder.GetInsertBlock()->getParent();

	    if (!low || !high)
	    {
		return 0;
	    }

	    low  = builder.CreateZExt(low, Types::GetType(Types::Integer), "zext.low");
	    high = builder.CreateZExt(high, Types::GetType(Types::Integer), "zext.high");

	    llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "loop", fn);    
	    builder.CreateBr(loopBB);
	    builder.SetInsertPoint(loopBB);

	    // Set bit "low" in set. 
	    llvm::Value* index = builder.CreateLShr(low, MakeIntegerConstant(5));
	    llvm::Value* offset = builder.CreateAnd(low, MakeIntegerConstant(31));
	    llvm::Value* bit = builder.CreateShl(MakeIntegerConstant(1), offset);
	    ind.push_back(MakeIntegerConstant(0));
	    ind.push_back(index);
	    llvm::Value *bitsetAddr = builder.CreateGEP(setV, ind, "bitsetaddr");
	    llvm::Value *bitset = builder.CreateLoad(bitsetAddr);
	    bitset = builder.CreateOr(bitset, bit);
	    builder.CreateStore(bitset, bitsetAddr);
	    
	    low = builder.CreateAdd(low, MakeConstant(1, low->getType()), "update");

	    llvm::Value* endCond = builder.CreateICmpSGE(low, high, "loopcond");

	    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "afterloop", fn);
	    builder.CreateCondBr(endCond, loopBB, afterBB);

	    builder.SetInsertPoint(afterBB);
	}
	else
	{
	    std::vector<llvm::Value*> ind;
	    llvm::Value* x = v->CodeGen();
	    if (!x)
	    {
		return 0;
	    }
	    x = builder.CreateZExt(x, Types::GetType(Types::Integer), "zext");
	    llvm::Value* index = builder.CreateLShr(x, MakeIntegerConstant(5));
	    llvm::Value* offset = builder.CreateAnd(x, MakeIntegerConstant(31));
	    llvm::Value* bit = builder.CreateShl(MakeIntegerConstant(1), offset);
	    ind.push_back(MakeIntegerConstant(0));
	    ind.push_back(index);
	    llvm::Value *bitsetAddr = builder.CreateGEP(setV, ind, "bitsetaddr");
	    llvm::Value *bitset = builder.CreateLoad(bitsetAddr);
	    bitset = builder.CreateOr(bitset, bit);
	    builder.CreateStore(bitset, bitsetAddr);
	}
    }
    
    return setV;
}

llvm::Value* SetExprAST::CodeGen()
{
    llvm::Value* v = Address();
    return builder.CreateLoad(v);
}

void WithExprAST::DoDump(std::ostream& out) const
{
    out << "With ... do " << std::endl;
}

llvm::Value* WithExprAST::CodeGen()
{
    return body->CodeGen();
}


int GetErrors(void)
{
    return errCnt;
}
