#include "expr.h"
#include "stack.h"
#include "builtin.h"
#include "options.h"
#include "trace.h"
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
#include <algorithm>
#include <cctype>

template<>
void Stack<llvm::Value*>::dump(std::ostream& out) const
{
    int n = 0;
    for(auto s : stack)
    {
	out << "Level " << n << std::endl;
	n++;
	for(auto v : s)
	{
	    out << v.first << ": ";
	    v.second->dump();
	    out << std::endl;
	}
    }
}

const size_t MEMCPY_THRESHOLD = 16;
const size_t MIN_ALIGN = 4;

extern llvm::Module* theModule;

typedef Stack<llvm::Value*> VarStack;
typedef StackWrapper<llvm::Value*> VarStackWrapper;

class MangleMap
{
public:
    MangleMap(const std::string& name)
	: actualName(name) {}
    void dump(std::ostream& out)
    {
	out << "Name: " << actualName << std::endl;
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

static bool IsConstant(ExprAST* e)
{
    if (llvm::isa<IntegerExprAST>(e) ||
	llvm::isa<CharExprAST>(e))
    {
	return true;
    }
    return false;
}

size_t AlignOfType(llvm::Type* ty)
{
    const llvm::DataLayout dl(theModule);
    return dl.getPrefTypeAlignment(ty);
}

bool FileInfo(llvm::Value* f, int& recSize, bool& isText)
{
    if (!f->getType()->isPointerTy())
    {
	return false;
    }
    llvm::Type* ty = f->getType()->getContainedType(0);
    llvm::StructType* st = llvm::dyn_cast<llvm::StructType>(ty);

    assert(st && "Should be a StructType at this point");

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

static llvm::AllocaInst* CreateNamedAlloca(llvm::Function* fn, llvm::Type* ty, const std::string& name)
{
    TRACE();
   /* Save where we were... */
    llvm::BasicBlock* bb = builder.GetInsertBlock();
    llvm::BasicBlock::iterator ip = builder.GetInsertPoint();

    llvm::IRBuilder<> bld(&fn->getEntryBlock(), fn->getEntryBlock().begin());
    assert(ty && "Must have type passed in");

    llvm::AllocaInst* a = bld.CreateAlloca(ty, 0, name);
    if (a->getAlignment() < MIN_ALIGN)
    {
	a->setAlignment(MIN_ALIGN);
    }

    // Now go back to where we were...
    builder.SetInsertPoint(bb, ip);
    return a;
}

static llvm::AllocaInst* CreateAlloca(llvm::Function* fn, const VarDef& var)
{
    if (Types::FieldCollection* fc = llvm::dyn_cast<Types::FieldCollection>(var.Type()))
    {
	fc->EnsureSized();
    }

    llvm::Type* ty = var.Type()->LlvmType();

    return CreateNamedAlloca(fn, ty, var.Name());
}

static llvm::AllocaInst* CreateTempAlloca(llvm::Type* ty)
{
    /* Get the "entry" block */
    llvm::Function* fn = builder.GetInsertBlock()->getParent();

    return CreateNamedAlloca(fn, ty, "tmp");
}

llvm::Value* MakeAddressable(ExprAST* e)
{
    if (AddressableAST* ea = llvm::dyn_cast<AddressableAST>(e))
    {
	llvm::Value* v = ea->Address();
	assert(v && "Expect address to be non-zero");
	return v;
    }
    else
    {
	llvm::Value* store = e->CodeGen();
	if (store->getType()->isPointerTy())
	{
	    return store;
	}

	llvm::Value* v = CreateTempAlloca(store->getType());
	assert(v && "Expect address to be non-zero");
	builder.CreateStore(store, v);
	return v;
    }
}

void ExprAST::dump(std::ostream& out) const
{
    out << "Node=" << reinterpret_cast<const void*>(this) << ": ";
    DoDump(out);
    out << std::endl;
}	

void ExprAST::dump(void) const
{
    dump(std::cerr);
}

llvm::Value* ErrorV(const std::string& msg)
{
    std::cerr << msg << std::endl;
    errCnt++;
    return 0;
}

static llvm::Function* ErrorF(const std::string& msg)
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

static llvm::Value* TempStringFromStringExpr(llvm::Value* dest, StringExprAST* rhs)
{
    TRACE();
    std::vector<llvm::Value*> ind{MakeIntegerConstant(0),MakeIntegerConstant(0)};
    llvm::Value* dest1 = builder.CreateGEP(dest, ind, "str_0");

    ind[1] = MakeIntegerConstant(1);
    llvm::Value* dest2 = builder.CreateGEP(dest, ind, "str_1");

    llvm::Value* v = rhs->CodeGen();
    builder.CreateStore(MakeCharConstant(rhs->Str().size()), dest1);

    return builder.CreateMemCpy(dest2, v, rhs->Str().size(), std::max(AlignOfType(v->getType()), MIN_ALIGN));
}

static llvm::Value* TempStringFromChar(llvm::Value* dest, ExprAST* rhs)
{
    TRACE();
    if (rhs->Type()->Type() != Types::Char)
    {
	return ErrorV("Expected char value");
    }
    std::vector<llvm::Value*> ind{MakeIntegerConstant(0), MakeIntegerConstant(0)};
    llvm::Value* dest1 = builder.CreateGEP(dest, ind, "str_0");

    ind[1] = MakeIntegerConstant(1);
    llvm::Value* dest2 = builder.CreateGEP(dest, ind, "str_1");

    builder.CreateStore(MakeCharConstant(1), dest1);
    return builder.CreateStore(rhs->CodeGen(), dest2);
}

static llvm::Value* LoadOrMemcpy(llvm::Value* src, Types::TypeDecl* ty)
{
    llvm::Value* dest = CreateTempAlloca(ty->LlvmType());
    size_t size = ty->Size();
    if (!disableMemcpyOpt && size >= MEMCPY_THRESHOLD)
    {
	size_t align = ty->AlignSize();
	builder.CreateMemCpy(dest, src, size, std::max(align, MIN_ALIGN));
	return dest;
    }
    else
    {
	llvm::Value* v = builder.CreateLoad(src);
	builder.CreateStore(v, dest);
	return dest;
    }
    assert(0 && "We don't want to get here!");
    return 0;
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
    if (Types::FieldCollection* fc = llvm::dyn_cast_or_null<Types::FieldCollection>(Type()))
    {
	fc->EnsureSized();
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
    out << "Nil:";
}

llvm::Value* NilExprAST::CodeGen()
{
    TRACE();
    return llvm::ConstantPointerNull::get(llvm::dyn_cast<llvm::PointerType>(Types::GetVoidPtrType()));
}

void CharExprAST::DoDump(std::ostream& out) const
{
    out << "Char: '";
    if (isprint(val))
	out << char(val);
    else
	out << "\\0x" << std::hex << val << std::dec;
    out << "'";
}

llvm::Value* CharExprAST::CodeGen()
{
    TRACE();
    llvm::Value* v = MakeCharConstant(val);
    return v;
}

llvm::Value* AddressableAST::CodeGen()
{
    TRACE();
    llvm::Value* v = Address();
    assert(v && "Expected to get an address");
    return builder.CreateLoad(v);
}

void VariableExprAST::DoDump(std::ostream& out) const
{
    out << "Variable: " << name << " ";
    Type()->dump(out);
}

llvm::Value* VariableExprAST::CodeGen()
{
    TRACE();
    llvm::Value* v = Address();
    assert(v && "Expected to get an address");
    return builder.CreateLoad(v, name);
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

ArrayExprAST::ArrayExprAST(const Location& loc,
			   VariableExprAST* v,
			   const std::vector<ExprAST*>& inds,
			   const std::vector<Types::RangeDecl*>& r,
			   Types::TypeDecl* ty)
    : VariableExprAST(loc, EK_ArrayExpr, v, ty), expr(v), indices(inds), ranges(r)
{
    size_t mul = 1;
    for(auto j = ranges.end()-1; j >= ranges.begin(); j--)
    {
	indexmul.push_back(mul);
	mul *= (*j)->GetRange()->Size();
    }
    std::reverse(indexmul.begin(), indexmul.end());
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
    llvm::Value* totalIndex = 0;
    for(size_t i = 0; i < indices.size(); i++)
    {
	llvm::Value* index;
	assert(llvm::isa<RangeReduceAST>(indices[i]));
	index = indices[i]->CodeGen();
	assert(index && "Expression failed for index");
	llvm::Type* ty = index->getType();
	if (indexmul[i] != 1)
	{
	    index = builder.CreateMul(index, MakeConstant(indexmul[i], ty));
	}
	if (!totalIndex)
	{
	    totalIndex = index;
	}
	else
	{
	    totalIndex = builder.CreateAdd(totalIndex, index);
	}
    }
    std::vector<llvm::Value*> ind{MakeIntegerConstant(0), totalIndex};
    v = builder.CreateGEP(v, ind, "valueindex");
    return v;
}

void ArrayExprAST::accept(Visitor& v)
{
    for(auto i : indices)
    {
	i->accept(v);
    }
    expr->accept(v);
    v.visit(this);
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
    if (llvm::Value* v = expr->Address())
    {
	std::vector<llvm::Value*> ind{MakeIntegerConstant(0), MakeIntegerConstant(element)};
	v = builder.CreateGEP(v, ind, "valueindex");
	return v;
    }
    return ErrorV("Expression did not form an address");
}

void FieldExprAST::accept(Visitor& v)
{
    expr->accept(v);
    v.visit(this);
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
    std::vector<llvm::Value*> ind{MakeIntegerConstant(0), MakeIntegerConstant(element)};
    v = builder.CreateGEP(v, ind, "valueindex");
    v = builder.CreateBitCast(v, llvm::PointerType::getUnqual(Type()->LlvmType()));

    return v;
}

void PointerExprAST::DoDump(std::ostream& out) const
{
    out << "Pointer:";
    pointer->dump(out);
}

llvm::Value* PointerExprAST::Address()
{
    TRACE();
    EnsureSized();
    return pointer->CodeGen();
}

void PointerExprAST::accept(Visitor& v)
{
    pointer->accept(v);
    v.visit(this);
}

void FilePointerExprAST::DoDump(std::ostream& out) const
{
    out << "FilePointer:";
    pointer->dump(out);
}

llvm::Value* FilePointerExprAST::Address()
{
    TRACE();
    VariableExprAST* vptr = llvm::dyn_cast<VariableExprAST>(pointer);
    llvm::Value* v = vptr->Address();
    std::vector<llvm::Value*> ind{MakeIntegerConstant(0), MakeIntegerConstant(Types::FileDecl::Buffer)};
    v = builder.CreateGEP(v, ind, "bufptr");
    v = builder.CreateLoad(v, "buffer");
    return v;
}

void FilePointerExprAST::accept(Visitor& v)
{
    pointer->accept(v);
    v.visit(this);
}

void FunctionExprAST::DoDump(std::ostream& out) const
{
    out << "Function " << name;
}

llvm::Value* FunctionExprAST::CodeGen()
{
    if (MangleMap* mm = mangles.Find(name))
    {
	std::string actualName = mm->Name();
	return theModule->getFunction(actualName);
    }
    return ErrorV(std::string("Name ") + name + " could not be found...");
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

static llvm::Value *SetOperation(const std::string& name, llvm::Value* res, llvm::Value *src)
{

    if (name == "Union")
    {
	return builder.CreateOr(res, src);
    }
    if (name == "Intersect")
    {
	return builder.CreateAnd(res, src);
    }
    if (name == "Diff")
    {
	src = builder.CreateNot(src);
	return builder.CreateAnd(res, src);
    }
    return 0;
}

llvm::Value* BinaryExprAST::InlineSetFunc(const std::string& name, bool resTyIsSet)
{
    if (optimization >= O1 && (name == "Union" || name == "Intersect" || name == "Diff"))
    {
	Types::TypeDecl* type = rhs->Type();

	assert(*type == *lhs->Type() && "Expect same types");
	llvm::Value* rV = MakeAddressable(rhs);
	llvm::Value* lV = MakeAddressable(lhs);

	size_t words = llvm::dyn_cast<Types::SetDecl>(type)->SetWords();
	assert(rV && lV && "Should have generated values for left and right set");

	llvm::Value* v = CreateTempAlloca(type->LlvmType());
	for(size_t i = 0; i < words; i++)
	{
	    std::vector<llvm::Value*> ind{MakeIntegerConstant(0), MakeIntegerConstant(i)};
	    llvm::Value* lAddr = builder.CreateGEP(lV, ind, "leftSet");
	    llvm::Value* rAddr = builder.CreateGEP(rV, ind, "rightSet");
	    llvm::Value* vAddr = builder.CreateGEP(v, ind, "resSet");
	    llvm::Value* res = builder.CreateLoad(lAddr);
	    llvm::Value* tmp = builder.CreateLoad(rAddr);
	    res = SetOperation(name, res, tmp);
	    builder.CreateStore(res, vAddr);
	}
	return builder.CreateLoad(v);
    }
    return 0;
}

llvm::Value* BinaryExprAST::CallSetFunc(const std::string& name, bool resTyIsSet)
{
    TRACE();

    if (llvm::Value* inl = InlineSetFunc(name, resTyIsSet))
    {
	return inl;
    }

    std::string func = std::string("__Set") + name;
    Types::TypeDecl* type = rhs->Type();
    assert(*type == *lhs->Type() && "Expect both sides to have same type" );

    assert(type && "Expect to get a type");

    llvm::Value* rV = MakeAddressable(rhs);
    llvm::Value* lV = MakeAddressable(lhs);

    assert(rV && lV && "Should have generated values for left and right set");

    llvm::Type* pty = llvm::PointerType::getUnqual(type->LlvmType());
    llvm::Type* intTy = Types::GetType(Types::Integer);
    llvm::Value* setWords = MakeIntegerConstant(llvm::dyn_cast<Types::SetDecl>(type)->SetWords());
    if (resTyIsSet)
    {
	llvm::Type* resTy = Types::GetType(Types::Void);
	std::vector<llvm::Type*> argTypes{pty, pty, pty, intTy};
	
	llvm::Value* v = CreateTempAlloca(type->LlvmType());
	llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction(func, ft);
	
	builder.CreateCall4(f, v, lV, rV, setWords);
	return builder.CreateLoad(v);
    }
    else
    {
	llvm::Type* resTy = Types::GetType(Types::Boolean);
	std::vector<llvm::Type*> argTypes{pty, pty, intTy};
	
	llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
	llvm::Constant* f = theModule->getOrInsertFunction(func, ft);
	
	return builder.CreateCall3(f, lV, rV, setWords, "calltmp");
    }

}

static llvm::Value* MakeStringFromExpr(ExprAST* e, llvm::Type*& ty)
{
    TRACE();
    llvm::Value* v;
    llvm::Type* strTy = Types::GetStringType()->LlvmType();
    if (e->Type()->Type() == Types::Char)
    {
	Types::StringDecl* sd = Types::GetStringType();
	if (!ty)
	{
	    ty = sd->LlvmType();
	}
	v = CreateTempAlloca(ty);
	TempStringFromChar(v, e);
    }
    else if (StringExprAST* se = llvm::dyn_cast<StringExprAST>(e))
    {
	Types::StringDecl* sd = Types::GetStringType();
	if (!ty)
	{
	    ty = sd->LlvmType();
	}
	v = CreateTempAlloca(sd->LlvmType());
	TempStringFromStringExpr(v, se);
    }
    else
    {
	if (!ty)
	{
	    ty = e->Type()->LlvmType();
	}
	v = MakeAddressable(e);
	if (ty != strTy)
	{
	    v = builder.CreateBitCast(v, llvm::PointerType::getUnqual(strTy));
	}
    }
    return v;
}

static llvm::Value* CallStrFunc(const std::string& name, ExprAST* lhs, ExprAST* rhs, llvm::Type* resTy,
				const std::string& twine)
{
    TRACE();
    llvm::Type* ty = NULL;
    llvm::Value* rV = MakeStringFromExpr(rhs, ty);
    llvm::Value* lV = MakeStringFromExpr(lhs, ty);

    if (!rV || !lV)
    {
	return 0;
    }

    llvm::Type* strTy = Types::GetStringType()->LlvmType();
    llvm::Type* pty = llvm::PointerType::getUnqual(strTy);
    std::vector<llvm::Type*> argTypes{pty, pty};

    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
    std::string func = std::string("__Str") + name;
    llvm::Constant* f = theModule->getOrInsertFunction(func, ft);

    return builder.CreateCall2(f, lV, rV, twine);
}

static llvm::Value* CallStrCat(ExprAST* lhs, ExprAST* rhs)
{
    TRACE();
    llvm::Type* ty = NULL;
    llvm::Value* rV = MakeStringFromExpr(rhs, ty);
    llvm::Value* lV = MakeStringFromExpr(lhs, ty);

    llvm::Type* strTy = Types::GetStringType()->LlvmType();

    llvm::Type* pty = llvm::PointerType::getUnqual(strTy);
    std::vector<llvm::Type*> argTypes{pty, pty, pty};

    llvm::Value* v = CreateTempAlloca(strTy);

    llvm::Type* resTy = Types::GetType(Types::Void);
    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
    llvm::Constant* f = theModule->getOrInsertFunction("__StrConcat", ft);
    builder.CreateCall3(f, v, lV, rV, "");
    return v;
}

llvm::Value* BinaryExprAST::CallStrFunc(const std::string& name)
{
    TRACE();
    llvm::Type* resTy = Types::GetType(Types::Integer);
    return ::CallStrFunc(name, lhs, rhs, resTy, "calltmp");
}

llvm::Value* BinaryExprAST::CallArrFunc(const std::string& name, size_t size)
{
    TRACE();
    std::string func = std::string("__Arr") + name;

    llvm::Value* rV = MakeAddressable(rhs);
    llvm::Value* lV = MakeAddressable(lhs);

    if (!rV || !lV)
    {
	return 0;
    }
    // Result is integer.
    llvm::Type* resTy = Types::GetType(Types::Integer);
    llvm::Type* pty = llvm::PointerType::getUnqual(Types::GetType(Types::Char));
    std::vector<llvm::Type*> argTypes{pty, pty, resTy};

    lV = builder.CreateBitCast(lV, pty);
    rV = builder.CreateBitCast(rV, pty);

    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
    llvm::Constant* f = theModule->getOrInsertFunction(func, ft);

    return builder.CreateCall3(f, lV, rV, MakeIntegerConstant(size), "calltmp");
}

static llvm::Value* TypeConvert(llvm::Value* v, llvm::Type* ty, bool isNil = false)
{
    llvm::Type* currentTy = v->getType();
    if (ty->isDoubleTy() && currentTy->isIntegerTy())
    {
	return builder.CreateSIToFP(v, ty, "tofp");
    }
    if (currentTy->isIntegerTy() && ty->isIntegerTy())
    {
	unsigned size = ty->getPrimitiveSizeInBits();
	unsigned curSize = currentTy->getPrimitiveSizeInBits();
	if (curSize > 1 && size > curSize)
	{
	    v = builder.CreateSExt(v, ty);
	}
    }

    if (ty->isPointerTy() && isNil)
    {
	return builder.CreateBitCast(v, ty);
    }
    return v;
}

void BinaryExprAST::UpdateType(Types::TypeDecl* ty)
{
    if (type == ty)
    {
	return;
    }
    assert(!type && "Type shouldn't be updated more than once");
    assert(ty && "Must supply valid type");

    type = ty;
}

Types::TypeDecl* BinaryExprAST::Type() const
{
    Token::TokenType tt = oper.GetToken();
    if (tt >= Token::FirstComparison && tt <= Token::LastComparison)
    {
	return new Types::BoolDecl;
    }

    if (type)
    {
	return type;
    }

    Types::TypeDecl* lType = lhs->Type();
    Types::TypeDecl* rType = rhs->Type();

    if (!rType && lType && lType->Type() == Types::Set)
    {
	return lType;
    }

    assert(rType && lType && "Should have types here...");

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

llvm::Value* BinaryExprAST::SetCodeGen()
{
    TRACE();
    if (lhs->Type() && lhs->Type()->isIntegral() && oper.GetToken() == Token::In)
    {
	llvm::Value* l = lhs->CodeGen();
	AddressableAST* rhsA = llvm::dyn_cast<AddressableAST>(rhs);
	if (!rhsA)
	{
	    return ErrorV("Set value should be addressable!");
	}
	llvm::Value* setV = rhsA->Address();
	Types::TypeDecl* type = rhs->Type();
	int start = type->GetRange()->GetStart();
	l = builder.CreateZExt(l, Types::GetType(Types::Integer), "zext.l");
	l = builder.CreateSub(l, MakeIntegerConstant(start));
	llvm::Value* index;
	if (llvm::dyn_cast<Types::SetDecl>(type)->SetWords() > 1)
	{
	    index = builder.CreateLShr(l, MakeIntegerConstant(Types::SetDecl::SetPow2Bits));
	}
	else
	{
	    index = MakeIntegerConstant(0);
	}
	llvm::Value* offset = builder.CreateAnd(l, MakeIntegerConstant(Types::SetDecl::SetMask));
	std::vector<llvm::Value*> ind{MakeIntegerConstant(0), index};
	llvm::Value* bitsetAddr = builder.CreateGEP(setV, ind, "valueindex");

	llvm::Value* bitset = builder.CreateLoad(bitsetAddr);
	llvm::Value* bit = builder.CreateLShr(bitset, offset);
	return builder.CreateTrunc(bit, Types::GetType(Types::Boolean));
    }
    else if (llvm::isa<SetExprAST>(lhs) || (lhs->Type() && llvm::isa<Types::SetDecl>(lhs->Type())))
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
	    // Swap left<->right sides
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

llvm::Value* MakeStrCompare(const Token& oper, llvm::Value* v)
{
    switch (oper.GetToken())
    {
    case Token::Equal:
	return builder.CreateICmpEQ(v, MakeIntegerConstant(0), "eq");

    case Token::NotEqual:
	return builder.CreateICmpNE(v, MakeIntegerConstant(0), "ne");

    case Token::GreaterOrEqual:
	return builder.CreateICmpSGE(v, MakeIntegerConstant(0), "ge");

    case Token::LessOrEqual:
	return builder.CreateICmpSLE(v, MakeIntegerConstant(0), "le");

    case Token::GreaterThan:
	return builder.CreateICmpSGT(v, MakeIntegerConstant(0), "gt");

    case Token::LessThan:
	return builder.CreateICmpSLT(v, MakeIntegerConstant(0), "lt");

    default:
	return ErrorV("Invalid operand for char arrays");
    }
}


llvm::Value* BinaryExprAST::CodeGen()
{
    TRACE();

    if (verbosity)
    {
	lhs->dump(); oper.dump(); rhs->dump();
    }

    if (llvm::isa<SetExprAST>(rhs) || llvm::isa<SetExprAST>(lhs) ||
	(rhs->Type() && llvm::isa<Types::SetDecl>(rhs->Type())) ||
	(lhs->Type() && llvm::isa<Types::SetDecl>(lhs->Type())))
    {
	return SetCodeGen();
    }

    if (!lhs->Type() || !rhs->Type())
    {
	assert(0 && "Huh? Both sides of expression should have type");
	return ErrorV("One or both sides of binary expression does not have a type...");
    }

    if (BothStringish(lhs, rhs))
    {
	if (oper.GetToken() == Token::Plus)
	{
	    return CallStrCat(lhs, rhs);
	}
	else
	{
	    return MakeStrCompare(oper, CallStrFunc("Compare"));
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

	    std::vector<Types::RangeDecl*> rr = ar->Ranges();
	    std::vector<Types::RangeDecl*> rl = al->Ranges();

	    if (rr.size() == 1 && rl.size() == 1 && rr[0]->Size() == rl[0]->Size())
	    {
		size_t size = rr[0]->Size();

		return MakeStrCompare(oper, CallArrFunc("Compare", size));
	    }
	}
    }

    llvm::Value* l = lhs->CodeGen();
    llvm::Value* r = rhs->CodeGen();

    assert(l && r && "Should have a value for both sides");

    bool isRhsNil = llvm::isa<NilExprAST>(rhs);

    llvm::Type* lty = l->getType();
    // If it's a divide operation, result is real, so convert to real.
    // Fixme: Should move to semantics?
    if (oper.GetToken() == Token::Divide)
    {
	lty = Types::GetType(Types::Real);
    }

    r = TypeConvert(r, lty, isRhsNil);
    llvm::Type* rty = r->getType();
    l = TypeConvert(l, rty);
    lty = l->getType();

    assert(rty == lty && "Expect same types");

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
	bool isUnsigned = rhs->Type()->isUnsigned();
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
	case Token::And:
	    return builder.CreateAnd(l, r, "and");
	case Token::Or:
	    return builder.CreateOr(l, r, "or");

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
	assert(0 && "Should not get here!");
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
	proto->dump();
	return ErrorV(std::string("Incorrect number of arguments for ") + proto->Name() + ".");
    }

    std::vector<llvm::Value*> argsV;
    std::vector<VarDef>::const_iterator viter = vdef.begin();
    std::vector<std::pair<int, llvm::Attribute::AttrKind>> argAttr;
    unsigned index = 0;
    for(auto i : args)
    {
	index++;
	llvm::Value* v = 0;
	VariableExprAST* vi = llvm::dyn_cast<VariableExprAST>(i);
	if (viter->IsRef())
	{
	    if (!vi)
	    {
		// TODO: Need to detect this better.
		return ErrorV("Args declared with 'var' must be a variable!");
	    }
	    v = vi->Address();
	}
	else
	{
	    StringExprAST* sv = llvm::dyn_cast<StringExprAST>(i);
	    if (llvm::isa<Types::StringDecl>(viter->Type()) &&
		sv && llvm::isa<Types::ArrayDecl>(sv->Type()))
	    {
		if (sv && sv->Type()->SubType()->Type() == Types::Char)
		{
		    v = CreateTempAlloca(viter->Type()->LlvmType());
		    TempStringFromStringExpr(v, sv);
		}
	    }

	    if (!v)
	    {
		if (vi && vi->Type()->isCompound())
		{
		    v = LoadOrMemcpy(vi->Address(), vi->Type());
		    argAttr.push_back(std::make_pair(index, llvm::Attribute::ByVal));
		}
		else
		{
		    v = i->CodeGen();
		    if (!v)
		    {
			return 0;
		    }
		    // TODO: This should be redundant.
		    v = TypeConvert(v, viter->Type()->LlvmType());
		}
	    }
	}
	if (vi && llvm::isa<Types::StringDecl>(vi->Type()) &&
	    llvm::isa<Types::StringDecl>(viter->Type()))
	{
	    llvm::Type* strTy = viter->Type()->LlvmType();
	    v = builder.CreateBitCast(v, llvm::PointerType::getUnqual(strTy));
	}
	if (!v)
	{
	    return ErrorV("Invalid argument for " + proto->Name() + " (" + i->ToString() + ")");
	}
	
	argsV.push_back(v);
	viter++;
    }
    llvm::CallInst* inst = 0;
    if (proto->Type()->Type() == Types::Void)
    {
	inst = builder.CreateCall(calleF, argsV, "");
    }
    else
    {
	inst = builder.CreateCall(calleF, argsV, "calltmp");
    }
    for(auto v : argAttr)
    {
	inst->addAttribute(v.first, v.second);
    }
    return inst;
}

void CallExprAST::accept(Visitor& v)
{
    for(auto i : args)
    {
	i->accept(v);
    }
    v.visit(this);
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

void BlockAST::accept(Visitor& v)
{
    for(auto i : content)
    {
	i->accept(v);
    }
}

llvm::Value* BlockAST::CodeGen()
{
    TRACE();
    llvm::Value* v = 0;
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
	i.dump(out);
	out << std::endl;
    }
    out << ")";
}

llvm::Function* PrototypeAST::CodeGen(const std::string& namePrefix)
{
    TRACE();
    assert(namePrefix != "" && "Prefix should never be empty");
    std::vector<std::pair<int, llvm::Attribute::AttrKind>> argAttr;
    std::vector<llvm::Type*> argTypes;
    unsigned index = 0;
    for(auto i : args)
    {
	llvm::AttrBuilder attrs;
	Types::TypeDecl* ty = i.Type();
	llvm::Type* argTy = ty->LlvmType();
	index++;
	if (!ty)
	{
	    return ErrorF(std::string("Invalid type for argument") + i.Name() + "...");
	}
	
	if (i.IsRef() || ty->isCompound() )
	{
	    argTy = llvm::PointerType::getUnqual(ty->LlvmType());
	    if (!i.IsRef())
	    {
		argAttr.push_back(std::make_pair(index, llvm::Attribute::ByVal));
	    }
	}

	argTypes.push_back(argTy);
    }
    llvm::Type* resTy = type->LlvmType();
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

    for(auto v : argAttr)
    {
	f->addAttribute(v.first, v.second);
    }

    return f;
}

llvm::Function* PrototypeAST::CodeGen()
{
    return CodeGen("P");
}

void PrototypeAST::CreateArgumentAlloca(llvm::Function* fn)
{
    TRACE();

    unsigned idx = 0;
    for(llvm::Function::arg_iterator ai = fn->arg_begin(); idx < args.size(); idx++, ai++)
    {
	llvm::Value* a;
	if (args[idx].IsRef() || args[idx].Type()->isCompound())
	{
	    a = ai;
	}
	else
	{
	    a = CreateAlloca(fn, args[idx]);
	    builder.CreateStore(ai, a);
	}
	if (!variables.Add(args[idx].Name(), a))
	{
	    ErrorF(std::string("Duplicate variable name ") + args[idx].Name());
	}
    }
    if (type->Type() != Types::Void)
    {
	llvm::AllocaInst* a=CreateAlloca(fn, VarDef(name, type));
	if(!variables.Add(name, a))
	{
	    ErrorF(std::string("Duplicate function result name ") + name);
	}
    }
}

void PrototypeAST::AddExtraArgsLast(const std::vector<VarDef>& extra)
{
    for(auto v : extra)
    {
	args.push_back(VarDef(v.Name(), v.Type(), true));
    }
}

void PrototypeAST::AddExtraArgsFirst(const std::vector<VarDef>& extra)
{
    std::vector<VarDef> newArgs;
    for(auto v : extra)
    {
	newArgs.push_back(VarDef(v.Name(), v.Type(), true));
    }
    for(auto v : args)
    {
	newArgs.push_back(v);
    }
    args.swap(newArgs);
}

void FunctionAST::DoDump(std::ostream& out) const
{
    out << "Function: " << std::endl;
    proto->dump(out);
    out << "Function body:" << std::endl;
    body->dump(out);
}

void FunctionAST::accept(Visitor& v)
{
    for(auto i : subFunctions)
    {
	i->accept(v);
    }
    if (varDecls)
    {
	varDecls->accept(v);
    }
    if (body)
    {
	body->accept(v);
    }
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

    llvm::BasicBlock* bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", theFunction);
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
	variables.dump(std::cerr);
	mangles.dump(std::cerr);
    }

    builder.SetInsertPoint(bb, ip);
    llvm::Value* block = body->CodeGen();
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

    verifyFunction(*theFunction);
    return theFunction;
}

llvm::Function* FunctionAST::CodeGen()
{
    return CodeGen("P");
}

void FunctionAST::SetUsedVars(const std::vector<NamedObject*>& varsUsed,
			      const Stack<NamedObject*>& nameStack)
{
    std::map<std::string, NamedObject*> nonLocal;
    size_t maxLevel = nameStack.MaxLevel();

    if (verbosity > 1)
    {
	nameStack.dump(std::cerr);
    }
    for(auto v : varsUsed)
    {
	size_t level;
	if (!nameStack.Find(v->Name(), level))
	{
	    v->dump(std::cerr);
	    assert(0 && "Hhhm. Variable has gone missing!");
	}
	if (!(level == 0 || level == maxLevel))
	{
	    if (verbosity)
	    {
		std::cerr << "Adding: " << v->Name() << " level=" << level << std::endl;
	    }
	    nonLocal[v->Name()] = v;
	}
    }

    // Now add those of the subfunctions.
    for(auto fn : subFunctions)
    {
	for(auto v : fn->UsedVars())
	{
	    size_t level;
	    if (!nameStack.Find(v.Name(), level))
	    {
		assert(0 && "Hhhm. Variable has gone missing!");
	    }
	    if (!(level == 0 || level == maxLevel))
	    {
		if (verbosity)
		{
		    std::cout << "Adding" << v.Name() << " level=" << level << std::endl;
		}
		nonLocal[v.Name()] = new VarDef(v);
	    }
	}
    }

    for(auto n : nonLocal)
    {
	VarDef* v = llvm::dyn_cast<VarDef>(n.second);
	if (v)
	{
	    usedVariables.push_back(*v);
	    if (verbosity)
	    {
		v->dump(std::cerr);
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
    return builder.CreateGlobalStringPtr(val, "_string");
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
    assert(sty && "Expect string type in lhsv->Type()");

    llvm::Value* dest = lhsv->Address();

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

    if (llvm::isa<const Types::StringDecl>(lhsv->Type()))
    {
	return AssignStr();
    }

    if (llvm::isa<StringExprAST>(rhs) && lhs->Type()->Type() == Types::Array)
    {
	Types::ArrayDecl* arr = llvm::dyn_cast<Types::ArrayDecl>(lhs->Type());
	if (arr->SubType()->Type() == Types::Char)
	{
	    StringExprAST* str = llvm::dyn_cast<StringExprAST>(rhs);
	    assert(rhs && "Expected string to convert correctly");
	    llvm::Value* dest = lhsv->Address();
	    std::vector<llvm::Value*> ind{MakeIntegerConstant(0), MakeIntegerConstant(0)};
	    llvm::Value* dest1 = builder.CreateGEP(dest, ind, "str_0");
	    llvm::Value* v = rhs->CodeGen();
	    return builder.CreateMemCpy(dest1, v, str->Str().size(),
					std::max(AlignOfType(v->getType()), MIN_ALIGN));
	}
    }

    llvm::Value* dest = lhsv->Address();
    if (!dest)
    {
	return ErrorV(std::string("Unknown variable name ") + lhsv->Name());
    }

    // If rhs is a simple variable, and "large", then use memcpy on it!
    if (VariableExprAST* rhsv = llvm::dyn_cast<VariableExprAST>(rhs))
    {
	if (rhsv->Type() == lhsv->Type())
	{
	    size_t size = rhsv->Type()->Size();
	    if (!disableMemcpyOpt && size >= MEMCPY_THRESHOLD)
	    {
		llvm::Value* src = rhsv->Address();
		return builder.CreateMemCpy(dest, src, size, std::max(rhsv->Type()->AlignSize(), MIN_ALIGN));
	    }
	}
    }

    if (SetExprAST* rhsSet = llvm::dyn_cast<SetExprAST>(rhs))
    {
	llvm::Value* v = rhsSet->CodeGen();
	builder.CreateStore(v, dest);
	return v;
    }

    llvm::Value* v = rhs->CodeGen();

    if (!v)
    {
	return ErrorV("Could not produce expression for assignment");
    }

    llvm::Type* leftType = dest->getType()->getContainedType(0);
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

    if (rty == llvm::Type::IntegerTyID && lty == llvm::Type::IntegerTyID)
    {
	unsigned lsize = leftType->getPrimitiveSizeInBits();
	unsigned rsize = v->getType()->getPrimitiveSizeInBits();
	if (lsize > 1 && rsize > 1 && lsize > rsize)
	{
	    v = builder.CreateSExt(v, leftType);
	}
    }

    assert(rty == lty && "Types must be the same in assignment.");

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

void IfExprAST::accept(Visitor& v)
{
    cond->accept(v);
    if (then)
    {
	then->accept(v);
    }
    if (other)
    {
	other->accept(v);
    }
}

llvm::Value* IfExprAST::CodeGen()
{
    TRACE();
    llvm::Value* condV = cond->CodeGen();
    if (!condV)
    {
	return 0;
    }

    if (condV->getType() !=  Types::GetType(Types::Boolean))
    {
	assert(0 && "Only boolean expressions allowed in if-statement");
    }
    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();
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

void ForExprAST::accept(Visitor& v)
{
    start->accept(v);
    end->accept(v);
    body->accept(v);
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

    startV = TypeConvert(startV, var->getType()->getContainedType(0));
    llvm::Value* stepVal = MakeConstant((stepDown)?-1:1, startV->getType());

    builder.CreateStore(startV, var);

    llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "loop", theFunction);
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "afterloop", theFunction);

    llvm::Value* curVar = builder.CreateLoad(var, varName);
    llvm::Value* endV = end->CodeGen();
    llvm::Value* endCond;

    if (start->Type()->isUnsigned())
    {
	if (stepDown)
	{
	    endCond = builder.CreateICmpUGE(curVar, endV, "loopcond");
	}
	else
	{
	    endCond = builder.CreateICmpULE(curVar, endV, "loopcond");
	}
    }
    else
    {
	if (stepDown)
	{
	    endCond = builder.CreateICmpSGE(curVar, endV, "loopcond");
	}
	else
	{
	    endCond = builder.CreateICmpSLE(curVar, endV, "loopcond");
	}
    }
    builder.CreateSub(endV, stepVal);
    builder.CreateCondBr(endCond, loopBB, afterBB);

    builder.SetInsertPoint(loopBB);
    if (!body->CodeGen())
    {
	return 0;
    }
    curVar = builder.CreateLoad(var, varName);
    if (start->Type()->isUnsigned())
    {
	if (stepDown)
	{
	    endCond = builder.CreateICmpUGT(curVar, endV, "loopcond");
	}
	else
	{
	    endCond = builder.CreateICmpULT(curVar, endV, "loopcond");
	}
    }
    else
    {
	if (stepDown)
	{
	    endCond = builder.CreateICmpSGT(curVar, endV, "loopcond");
	}
	else
	{
	    endCond = builder.CreateICmpSLT(curVar, endV, "loopcond");
	}
    }
    curVar = builder.CreateAdd(curVar, stepVal, "nextvar");
    builder.CreateStore(curVar, var);


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
    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();

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

void WhileExprAST::accept(Visitor& v)
{
    cond->accept(v);
    body->accept(v);
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
    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();

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

void RepeatExprAST::accept(Visitor& v)
{
    cond->accept(v);
    body->accept(v);
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

void WriteAST::accept(Visitor& v)
{
    for(auto a : args)
    {
	a.expr->accept(v);
	if (a.width)
	{
	    a.width->accept(v);
	}
	if (a.precision)
	{
	    a.precision->accept(v);
	}
    }
}

static llvm::Constant* CreateWriteFunc(Types::TypeDecl* ty, llvm::Type* fty)
{
    std::string suffix;
    llvm::Type* resTy = Types::GetType(Types::Void);
    llvm::Type* intTy = Types::GetType(Types::Integer);
    std::vector<llvm::Type*> argTypes{fty};
    if (ty)
    {
	switch(ty->Type())
	{
	case Types::Char:
	    argTypes.push_back(ty->LlvmType());
	    argTypes.push_back(intTy);
	    suffix = "char";
	    break;

	case Types::Boolean:
	    argTypes.push_back(ty->LlvmType());
	    argTypes.push_back(intTy);
	    suffix = "bool";
	    break;

	case Types::Integer:
	    argTypes.push_back(intTy);
	    argTypes.push_back(intTy);
	    suffix = "int";
	    break;

	case Types::Int64:
	    argTypes.push_back(ty->LlvmType());
	    argTypes.push_back(intTy);
	    suffix = "int64";
	    break;

	case Types::Real:
	    // Args: double, int, int
	    argTypes.push_back(ty->LlvmType());
	    argTypes.push_back(intTy);
	    argTypes.push_back(intTy);
	    suffix = "real";
	    break;

	case Types::String:
	{
	    llvm::Type* pty = llvm::PointerType::getUnqual(Types::GetType(Types::Char));
	    argTypes.push_back(pty);
	    argTypes.push_back(intTy);
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
	    argTypes.push_back(intTy);
	    suffix = "chars";
	    break;
	}
	default:
	    ty->dump(std::cerr);
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

static llvm::Constant* CreateWriteBinFunc(llvm::Type* ty, llvm::Type* fty)
{
    llvm::Type* resTy = Types::GetType(Types::Void);
    assert(ty && "Type should not be NULL!");
    if (!ty->isPointerTy())
    {
	return ErrorF("Write argument is not a variable type!");
    }
    llvm::Type* voidPtrTy = Types::GetVoidPtrType();
    std::vector<llvm::Type*> argTypes{fty, voidPtrTy};

    std::string name = std::string("__write_bin");
    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
    llvm::Constant* f = theModule->getOrInsertFunction(name, ft);

    return f;
}

llvm::Value* WriteAST::CodeGen()
{
    TRACE();
    llvm::Type* voidPtrTy = Types::GetVoidPtrType();
    llvm::Value* f = file->Address();

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
		if (AddressableAST* a = llvm::dyn_cast<AddressableAST>(arg.expr))
		{
		    v = a->Address();
		}
		else
		{
		    v = MakeAddressable(arg.expr);
		}
		std::vector<llvm::Value*> ind{MakeIntegerConstant(0), MakeIntegerConstant(0)};
		v = builder.CreateGEP(v, ind, "str_addr");
	    }
	    else if (type->Type() == Types::Array &&
		     type->SubType()->Type() == Types::Char &&
		     !llvm::isa<StringExprAST>(arg.expr))
	    {
		AddressableAST* a = llvm::dyn_cast<AddressableAST>(arg.expr);
		assert(a && "Expected addressable value");
		v = a->Address();
		std::vector<llvm::Value*> ind{MakeIntegerConstant(0), MakeIntegerConstant(0)};
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
	    llvm::Type* ty = v->getType();
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

void ReadAST::accept(Visitor& v)
{
    for(auto a : args)
    {
	a->accept(v);
    }
}

static llvm::Constant* CreateReadFunc(Types::TypeDecl* ty, llvm::Type* fty)
{
    std::string suffix;
    llvm::Type* resTy = Types::GetType(Types::Void);
    std::vector<llvm::Type*> argTypes{fty};
    // ty is NULL if we're doing readln.
    if (!ty)
    {
	suffix = "nl";
    }
    else
    {
	llvm::Type* lty = llvm::PointerType::getUnqual(ty->LlvmType());
	argTypes.push_back(lty);
	switch(ty->Type())
	{
	case Types::Char:
	    suffix = "chr";
	    break;

	case Types::Integer:
	    suffix = "int";
	    break;

	case Types::Real:
	    suffix = "real";
	    break;

	case Types::String:
	    suffix = "str";
	    break;

	default:
	    return ErrorF("Invalid type argument for read");
	}
    }
    std::string name = std::string("__read_") + suffix;
    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
    llvm::Constant* f = theModule->getOrInsertFunction(name, ft);

    return f;
}

static llvm::Constant* CreateReadBinFunc(Types::TypeDecl* ty, llvm::Type* fty)
{
    llvm::Type* resTy = Types::GetType(Types::Void);
    llvm::Type* vTy = llvm::PointerType::getUnqual(ty->LlvmType());
    assert(vTy && "Type should not be NULL!");
    llvm::Type* voidPtrTy = Types::GetVoidPtrType();
    std::vector<llvm::Type*> argTypes{fty, voidPtrTy};

    std::string name = std::string("__read_bin");
    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
    llvm::Constant* f = theModule->getOrInsertFunction(name, ft);

    return f;
}

llvm::Value* ReadAST::CodeGen()
{
    TRACE();
    llvm::Type* voidPtrTy = Types::GetVoidPtrType();
    llvm::Value* f = file->Address();

    bool isText = FileIsText(f);
    llvm::Type* fTy =  f->getType();
    for(auto arg: args)
    {
	std::vector<llvm::Value*> argsV{f};
	VariableExprAST* vexpr = llvm::dyn_cast<VariableExprAST>(arg);
	if (!vexpr)
	{
	    return ErrorV("Argument for read/readln should be a variable");
	}
	
	Types::TypeDecl* ty = vexpr->Type();
	llvm::Value* v = vexpr->Address();
	if (!v)
	{
	    assert(0 && "Could not evaluate address of expression for read");
	    return 0;
	}
	if (!isText)
	{
	    v = builder.CreateBitCast(v, voidPtrTy);
	}
	argsV.push_back(v);
	llvm::Constant* fn;
	if (isText)
	{
	    fn = CreateReadFunc(ty, fTy);
	}
	else
	{
	    fn = CreateReadBinFunc(ty, fTy);
	}
	
	builder.CreateCall(fn, argsV, "");
    }
    if (isReadln)
    {
	if (!isText)
	{
	    return ErrorV("File is not text for readln");
	}
	llvm::Constant* fn = CreateReadFunc(0, fTy);
	builder.CreateCall(fn, f, "");
    }
    return MakeIntegerConstant(0);
}

void VarDeclAST::DoDump(std::ostream& out) const
{
    out << "Var ";
    for(auto v : vars)
    {
	v.dump(out);
	out << std::endl;
    }
}

llvm::Value* VarDeclAST::CodeGen()
{
    TRACE();
    llvm::Value* v = 0;
    for(auto var : vars)
    {
	// Are we declaring global variables  - no function!
	if (!func)
	{
	    llvm::Type* ty = var.Type()->LlvmType();
	    assert(ty && "Type should have a value");
	    llvm::Constant* init = llvm::Constant::getNullValue(ty);
	    llvm::GlobalValue::LinkageTypes linkage = (var.IsExternal()?
						       llvm::GlobalValue::ExternalLinkage:
						       llvm::Function::InternalLinkage);

	    llvm::GlobalVariable* gv = new llvm::GlobalVariable(*theModule, ty, false, linkage,
								init, var.Name());
	    const llvm::DataLayout dl(theModule);
	    size_t al = dl.getPrefTypeAlignment(ty);
	    al = std::max(4ul, al);
	    gv->setAlignment(al);
	    v = gv;
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
    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();
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

void LabelExprAST::accept(Visitor& v)
{
    stmt->accept(v);
    v.visit(this);
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

void CaseExprAST::accept(Visitor& v)
{
    expr->accept(v);
    for(auto l : labels)
    {
	l->accept(v);
    }
    if (otherwise)
    {
	otherwise->accept(v);
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

    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();
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

Types::TypeDecl* RangeExprAST::Type() const
{
    // We have to pick one here. Semantic Analysis will check that both are same type
    return low->Type();
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

llvm::Value* SetExprAST::MakeConstantSet(Types::TypeDecl* type)
{
    static int index=1;
    llvm::Type* ty = type->LlvmType();
    assert(ty && "Expect type for set to work");

    Types::SetDecl::ElemType elems[Types::SetDecl::MaxSetWords] = {};
    for(auto v : values)
    {
	if (RangeExprAST* r = llvm::dyn_cast<RangeExprAST>(v))
	{
	    IntegerExprAST* le = llvm::dyn_cast<IntegerExprAST>(r->LowExpr());
	    IntegerExprAST* he = llvm::dyn_cast<IntegerExprAST>(r->HighExpr());
	    int start = type->GetRange()->GetStart();
	    int low = le->Int() - start;
	    int high = he->Int() - start;
	    assert(low >= 0 && "Range should end up positive ok");
	    for(int i = low; i <= high; i++)
	    {
		elems[i >> Types::SetDecl::SetPow2Bits] |= (1 << (i & Types::SetDecl::SetMask));
	    }
	}
	else
	{
	    IntegerExprAST* e = llvm::dyn_cast<IntegerExprAST>(v);
	    unsigned i = e->Int();
	    elems[i >> Types::SetDecl::SetPow2Bits] |= (1 << (i & Types::SetDecl::SetMask));
	}
    }

    Types::SetDecl* setType = llvm::dyn_cast<Types::SetDecl>(type);
    size_t size = setType->SetWords();
    llvm::Constant** initArr = new llvm::Constant*[size];
    llvm::ArrayType* aty = llvm::dyn_cast<llvm::ArrayType>(ty);
    llvm::Type* eltTy = aty->getElementType();

    for(size_t i = 0; i < size; i++)
    {
	initArr[i] = llvm::ConstantInt::get(eltTy, elems[i]);
    }

    llvm::Constant* init = llvm::ConstantArray::get(aty, llvm::ArrayRef<llvm::Constant*>(initArr, size));
    llvm::GlobalValue::LinkageTypes linkage = llvm::Function::InternalLinkage;
    std::string name(std::string("P") + std::to_string(index) + ".set");
    llvm::GlobalVariable* gv = new llvm::GlobalVariable(*theModule, ty, false, linkage,
							init, name);

    delete [] initArr;

    return gv;
}

llvm::Value* SetExprAST::Address()
{
    TRACE();

    assert(type && "No type supplied");

    // Check if ALL the values involved are constants.
    bool allConstants = true;
    for(auto v : values)
    {
	if (RangeExprAST* r = llvm::dyn_cast<RangeExprAST>(v))
	{
	    if (!IsConstant(r->HighExpr()) || !IsConstant(r->LowExpr()))
	    {
		allConstants = false;
		break;
	    }
	}
	else
	{
	    if (!IsConstant(v))
	    {
		allConstants = false;
		break;
	    }
	}
    }

    if (allConstants)
    {
	return MakeConstantSet(type);
    }

    llvm::Type* ty = type->LlvmType();
    assert(ty && "Expect type for set to work");

    llvm::Value* setV = CreateTempAlloca(ty);
    assert(setV && "Expect CreateTempAlloca() to work");

    llvm::Value* tmp = builder.CreateBitCast(setV, Types::GetVoidPtrType());

    builder.CreateMemSet(tmp, MakeConstant(0, Types::GetType(Types::Char)),
			 (llvm::dyn_cast<Types::SetDecl>(type)->SetWords() *
			  Types::SetDecl::SetBits / 8), 0);

    size_t size = llvm::dyn_cast<Types::SetDecl>(type)->SetWords();

    // TODO: For optimisation, we may want to pass through the vector and see if the values
    // are constants, and if so, be clever about it.
    // Also, we should combine stores to the same word!
    for(auto v : values)
    {
	// If we have a "range", then make a loop.
	if (RangeExprAST* r = llvm::dyn_cast<RangeExprAST>(v))
	{
	    llvm::Value* low = r->Low();
	    llvm::Value* high = r->High();
	    llvm::Function* fn = builder.GetInsertBlock()->getParent();

	    llvm::Value* loopVar = CreateTempAlloca(Types::GetType(Types::Integer));

	    if (!low || !high)
	    {
		assert(0 && "Expected expressions to evalueate");
		return 0;
	    }

	    // Adjust for range:
	    Types::Range* range = type->GetRange();

	    low  = builder.CreateSExt(low, Types::GetType(Types::Integer), "sext.low");
	    high = builder.CreateSExt(high, Types::GetType(Types::Integer), "sext.high");

	    llvm::Value* rangeStart = MakeIntegerConstant(range->GetStart());
	    low = builder.CreateSub(low, rangeStart);
	    high = builder.CreateSub(high, rangeStart);

	    builder.CreateStore(low, loopVar);

	    llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "loop", fn);
	    builder.CreateBr(loopBB);
	    builder.SetInsertPoint(loopBB);

	    llvm::Value* loop = builder.CreateLoad(loopVar, "loopVar");

	    // Set bit "loop" in set.
	    llvm::Value* mask = MakeIntegerConstant(Types::SetDecl::SetMask);
	    llvm::Value* offset = builder.CreateAnd(loop, mask);
	    llvm::Value* bit = builder.CreateShl(MakeIntegerConstant(1), offset);
	    llvm::Value* index = 0;
	    if (size != 1)
	    {
		index = builder.CreateLShr(loop, MakeIntegerConstant(Types::SetDecl::SetPow2Bits));
	    }
	    else
	    {
		index = MakeIntegerConstant(0);
	    }
	    std::vector<llvm::Value*> ind{MakeIntegerConstant(0), index};
	    llvm::Value* bitsetAddr = builder.CreateGEP(setV, ind, "bitsetaddr");
	    llvm::Value* bitset = builder.CreateLoad(bitsetAddr);
	    bitset = builder.CreateOr(bitset, bit);
	    builder.CreateStore(bitset, bitsetAddr);

	    loop = builder.CreateAdd(loop, MakeConstant(1, loop->getType()), "update");
	    builder.CreateStore(loop, loopVar);

	    llvm::Value* endCond = builder.CreateICmpSLE(loop, high, "loopcond");

	    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "afterloop", fn);
	    builder.CreateCondBr(endCond, loopBB, afterBB);

	    builder.SetInsertPoint(afterBB);
	}
	else
	{
	    llvm::Value* x = v->CodeGen();
	    if (!x)
	    {
		return 0;
	    }
	    x = builder.CreateZExt(x, Types::GetType(Types::Integer), "zext");
	    llvm::Value* mask = MakeIntegerConstant(Types::SetDecl::SetMask);
	    llvm::Value* offset = builder.CreateAnd(x, mask);
	    llvm::Value* bit = builder.CreateShl(MakeIntegerConstant(1), offset);
	    llvm::Value* index = 0;
	    if (size != 1)
	    {
		index = builder.CreateLShr(x, MakeIntegerConstant(Types::SetDecl::SetPow2Bits));
	    }
	    else
	    {
		index = MakeIntegerConstant(0);
	    }
	    std::vector<llvm::Value*> ind{MakeIntegerConstant(0), index};
	    llvm::Value* bitsetAddr = builder.CreateGEP(setV, ind, "bitsetaddr");
	    llvm::Value* bitset = builder.CreateLoad(bitsetAddr);
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

void RangeReduceAST::DoDump(std::ostream& out) const
{
    out << "RangeReduce: ";
    expr->dump(out);
    out << "[";
    range->dump(out);
    out << "]";
}

llvm::Value* RangeReduceAST::CodeGen()
{
    TRACE();
    llvm::Value* index = expr->CodeGen();
    assert(index->getType()->isIntegerTy() && "Index is supposed to be integral type");

    llvm::Type* ty = index->getType();
    int start = range->GetStart();
    if (start)
    {
	index = builder.CreateSub(index, MakeConstant(start, ty));
    }
    if (ty->getPrimitiveSizeInBits() < Types::GetType(Types::Integer)->getPrimitiveSizeInBits())
    {
	if (expr->Type()->isUnsigned())
	{
	    index = builder.CreateZExt(index, Types::GetType(Types::Integer), "zext");
	}
	else
	{
	    index = builder.CreateSExt(index, Types::GetType(Types::Integer), "sext");
	}
    }
    return index;
}

void RangeCheckAST::DoDump(std::ostream& out) const
{
    out << "RangeCheck: ";
    expr->dump(out);
    out << "[";
    range->dump(out);
    out << "]";
}

llvm::Value* RangeCheckAST::CodeGen()
{
    TRACE();
    llvm::Value* index = expr->CodeGen();
    assert(index && "Expected expression to generate code");
    assert(index->getType()->isIntegerTy() && "Index is supposed to be integral type");

    llvm::Value* orig_index = index;
    llvm::Type* intTy = Types::GetType(Types::Integer);

    llvm::Type* ty = index->getType();
    int start = range->GetStart();
    if (start)
    {
	index = builder.CreateSub(index, MakeConstant(start, ty));
    }
    if (ty->getPrimitiveSizeInBits() < intTy->getPrimitiveSizeInBits())
    {
	if (expr->Type()->isUnsigned())
	{
	    index = builder.CreateZExt(index, intTy, "zext");
	    orig_index = builder.CreateZExt(orig_index, intTy, "zext");
	}
	else
	{
	    index = builder.CreateSExt(index, intTy, "sext");
	    orig_index = builder.CreateSExt(orig_index, intTy, "sext");
	}
    }
    int end = range->GetRange()->Size();
    llvm::Value* cmp = builder.CreateICmpUGE(index, MakeIntegerConstant(end), "rangecheck");
    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* oorBlock = llvm::BasicBlock::Create(llvm::getGlobalContext(), "out_of_range");
    llvm::BasicBlock* contBlock = llvm::BasicBlock::Create(llvm::getGlobalContext(), "continue", theFunction);
    builder.CreateCondBr(cmp, oorBlock, contBlock);

    theFunction->getBasicBlockList().push_back(oorBlock);
    builder.SetInsertPoint(oorBlock);
    std::vector<llvm::Value*> args = { builder.CreateGlobalStringPtr(Loc().FileName()),
				       MakeIntegerConstant(Loc().LineNumber()),
				       MakeIntegerConstant(start),
				       MakeIntegerConstant(end),
				       orig_index };
    std::vector<llvm::Type*> argTypes = { llvm::PointerType::getUnqual(Types::GetType(Types::Char)),
					  intTy,
					  intTy,
					  intTy,
					  intTy };

    llvm::Type* voidPtrTy = Types::GetVoidPtrType();
    llvm::FunctionType* ft = llvm::FunctionType::get(voidPtrTy, argTypes, false);
    llvm::Constant* fn = theModule->getOrInsertFunction("range_error", ft);

    builder.CreateCall(fn, args, "");
    builder.CreateUnreachable();

    builder.SetInsertPoint(contBlock);
    return index;
}

void TypeCastAST::DoDump(std::ostream& out) const
{
    out << "Convert to ";
    type->dump(out);
    out << "(";
    expr->dump(out);
    out << ")";
}

llvm::Value* TypeCastAST::CodeGen()
{
    if (type->Type() == Types::Real)
    {
	return builder.CreateSIToFP(expr->CodeGen(), type->LlvmType(), "tofp");
    }
    
    Types::TypeDecl* current = expr->Type();
    if (type->isIntegral() && current->isIntegral())
    {
	if (type->isUnsigned())
	{
	    return builder.CreateZExt(expr->CodeGen(), type->LlvmType());
	}
	return builder.CreateSExt(expr->CodeGen(), type->LlvmType());

    }
    if (current->Type() == Types::Array && current->SubType()->Type() == Types::Char && 
	type->Type() == Types::String)
    {
	llvm::Type* ty;
	return MakeStringFromExpr(expr, ty);
    }
    dump();
    assert(0 && "Expected to get something out of this function");
}

llvm::Value* SizeOfExprAST::CodeGen()
{
    return MakeIntegerConstant(type->Size());
}

void SizeOfExprAST::DoDump(std::ostream& out) const
{
    out << "Sizeof(";
    type->dump();
    out << ")" << std::endl;
}


void BuiltinExprAST::DoDump(std::ostream& out) const
{
    out << "Builtin(";
    type->dump();
    out << ")" << std::endl;
}

llvm::Value* BuiltinExprAST::CodeGen()
{
    return bif->CodeGen(builder);
}

void BuiltinExprAST::accept(Visitor& v)
{
    bif->accept(v);
    v.visit(this);
}
