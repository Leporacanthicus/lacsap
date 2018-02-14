#include "expr.h"
#include "stack.h"
#include "builtin.h"
#include "options.h"
#include "trace.h"
#include "types.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/ADT/APSInt.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/Support/raw_os_ostream.h>

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

class DebugInfo
{
public:
    DebugInfo() : cu(0), builder(0) {}
    llvm::DICompileUnit* cu;
    llvm::DIBuilder* builder;
    std::vector<llvm::DIScope*> lexicalBlocks;
    void EmitLocation(Location loc);
    ~DebugInfo();
};

class Label
{
public:
    Label(llvm::BasicBlock* b, int lab): bb(b), label(lab) {}
    void dump(std::ostream& out)
    {
	out << "label: " << label << std::endl;
    }
    llvm::BasicBlock* GetBB() { return bb; }
private:
    llvm::BasicBlock* bb;
    int label;
};

typedef Stack<Label*> LabelStack;
typedef StackWrapper<Label*> LabelWrapper;
typedef Stack<llvm::Value*> VarStack;
typedef StackWrapper<llvm::Value*> VarStackWrapper;

const size_t MEMCPY_THRESHOLD = 16;
const size_t MIN_ALIGN = 4;

extern llvm::Module* theModule;

static VarStack variables;
static LabelStack labels;
llvm::LLVMContext theContext;
static llvm::IRBuilder<> builder(theContext);
static int errCnt;
static std::vector<VTableAST*> vtableBackPatchList;
static std::vector<FunctionAST*> unitInit;

// Debug stack. We just use push_back and pop_back to make it like a stack.
static std::vector<DebugInfo*> debugStack;

static DebugInfo& GetDebugInfo()
{
    assert(!debugStack.empty() && "Debugstack should not be empty!");
    return *debugStack.back();
}

void DebugInfo::EmitLocation(Location loc)
{
    if (loc.LineNumber() == 0)
    {
	::builder.SetCurrentDebugLocation(llvm::DebugLoc());
	return;
    }
    llvm::DIScope* scope;
    if (lexicalBlocks.empty())
    {
	scope = cu;
    }
    else
    {
	scope = lexicalBlocks.back();
    }
    ::builder.SetCurrentDebugLocation(llvm::DebugLoc::get(loc.LineNumber(), loc.Column(), scope));
}

DebugInfo::~DebugInfo()
{
    if (builder)
    {
	Types::Finalize(builder);
	builder->finalize();
    }
    delete builder;
}

static void BasicDebugInfo(ExprAST* e)
{
    if (debugInfo)
    {
	GetDebugInfo().EmitLocation(e->Loc());
    }
}

static llvm::BasicBlock* CreateGotoTarget(int label)
{
    std::string name = std::to_string(label);
    if (Label* lab = labels.Find(name))
    {
	return lab->GetBB();
    }
    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(theContext, name, theFunction);
    labels.Add(name, new Label(bb, label));
    return bb;
}

std::string ShortName(const std::string& name)
{
    std::string shortname = name;
    std::string::size_type pos = name.find_last_of('$');
    if (pos != std::string::npos)
    {
	shortname = shortname.substr(pos+1);
    }
    return shortname;
}

llvm::Constant* GetFunction(llvm::Type* resTy, const std::vector<llvm::Type*>& args,
			    const std::string& name)
{
    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, args, false);
    return theModule->getOrInsertFunction(name, ft);
}

llvm::Constant* GetFunction(Types::TypeDecl* res, const std::vector<llvm::Type*>& args,
			    const std::string& name)
{
    llvm::Type* resTy = res->LlvmType();
    return GetFunction(resTy, args, name);
}

static bool IsConstant(ExprAST* e)
{
    return llvm::isa<IntegerExprAST>(e) || llvm::isa<CharExprAST>(e);
}

size_t AlignOfType(llvm::Type* ty)
{
    const llvm::DataLayout dl(theModule);
    return dl.getPrefTypeAlignment(ty);
}

static llvm::AllocaInst* CreateNamedAlloca(llvm::Function* fn, Types::TypeDecl* ty,
					   const std::string& name)
{
    TRACE();
   /* Save where we were... */
    llvm::BasicBlock* bb = builder.GetInsertBlock();
    llvm::BasicBlock::iterator ip = builder.GetInsertPoint();

    llvm::IRBuilder<> bld(&fn->getEntryBlock(), fn->getEntryBlock().begin());

    assert(ty && "Must have type passed in");
    llvm::Type* type = ty->LlvmType();

    llvm::AllocaInst* a = bld.CreateAlloca(type, 0, name);
    size_t align = std::max(ty->AlignSize(), MIN_ALIGN);
    if (a->getAlignment() < align)
    {
	a->setAlignment(align);
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

    return CreateNamedAlloca(fn, var.Type(), var.Name());
}

static llvm::AllocaInst* CreateTempAlloca(Types::TypeDecl* ty)
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
	assert(v && "Expect addressable object to have address");
	return v;
    }

    llvm::Value* store = e->CodeGen();
    if (store->getType()->isPointerTy())
    {
	return store;
    }

    llvm::Value* v = CreateTempAlloca(e->Type());
    assert(v && "Expect address to be non-zero");
    builder.CreateStore(store, v);
    return v;
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

static llvm::Value* ErrorV(const ExprAST* e, const std::string& msg)
{
    if (e && e->Loc())
    {
	std::cerr << e->Loc() << ": ";
    }
    std::cerr << msg << std::endl;
    errCnt++;
    return 0;
}

static llvm::Function* ErrorF(const ExprAST* e, const std::string& msg)
{
    return reinterpret_cast<llvm::Function*>(ErrorV(e, msg));
}

static llvm::Constant* NoOpValue()
{
    return MakeIntegerConstant(0);
}

llvm::Constant* MakeConstant(uint64_t val, Types::TypeDecl* ty)
{
    return llvm::ConstantInt::get(ty->LlvmType(), val);
}

llvm::Constant* MakeIntegerConstant(int val)
{
    return MakeConstant(val, Types::GetIntegerType());
}

llvm::Constant* MakeBooleanConstant(int val)
{
    return MakeConstant(val, Types::GetBooleanType());
}

static llvm::Constant* MakeCharConstant(int val)
{
    return MakeConstant(val, Types::GetCharType());
}

static llvm::Value* TempStringFromStringExpr(llvm::Value* dest, StringExprAST* rhs)
{
    TRACE();
    std::vector<llvm::Value*> ind = { MakeIntegerConstant(0),MakeIntegerConstant(0) };
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
    assert(rhs->Type()->Type() == Types::TypeDecl::TK_Char &&
	   "Expected char value");
    std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), MakeIntegerConstant(0) };
    llvm::Value* dest1 = builder.CreateGEP(dest, ind, "str_0");

    ind[1] = MakeIntegerConstant(1);
    llvm::Value* dest2 = builder.CreateGEP(dest, ind, "str_1");

    builder.CreateStore(MakeCharConstant(1), dest1);
    return builder.CreateStore(rhs->CodeGen(), dest2);
}

static llvm::Value* LoadOrMemcpy(llvm::Value* src, Types::TypeDecl* ty)
{
    llvm::Value* dest = CreateTempAlloca(ty);
    size_t size = ty->Size();
    if (!disableMemcpyOpt && size >= MEMCPY_THRESHOLD)
    {
	size_t align = ty->AlignSize();
	builder.CreateMemCpy(dest, src, size, std::max(align, MIN_ALIGN));
	return dest;
    }

    llvm::Value* v = builder.CreateLoad(src, "src");
    builder.CreateStore(v, dest);
    return dest;
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

    BasicDebugInfo(this);

    return llvm::ConstantFP::get(theContext, llvm::APFloat(val));
}

void IntegerExprAST::DoDump(std::ostream& out) const
{
    out << "Integer: " << val;
}

llvm::Value* IntegerExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    return MakeConstant(val, type);
}

void NilExprAST::DoDump(std::ostream& out) const
{
    out << "Nil:";
}

llvm::Value* NilExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

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

    BasicDebugInfo(this);

    return MakeCharConstant(val);
}

llvm::Value* AddressableAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    llvm::Value* v = Address();
    assert(v && "Expected to get an address");
    return builder.CreateLoad(v, Name());
}

void VariableExprAST::DoDump(std::ostream& out) const
{
    out << "Variable: " << name << " ";
    Type()->dump(out);
}

llvm::Value* VariableExprAST::Address()
{
    TRACE();
    size_t level;
    if (llvm::Value* v = variables.Find(name, level))
    {
	assert((level == 0 || level == variables.MaxLevel()) &&
	       "Expected variable to either be local or global");
	EnsureSized();
	return v;
    }
    return ErrorV(this, "Unknown variable name '" + name + "'");
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
    assert(v && "Expected variable to have an address");
    EnsureSized();
    llvm::Value* totalIndex = 0;
    for(size_t i = 0; i < indices.size(); i++)
    {
	assert(llvm::isa<RangeReduceAST>(indices[i]));
	llvm::Value* index = indices[i]->CodeGen();
	assert(index && "Expression failed for index");
	if (indexmul[i] != 1)
	{
	    index = builder.CreateMul(index, MakeConstant(indexmul[i], indices[i]->Type()));
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
    std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), totalIndex };
    v = builder.CreateGEP(v, ind, "valueindex");
    return v;
}

void ArrayExprAST::accept(ASTVisitor& v)
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
	std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), MakeIntegerConstant(element) };
	return builder.CreateGEP(v, ind, "valueindex");
    }
    return ErrorV(this, "Expression did not form an address");
}

void FieldExprAST::accept(ASTVisitor& v)
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
    llvm::Value* v = expr->Address();
    std::vector<llvm::Value*> ind{MakeIntegerConstant(0), MakeIntegerConstant(element)};
    v = builder.CreateGEP(v, ind, "valueindex");
    return builder.CreateBitCast(v, llvm::PointerType::getUnqual(Type()->LlvmType()));
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

void PointerExprAST::accept(ASTVisitor& v)
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
    assert(vptr && "Expected variable expression!");
    llvm::Value* v = vptr->Address();
    std::vector<llvm::Value*> ind = { MakeIntegerConstant(0),
				      MakeIntegerConstant(Types::FileDecl::Buffer) };
    v = builder.CreateGEP(v, ind, "bufptr");
    return builder.CreateLoad(v, "buffer");
}

void FilePointerExprAST::accept(ASTVisitor& v)
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
    assert(proto->Function());

    BasicDebugInfo(this);
    return proto->LlvmFunction();
}

static int StringishScore(ExprAST* e)
{
    if (llvm::isa<CharExprAST>(e) || e->Type()->Type() == Types::TypeDecl::TK_Char)
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
    return 0;
}

static bool BothStringish(ExprAST* lhs, ExprAST* rhs)
{
    int lScore = StringishScore(lhs);
    int rScore = StringishScore(rhs);

    return lScore && rScore && (lScore + rScore) >= 2;
}

void BinaryExprAST::DoDump(std::ostream& out) const
{
    out << "BinaryOp: ";
    lhs->dump(out);
    oper.dump(out);
    rhs->dump(out);
}

static llvm::Value* SetOperation(const std::string& name, llvm::Value* res, llvm::Value* src)
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

	llvm::Value* v = CreateTempAlloca(type);
	for(size_t i = 0; i < words; i++)
	{
	    std::vector<llvm::Value*> ind{MakeIntegerConstant(0), MakeIntegerConstant(i)};
	    llvm::Value* lAddr = builder.CreateGEP(lV, ind, "leftSet");
	    llvm::Value* rAddr = builder.CreateGEP(rV, ind, "rightSet");
	    llvm::Value* vAddr = builder.CreateGEP(v, ind, "resSet");
	    llvm::Value* res = builder.CreateLoad(lAddr, "laddr");
	    llvm::Value* tmp = builder.CreateLoad(rAddr, "raddr");
	    res = SetOperation(name, res, tmp);
	    builder.CreateStore(res, vAddr);
	}
	return builder.CreateLoad(v, "set");
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

    Types::SetDecl* type = llvm::dyn_cast<Types::SetDecl>(rhs->Type());
    assert(*type == *lhs->Type() && "Expect both sides to have same type" );
    assert(type && "Expect to get a type");

    llvm::Value* rV = MakeAddressable(rhs);
    llvm::Value* lV = MakeAddressable(lhs);
    assert(rV && lV && "Should have generated values for left and right set");

    llvm::Type* pty = llvm::PointerType::getUnqual(type->LlvmType());
    llvm::Value* setWords = MakeIntegerConstant(type->SetWords());
    llvm::Type* intTy = setWords->getType();
    if (resTyIsSet)
    {
	llvm::Constant* f = GetFunction(Types::GetVoidType(), { pty, pty, pty, intTy },
					"__Set" + name);
	
	llvm::Value* v = CreateTempAlloca(type);
	std::vector<llvm::Value*> args = { v, lV, rV, setWords };
	builder.CreateCall(f, args);
	return builder.CreateLoad(v, "set");
    }

    llvm::Constant* f = GetFunction(Types::GetBooleanType(), { pty, pty, intTy }, "__Set" + name);
    return builder.CreateCall(f, { lV, rV, setWords }, "calltmp");
}

llvm::Value* MakeStringFromExpr(ExprAST* e, Types::TypeDecl* ty)
{
    TRACE();
    if (e->Type()->Type() == Types::TypeDecl::TK_Char)
    {
	llvm::Value* v = CreateTempAlloca(Types::GetStringType());
	TempStringFromChar(v, e);
	return v;
    }
    if (StringExprAST* se = llvm::dyn_cast<StringExprAST>(e))
    {
	llvm::Value* v = CreateTempAlloca(ty);
	TempStringFromStringExpr(v, se);
	return v;
    }

    return MakeAddressable(e);
}

static llvm::Value* CallStrFunc(const std::string& name, ExprAST* lhs, ExprAST* rhs, Types::TypeDecl* resTy,
				const std::string& twine)
{
    TRACE();
    llvm::Value* rV = MakeStringFromExpr(rhs, rhs->Type());
    llvm::Value* lV = MakeStringFromExpr(lhs, lhs->Type());

    llvm::Constant* f = GetFunction(resTy, { lV->getType(), rV->getType() }, "__Str" + name);
    return builder.CreateCall(f, { lV, rV }, twine);
}

static llvm::Value* CallStrCat(ExprAST* lhs, ExprAST* rhs)
{
    TRACE();
    llvm::Value* rV = MakeStringFromExpr(rhs, rhs->Type());
    llvm::Value* lV = MakeStringFromExpr(lhs, lhs->Type());

    llvm::Type* strTy = Types::GetStringType()->LlvmType();
    llvm::Type* pty = llvm::PointerType::getUnqual(strTy);

    llvm::Constant* f = GetFunction(Types::GetVoidType(), {pty, lV->getType(), rV->getType()},
				    "__StrConcat");

    llvm::Value* dest = CreateTempAlloca(Types::GetStringType());
    builder.CreateCall(f, {dest, lV, rV});
    return dest;
}

llvm::Value* BinaryExprAST::CallStrFunc(const std::string& name)
{
    TRACE();
    return ::CallStrFunc(name, lhs, rhs, Types::GetIntegerType(), "calltmp");
}

llvm::Value* BinaryExprAST::CallArrFunc(const std::string& name, size_t size)
{
    TRACE();

    llvm::Value* rV = MakeAddressable(rhs);
    llvm::Value* lV = MakeAddressable(lhs);

    assert(rV && lV && "Expect to get values here...");

    // Result is integer.
    llvm::Type* resTy = Types::GetIntegerType()->LlvmType();
    llvm::Type* pty = llvm::PointerType::getUnqual(Types::GetCharType()->LlvmType());

    lV = builder.CreateBitCast(lV, pty);
    rV = builder.CreateBitCast(rV, pty);

    llvm::Constant* f = GetFunction(resTy, { pty, pty, resTy }, "__Arr" + name);

    std::vector<llvm::Value*> args = { lV, rV,  MakeIntegerConstant(size) };
    return builder.CreateCall(f, args, "calltmp");
}

void BinaryExprAST::UpdateType(Types::TypeDecl* ty)
{
    if (type != ty)
    {
	assert(!type && "Type shouldn't be updated more than once");
	assert(ty && "Must supply valid type");

	type = ty;
    }
}

Types::TypeDecl* BinaryExprAST::Type() const
{
    if (oper.IsCompare())
    {
	return Types::GetBooleanType();
    }

    if (type)
    {
	return type;
    }

    assert(lhs->Type() && "Should have types here...");

    // Last resort, return left type.
    return lhs->Type();
}

llvm::Value* BinaryExprAST::SetCodeGen()
{
    TRACE();
    if (lhs->Type() && lhs->Type()->IsIntegral() && oper.GetToken() == Token::In)
    {
	llvm::Value* l = lhs->CodeGen();
	llvm::Value* setV = MakeAddressable(rhs);
	Types::TypeDecl* type = rhs->Type();
	int start = type->GetRange()->Start();
	l = builder.CreateZExt(l, Types::GetIntegerType()->LlvmType(), "zext.l");
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

	llvm::Value* bitset = builder.CreateLoad(bitsetAddr, "bitsetaddr");
	llvm::Value* bit = builder.CreateLShr(bitset, offset);
	return builder.CreateTrunc(bit, Types::GetBooleanType()->LlvmType());
    }

    if (llvm::isa<SetExprAST>(lhs) || (lhs->Type() && llvm::isa<Types::SetDecl>(lhs->Type())))
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

	case Token::GreaterThan:
	    return builder.CreateNot(CallSetFunc("Contains", false));

	case Token::LessThan:
	{
	    // Swap left<->right sides
	    ExprAST* tmp = rhs;
	    rhs = lhs;
	    lhs = tmp;
	    llvm::Value* v = builder.CreateNot(CallSetFunc("Contains", false));
	    // Set it back
	    lhs = rhs;
	    rhs = tmp;
	    return v;
	}

	default:
	    return ErrorV(this, "Unknown operator on set");
	}
    }
    return ErrorV(this, "Invalid arguments in set operation");
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
	return ErrorV(0, "Invalid operand for char arrays");
    }
}

llvm::Value* BinaryExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

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

    assert(lhs->Type() && rhs->Type() && "Huh? Both sides of expression should have type");

    if (BothStringish(lhs, rhs))
    {
	if (oper.GetToken() == Token::Plus)
	{
	    return CallStrCat(lhs, rhs);
	}

	/* We don't need to do this of both sides are char - then it's just a simple comparison */
	if (lhs->Type()->Type() != Types::TypeDecl::TK_Char ||
	    rhs->Type()->Type() != Types::TypeDecl::TK_Char)
	{
	    return MakeStrCompare(oper, CallStrFunc("Compare"));
	}
    }

    if (llvm::isa<Types::ArrayDecl>(rhs->Type()) && llvm::isa<Types::ArrayDecl>(lhs->Type()))
    {
	// Comparison operators are allowed for old style Pascal strings (char arrays)
	// But only if they are the same size.
	if (lhs->Type()->SubType()->Type() == Types::TypeDecl::TK_Char &&
	    rhs->Type()->SubType()->Type() == Types::TypeDecl::TK_Char)
	{
	    Types::ArrayDecl* ar = llvm::dyn_cast<Types::ArrayDecl>(rhs->Type());
	    Types::ArrayDecl* al = llvm::dyn_cast<Types::ArrayDecl>(lhs->Type());

	    std::vector<Types::RangeDecl*> rr = ar->Ranges();
	    std::vector<Types::RangeDecl*> rl = al->Ranges();

	    if (rr.size() == 1 && rl.size() == 1 && rr[0]->Size() == rl[0]->Size())
	    {
		return MakeStrCompare(oper, CallArrFunc("Compare", rr[0]->Size()));
	    }
	}
    }

    llvm::Value* l = lhs->CodeGen();
    llvm::Value* r = rhs->CodeGen();

    assert(l && r && "Should have a value for both sides");

    llvm::Type* lty = l->getType();
    llvm::Type* rty = r->getType();
    (void) lty;
    assert(rty == lty && "Expect same types");

    // Can compare for (in)equality with pointers and integers
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
	bool IsUnsigned = rhs->Type()->IsUnsigned();
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
	    if (IsUnsigned)
	    {
		return builder.CreateICmpULT(l, r, "lt");
	    }
	    return builder.CreateICmpSLT(l, r, "lt");
	case Token::LessOrEqual:
	    if (IsUnsigned)
	    {
		return builder.CreateICmpULE(l, r, "le");
	    }
	    return builder.CreateICmpSLE(l, r, "le");
	case Token::GreaterThan:
	    if (IsUnsigned)
	    {
		return builder.CreateICmpUGT(l, r, "gt");
	    }
	    return builder.CreateICmpSGT(l, r, "gt");
	case Token::GreaterOrEqual:
	    if (IsUnsigned)
	    {
		return builder.CreateICmpUGE(l, r, "ge");
	    }
	    return builder.CreateICmpSGE(l, r, "ge");

	default:
	    return ErrorV(this, "Unknown token: " + oper.ToString());
	}
    }
    if (rty->isDoubleTy())
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
	    return ErrorV(this, "Unknown token: " + oper.ToString());
	}
    }

    l->dump();
    oper.dump(std::cout);
    r->dump();
    assert(0 && "Should not get here!");
    return 0;
}

void UnaryExprAST::DoDump(std::ostream& out) const
{
    out << "Unary: " << oper.ToString();
    rhs->dump(out);
}

llvm::Value* UnaryExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

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
	    break;
	}
    }
    if (rty == llvm::Type::DoubleTyID)
    {
	if(oper.GetToken() == Token::Minus)
	{
	    return builder.CreateFNeg(r, "minus");
	}
    }
    return ErrorV(this, "Unknown operation: " + oper.ToString());
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
    assert(proto && "Function prototype should be set");

    BasicDebugInfo(this);

    llvm::Value* calleF = callee->CodeGen();
    if (!calleF)
    {
	return ErrorV(this, "Unknown function '" + proto->Name() + "' referenced");
    }	

    const std::vector<VarDef>& vdef = proto->Args();
    assert(vdef.size() == args.size() && "Incorrect number of arguments for function");

    std::vector<llvm::Value*> argsV;
    std::vector<std::pair<int, llvm::Attribute::AttrKind>> argAttr;
    unsigned index = 0;
    for(auto i : args)
    {
	llvm::Value* v = 0;

	if (ClosureAST* ca = llvm::dyn_cast<ClosureAST>(i))
	{
	    v = ca->CodeGen();
	    argAttr.push_back(std::make_pair(index+1, llvm::Attribute::Nest));
	}
	else
	{
	    VariableExprAST* vi = llvm::dyn_cast<VariableExprAST>(i);
	    if (vdef[index].IsRef())
	    {
		if (vi)
		{
		    v = vi->Address();
		}
		else
		{
		    TypeCastAST* tc = llvm::dyn_cast<TypeCastAST>(i);
		    assert(tc && "Uhm - this should be a typecast expression!");

		    if (!llvm::isa<VariableExprAST>(tc->Expr()))
		    {
			return ErrorV(this, "Argument declared with 'var' must be a variable!");
		    }

		    if (!(v = tc->Address()))
		    {
			return 0;
		    }
		}
	    }
	    else
	    {
		if (llvm::isa<Types::FuncPtrDecl>(vdef[index].Type()))
		{
		    v = i->CodeGen();
		}
		if (!v)
		{
		    if (i->Type()->IsCompound())
		    {
			if (vi)
			{
			    v = LoadOrMemcpy(vi->Address(), vi->Type());
			}
			else
			{
			    v = CreateTempAlloca(i->Type());
			    builder.CreateStore(i->CodeGen(), v);
			}
			argAttr.push_back(std::make_pair(index+1, llvm::Attribute::ByVal));
		    }
		    else
		    {
			if (!(v = i->CodeGen()))
			{
			    return 0;
			}
		    }
		}
	    }
	}
	assert(v && "Expect argument here");
	argsV.push_back(v);
	index++;
    }
    const char* res = "";
    if (proto->Type()->Type() != Types::TypeDecl::TK_Void)
    {
	res = "calltmp";
    }
    llvm::CallInst* inst = builder.CreateCall(calleF, argsV, res);
    for(auto v : argAttr)
    {
	inst->addAttribute(v.first, v.second);
    }
    return inst;
}

void CallExprAST::accept(ASTVisitor& v)
{
    callee->accept(v);
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

void BlockAST::accept(ASTVisitor& v)
{
    for(auto i : content)
    {
	i->accept(v);
    }
}

llvm::Value* BlockAST::CodeGen()
{
    TRACE();

    for(auto e : content)
    {
	llvm::Value* v = e->CodeGen();
	(void)v;
	assert(v && "Expect codegen to work!");
    }
    return NoOpValue();
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

llvm::Function* PrototypeAST::Create(const std::string& namePrefix)
{
    TRACE();
    assert(namePrefix != "" && "Prefix should never be empty");
    if(llvmFunc)
    {
	return llvmFunc;
    }

    std::vector<std::pair<int, llvm::Attribute::AttrKind>> argAttr;
    std::vector<llvm::Type*> argTypes;
    unsigned index = 0;
    for(auto i : args)
    {
	llvm::AttrBuilder attrs;
	assert(i.Type() && "Invalid type for argument");
	llvm::Type* argTy = i.Type()->LlvmType();
	
	index++;
	if (index == 1 && Function()->ClosureType())
	{
	    argAttr.push_back(std::make_pair(index, llvm::Attribute::Nest));
	}
	if (i.IsRef() || i.Type()->IsCompound() )
	{
	    argTy = llvm::PointerType::getUnqual(argTy);
	    if (!i.IsRef())
	    {
		argAttr.push_back(std::make_pair(index, llvm::Attribute::ByVal));
	    }
	}

	argTypes.push_back(argTy);
    }
    llvm::Type* resTy = type->LlvmType();
    std::string actualName;
    /* Don't mangle our 'main' functions name, as we call that from C */
    if (name == "__PascalMain")
    {
	actualName = name;
    }
    else
    {
	actualName = namePrefix + ".";
	if (baseobj)
	{
	    actualName += baseobj->Name() + "$";
	}
	actualName += name;
    }

    llvmFunc = llvm::dyn_cast<llvm::Function>(GetFunction(resTy, argTypes, actualName));
    assert(llvmFunc && "Should have found a function here!");
    if (!llvmFunc->empty())
    {
	return ErrorF(this, "redefinition of function: " + name);
    }

    assert(llvmFunc->arg_size() == args.size() && "Expect number of arguments to match");

    auto a = args.begin();
    for(auto& arg : llvmFunc->args())
    {
	arg.setName(a->Name());
	a++;
    }

    for(auto v : argAttr)
    {
	llvmFunc->addAttribute(v.first, v.second);
    }
    // TODO: Allow this to be disabled.
    llvmFunc->addFnAttr("no-frame-pointer-elim", "true");

    return llvmFunc;
}

void PrototypeAST::CreateArgumentAlloca()
{
    TRACE();

    unsigned offset = 0;
    llvm::Function::arg_iterator ai = llvmFunc->arg_begin();
    if (Types::TypeDecl* closureType = Function()->ClosureType())
    {
	assert(closureType == args[0].Type() && "Expect type to match here");
	// Skip over the closure argument in the loop below.
	offset = 1;

	Types::RecordDecl* rd = llvm::dyn_cast<Types::RecordDecl>(closureType);
	assert(rd && "Expected a record for closure type!");
	std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), 0 };
	for(int i = 0; i < rd->FieldCount(); i++)
	{
	    const Types::FieldDecl* f = rd->GetElement(i);
	    ind[1] = MakeIntegerConstant(i);
	    llvm::Value* a = builder.CreateGEP(&*ai, ind, f->Name());
	    a = builder.CreateLoad(a, f->Name());
	    if (!variables.Add(f->Name(), a))
	    {
		ErrorF(this, "Duplicate variable name " + f->Name());
	    }
	}
	// Now "done" with this argument, so skip to next.
	ai++;
    }
    for(unsigned idx = offset; idx < args.size(); idx++, ai++)
    {
	llvm::Value* a;
	if (args[idx].IsRef() || args[idx].Type()->IsCompound())
	{
	    a = &*ai;
	}
	else
	{
	    a = CreateAlloca(llvmFunc, args[idx]);
	    builder.CreateStore(&*ai, a);
	}
	if (!variables.Add(args[idx].Name(), a))
	{
	    ErrorF(this, "Duplicate variable name " + args[idx].Name());
	}

	if (debugInfo)
	{
	    DebugInfo& di = GetDebugInfo();
	    assert(!di.lexicalBlocks.empty() && "Should not have empty lexicalblocks here!");
	    llvm::DIScope* sp = di.lexicalBlocks.back();
	    llvm::DIType* debugType = args[idx].Type()->DebugType(di.builder);
	    if (!debugType)
	    {
		// Ugly hack until we have all debug types.
		std::cerr << "Skipping unknown debug type element." << std::endl;
		args[idx].Type()->dump();
		goto skip;
	    }
	    {
	    // Create a debug descriptor for the variable.
	    int lineNum = function->Loc().LineNumber();
	    llvm::DIFile* unit = sp->getFile();
	    llvm::DILocalVariable* dv =
		di.builder->createParameterVariable(sp, args[idx].Name(), idx+1, unit, lineNum,
						    debugType, true);
	    di.builder->insertDeclare(a, dv, di.builder->createExpression(),
				      llvm::DebugLoc::get(lineNum, 0, sp),
				      ::builder.GetInsertBlock());
	    }
	skip: ;
	}

    }
    if (type->Type() != Types::TypeDecl::TK_Void)
    {
	std::string shortname = ShortName(name);
	llvm::AllocaInst* a = CreateAlloca(llvmFunc, VarDef(shortname, type));
	if (!variables.Add(shortname, a))
	{
	    ErrorF(this, "Duplicate function result name '" + shortname + "'.");
	}
    }
}

void PrototypeAST::SetIsForward(bool v)
{
    assert(!function && "Can't make a real function prototype forward");
    isForward = v;
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

bool PrototypeAST::operator==(const PrototypeAST& rhs) const
{
    // Easy case: They are the same pointer!
    if (&rhs == this)
    {
	return true;
    }
    // Can't be same if baseobj is different? Is this so?
    // Or if args count is different
    if (baseobj != rhs.baseobj || args.size() != rhs.args.size())
    {
	return false;
    }
    // Not equal if args are different types
    for(size_t i = 0; i < args.size(); i++)
    {
	if (*args[i].Type() != *rhs.args[i].Type())
	{
	    return false;
	}
    }
    // All done, must be equal.
    return true;
}

/* Check "if we remove the closure, is the arguments matching".
 * This function will return false if this->args[0] isn't a closure.
 * It will return true if all arguments (and return typ) is the saem type.
 */
bool PrototypeAST::IsMatchWithoutClosure(const PrototypeAST* rhs) const
{
    /* Don't allow comparison with ourselves */
    if (rhs == this)
    {
	return false;
    }
    if (Types::TypeDecl* closure = Function()->ClosureType())
    {
	if (args[0].Type() != closure)
	{
	    return false;
	}
    }
    // Can't be same if baseobj is different? Is this so?
    // Or if args count is different
    if (baseobj != rhs->baseobj || args.size() != rhs->args.size() + 1)
    {
	return false;
    }
    for(size_t i = 0; i < rhs->args.size(); i++)
    {
	if (*args[i+1].Type() != *rhs->args[i].Type())
	{
	    return false;
	}
    }
    return true;
}

FunctionAST::FunctionAST(const Location& w, PrototypeAST* prot, const std::vector<VarDeclAST*>& v,
			 BlockAST* b)
    : ExprAST(w, EK_Function), proto(prot), varDecls(v), body(b), parent(0), closureType(0)
{
    assert((proto->IsForward() || body) && "Function should have body");
    if (!proto->IsForward())
    {
	proto->SetFunction(this);
    }
    for(auto vv : varDecls)
    {
	vv->SetFunction(this);
    }
}

void FunctionAST::DoDump(std::ostream& out) const
{
    out << "Function: " << std::endl;
    proto->dump(out);
    out << "Function body:" << std::endl;
    body->dump(out);
}

void FunctionAST::accept(ASTVisitor& v)
{
    v.visit(this);
    for(auto d : varDecls)
    {
	d->accept(v);
    }
    if (body)
    {
	body->accept(v);
    }
    for(auto i : subFunctions)
    {
	i->accept(v);
    }
}

static llvm::DISubroutineType* CreateFunctionType(DebugInfo& di, PrototypeAST* proto)
{
    std::vector<llvm::Metadata*> eltTys;

    eltTys.push_back(proto->Type()->DebugType(di.builder));

    for(auto a : proto->Args())
    {
	eltTys.push_back(a.Type()->DebugType(di.builder));
    }
    return di.builder->createSubroutineType(di.builder->getOrCreateTypeArray(eltTys));
}

llvm::Function* FunctionAST::CodeGen(const std::string& namePrefix)
{
    TRACE();
    VarStackWrapper w(variables);
    LabelWrapper l(labels);
    assert(namePrefix != "" && "Prefix should not be empty");
    llvm::Function* theFunction = proto->Create(namePrefix);

    if (!theFunction)
    {
	return 0;
    }
    if (!body)
    {
	return theFunction;
    }

    if (debugInfo)
    {
	DebugInfo& di = GetDebugInfo();
	Location loc = body->Loc();
	llvm::DIFile* unit = di.builder->createFile(di.cu->getFilename(), di.cu->getDirectory());
	llvm::DIScope* fnContext = unit;
	llvm::DISubroutineType* st = CreateFunctionType(di, proto);
	std::string name = proto->Name();
	int lineNum = loc.LineNumber();
	llvm::DISubprogram* sp = di.builder->createFunction(fnContext, name, "", unit,
							    lineNum, st, false, true,
							    lineNum,
							    llvm::DINode::FlagPrototyped, false);

	theFunction->setSubprogram(sp);
	di.lexicalBlocks.push_back(sp);
	di.EmitLocation(Location());
    }
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(theContext, "entry", theFunction);
    builder.SetInsertPoint(bb);

    proto->CreateArgumentAlloca();
    for(auto d : varDecls)
    {
	d->CodeGen();
    }

    llvm::BasicBlock::iterator ip = builder.GetInsertPoint();

    std::string newPrefix = namePrefix + "." + proto->Name();
    for(auto fn : subFunctions)
    {
	fn->CodeGen(newPrefix);
    }

    if (verbosity > 1)
    {
	variables.dump(std::cerr);
    }

    if (debugInfo)
    {
	DebugInfo& di = GetDebugInfo();
	di.EmitLocation(body->Loc());
    }
    builder.SetInsertPoint(bb, ip);
    llvm::Value* block = body->CodeGen();
    if (!block && !body->IsEmpty())
    {
	return 0;
    }

    // Mark end of function!
    if (debugInfo)
    {
	DebugInfo& di = GetDebugInfo();
	di.EmitLocation(endLoc);
    }
    if (proto->Type()->Type() == Types::TypeDecl::TK_Void)
    {
	builder.CreateRetVoid();
    }
    else
    {
	std::string shortname = ShortName(proto->Name());
	llvm::Value* v = variables.Find(shortname);
	assert(v && "Expect function result 'variable' to exist");
	llvm::Value* retVal = builder.CreateLoad(v, shortname);
	builder.CreateRet(retVal);
    }

    if (debugInfo)
    {
	DebugInfo& di = GetDebugInfo();
	di.lexicalBlocks.pop_back();
    }

    if (!debugInfo && body && emitType != LlvmIr)
    {
	llvm::raw_os_ostream err(std::cerr);
	assert(!verifyFunction(*theFunction, &err) && "Something went wrong in code generation");
    }

    return theFunction;
}

llvm::Function* FunctionAST::CodeGen()
{
    return CodeGen("P");
}

Types::TypeDecl* FunctionAST::ClosureType()
{
    if (usedVariables.empty())
    {
	return 0;
    }
    // Have we cached it? Return now!
    if (!closureType)
    {
	std::vector<Types::FieldDecl*> vf;
	for(auto u : usedVariables)
	{
	    Types::TypeDecl* ty = new Types::PointerDecl(u.Type());
	    vf.push_back(new Types::FieldDecl(u.Name(), ty, false));
	}
	closureType = new Types::RecordDecl(vf, 0);
    }
    return closureType;
}

void StringExprAST::DoDump(std::ostream& out) const
{
    out << "String: '" << val << "'" << std::endl;
}

llvm::Value* StringExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    return builder.CreateGlobalStringPtr(val, "_string");
}

llvm::Value* StringExprAST::Address()
{
    TRACE();

    BasicDebugInfo(this);

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
    assert(llvm::isa<Types::StringDecl>(lhsv->Type()) && "Expect string type in lhsv->Type()");

    if (StringExprAST* srhs = llvm::dyn_cast<StringExprAST>(rhs))
    {
	llvm::Value* dest = lhsv->Address();
	return TempStringFromStringExpr(dest, srhs);
    }

    assert(llvm::isa<Types::StringDecl>(rhs->Type()));
    return CallStrFunc("Assign", lhs, rhs, Types::GetVoidType(), "");
}

llvm::Value* AssignExprAST::AssignSet()
{
    if (llvm::Value* v = rhs->CodeGen())
    {
	VariableExprAST* lhsv = llvm::dyn_cast<VariableExprAST>(lhs);
	llvm::Value* dest = lhsv->Address();
	if (*lhs->Type() == *rhs->Type())
	{
	    assert(dest && "Expected address from lhsv!");
	    builder.CreateStore(v, dest);
	    return v;
	}
    }
    return 0;
}

llvm::Value* AssignExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    VariableExprAST* lhsv = llvm::dyn_cast<VariableExprAST>(lhs);
    if (!lhsv)
    {
	return ErrorV(this, "Left hand side of assignment must be a variable");
    }

    if (llvm::isa<const Types::StringDecl>(lhsv->Type()))
    {
	return AssignStr();
    }

    if (llvm::isa<Types::SetDecl>(lhsv->Type()))
    {
	return AssignSet();
    }

    if (llvm::isa<StringExprAST>(rhs) && lhs->Type()->Type() == Types::TypeDecl::TK_Array)
    {
	Types::ArrayDecl* arr = llvm::dyn_cast<Types::ArrayDecl>(lhs->Type());
	if (arr->SubType()->Type() == Types::TypeDecl::TK_Char)
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
	return ErrorV(this, "Unknown variable name '" + lhsv->Name() + "'");
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

    if (llvm::Value* v = rhs->CodeGen())
    {
	builder.CreateStore(v, dest);
	return v;
    }
    return ErrorV(this, "Could not produce expression for assignment");
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

void IfExprAST::accept(ASTVisitor& v)
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

    BasicDebugInfo(this);

    assert(cond->Type() && "Expect type here");
    assert(*cond->Type() ==  *Types::GetBooleanType() && 
	   "Only boolean expressions allowed in if-statement");

    llvm::Value* condV = cond->CodeGen();
    if (!condV)
    {
	return 0;
    }

    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(theContext, "then", theFunction);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(theContext, "ifcont");

    llvm::BasicBlock* elseBB = mergeBB;
    if (other)
    {
	elseBB = llvm::BasicBlock::Create(theContext, "else");
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

void ForExprAST::accept(ASTVisitor& v)
{
    start->accept(v);
    end->accept(v);
    body->accept(v);
    v.visit(this);
}

llvm::Value* ForExprAST::CodeGen()
{
    TRACE();
    BasicDebugInfo(this);

    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();
    llvm::Value* var = variable->Address();
    assert(var && "Expected variable here");

    llvm::Value* startV = start->CodeGen();
    assert(startV && "Expected start to generate code");

    llvm::Value* stepVal = MakeConstant((stepDown)?-1:1, start->Type());
    llvm::Value* endV = end->CodeGen();
    assert(endV && "Expected end to generate code");

    builder.CreateStore(startV, var);

    llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(theContext, "loop", theFunction);
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(theContext, "afterloop", theFunction);

    llvm::Value* curVar = builder.CreateLoad(var, variable->Name());
    llvm::Value* endCond;

    if (start->Type()->IsUnsigned())
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
    curVar = builder.CreateLoad(var, variable->Name());
    if (start->Type()->IsUnsigned())
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

    BasicDebugInfo(this);
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
    llvm::BasicBlock* preBodyBB = llvm::BasicBlock::Create(theContext, "prebody",
							   theFunction);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(theContext, "body",
							 theFunction);
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(theContext, "after",
							 theFunction);

    BasicDebugInfo(this);

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
    BasicDebugInfo(this);
    builder.CreateBr(preBodyBB);

    builder.SetInsertPoint(afterBB);

    return afterBB;
}

void WhileExprAST::accept(ASTVisitor& v)
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
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(theContext, "body",
							 theFunction);
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(theContext, "after",
							 theFunction);

    BasicDebugInfo(this);

    builder.CreateBr(bodyBB);
    builder.SetInsertPoint(bodyBB);
    if (!body->CodeGen())
    {
	return 0;
    }
    llvm::Value* condv = cond->CodeGen();
    llvm::Value* endCond = builder.CreateICmpNE(condv, MakeBooleanConstant(0), "untilcond");
    BasicDebugInfo(this);
    builder.CreateCondBr(endCond, afterBB, bodyBB);

    builder.SetInsertPoint(afterBB);

    return afterBB;
}

void RepeatExprAST::accept(ASTVisitor& v)
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

void WriteAST::accept(ASTVisitor& v)
{
    file->accept(v);
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
    llvm::Type* intTy = Types::GetIntegerType()->LlvmType();
    std::vector<llvm::Type*> argTypes{fty};
    switch(ty->Type())
    {
    case Types::TypeDecl::TK_Char:
	argTypes.push_back(ty->LlvmType());
	argTypes.push_back(intTy);
	suffix = "char";
	break;

    case Types::TypeDecl::TK_Boolean:
	argTypes.push_back(ty->LlvmType());
	argTypes.push_back(intTy);
	suffix = "bool";
	break;

    case Types::TypeDecl::TK_Integer:
	argTypes.push_back(intTy);
	argTypes.push_back(intTy);
	suffix = "int";
	break;

    case Types::TypeDecl::TK_LongInt:
	argTypes.push_back(ty->LlvmType());
	argTypes.push_back(intTy);
	suffix = "int64";
	break;

    case Types::TypeDecl::TK_Real:
	// Args: double, int, int
	argTypes.push_back(ty->LlvmType());
	argTypes.push_back(intTy);
	argTypes.push_back(intTy);
	suffix = "real";
	break;

    case Types::TypeDecl::TK_String:
    {
	llvm::Type* pty = llvm::PointerType::getUnqual(Types::GetCharType()->LlvmType());
	argTypes.push_back(pty);
	argTypes.push_back(intTy);
	suffix = "str";
	break;
    }
    case Types::TypeDecl::TK_Array:
    {
	Types::ArrayDecl* ad = llvm::dyn_cast<Types::ArrayDecl>(ty);
	assert(ad && "Expect array declaration to expand!");
	assert(ad->SubType()->Type() == Types::TypeDecl::TK_Char && "Expected char type");
	llvm::Type* pty = llvm::PointerType::getUnqual(Types::GetCharType()->LlvmType());
	argTypes.push_back(pty);
	argTypes.push_back(intTy);
	argTypes.push_back(intTy);
	suffix = "chars";
	break;
    }
    default:
	ty->dump(std::cerr);
	assert(0);
	return ErrorF(0, "Invalid type argument for write");
    }
    return GetFunction(Types::GetVoidType(), argTypes, "__write_" + suffix);
}

static llvm::Constant* CreateWriteBinFunc(llvm::Type* ty, llvm::Type* fty)
{
    assert(ty && "Type should not be NULL!");
    assert(ty->isPointerTy() && "Expected pointer argument");
    llvm::Type* voidPtrTy = Types::GetVoidPtrType();
    llvm::Constant* f = GetFunction(Types::GetVoidType(), { fty, voidPtrTy }, "__write_bin");

    return f;
}

llvm::Value* WriteAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    llvm::Value* f = file->Address();
    llvm::Value* v = 0;
    bool isText = llvm::isa<Types::TextDecl>(file->Type());
    if (isText && args.empty() && !isWriteln)
    {
	return NoOpValue();
    }
    for(auto arg: args)
    {
	std::vector<llvm::Value*> argsV;
	llvm::Constant* fn;
	argsV.push_back(f);
	if (isText)
	{
	    Types::TypeDecl* type = arg.expr->Type();
	    assert(type && "Expected type here");
	    if (!(fn = CreateWriteFunc(type, f->getType())))
	    {
		return 0;
	    }
	    if (type->Type() == Types::TypeDecl::TK_String)
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
	    else if (type->Type() == Types::TypeDecl::TK_Array &&
		     type->SubType()->Type() == Types::TypeDecl::TK_Char)
	    {
		if (llvm::isa<StringExprAST>(arg.expr))
		{
		    v = arg.expr->CodeGen();
		}
		else
		{
		    AddressableAST* a = llvm::dyn_cast<AddressableAST>(arg.expr);
		    assert(a && "Expected addressable value");
		    v = a->Address();
		    std::vector<llvm::Value*> ind{MakeIntegerConstant(0), MakeIntegerConstant(0)};
		    v = builder.CreateGEP(v, ind, "str_addr");
		}
		argsV.push_back(v);
		v = MakeIntegerConstant(type->Size());
	    }
	    else
	    {
		v = arg.expr->CodeGen();
	    }
	    if (!v)
	    {
		return ErrorV(this, "Argument codegen failed");
	    }
	    argsV.push_back(v);
	    llvm::Value* w = MakeIntegerConstant(0);
	    if (arg.width)
	    {
		w = arg.width->CodeGen();
		assert(w && "Expect width expression to generate code ok");
	    }

	    if (!w->getType()->isIntegerTy())
	    {
		return ErrorV(this, "Expected width to be integer value");
	    }
	    argsV.push_back(w);
	    if (type->Type() == Types::TypeDecl::TK_Real)
	    {
		llvm::Value* p;
		if (arg.precision)
		{
		    p = arg.precision->CodeGen();
		    if (!p->getType()->isIntegerTy())
		    {
			return ErrorV(this, "Expected precision to be integer value");
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
	    v = MakeAddressable(arg.expr);
	    argsV.push_back(builder.CreateBitCast(v, Types::GetVoidPtrType()));
	    fn = CreateWriteBinFunc(v->getType(), f->getType());
	}
	v = builder.CreateCall(fn, argsV, "");
    }
    if (isWriteln)
    {
	llvm::Constant* fn = GetFunction(Types::GetVoidType(), { f->getType() }, "__write_nl");
	v = builder.CreateCall(fn, f, "");
    }
    return v;
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

void ReadAST::accept(ASTVisitor& v)
{
    file->accept(v);
    for(auto a : args)
    {
	a->accept(v);
    }
    v.visit(this);
}

static llvm::Constant* CreateReadFunc(Types::TypeDecl* ty, llvm::Type* fty)
{
    std::string suffix;
    llvm::Type* lty = llvm::PointerType::getUnqual(ty->LlvmType());
    std::vector<llvm::Type*> argTypes= { fty, lty };
    switch(ty->Type())
    {
    case Types::TypeDecl::TK_Char:
	suffix = "chr";
	break;

    case Types::TypeDecl::TK_Integer:
	suffix = "int";
	break;
	
    case Types::TypeDecl::TK_Real:
	suffix = "real";
	break;

    case Types::TypeDecl::TK_String:
	suffix = "str";
	break;

    case Types::TypeDecl::TK_Array:
	suffix = "chars";
	break;

    default:
	return ErrorF(0, "Invalid type argument for read");
    }
    return GetFunction(Types::GetVoidType(), argTypes, "__read_" + suffix);
}

static llvm::Constant* CreateReadBinFunc(Types::TypeDecl* ty, llvm::Type* fty)
{
    return GetFunction(Types::GetVoidType(), { fty, Types::GetVoidPtrType() }, "__read_bin");
}

llvm::Value* ReadAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    llvm::Value* f = file->Address();
    llvm::Value* v;
    bool isText = llvm::isa<Types::TextDecl>(file->Type());
    llvm::Type* fTy =  f->getType();
    if (isText && args.empty() && !isReadln)
    {
	return NoOpValue();
    }
    for(auto arg : args)
    {
	std::vector<llvm::Value*> argsV = { f };
	VariableExprAST* vexpr = llvm::dyn_cast<VariableExprAST>(arg);
	assert(vexpr && "Argument for read/readln should be a variable");
	
	Types::TypeDecl* ty = vexpr->Type();
	v = vexpr->Address();
	assert(v && "Could not evaluate address of expression for read");

	if (!isText)
	{
	    v = builder.CreateBitCast(v, Types::GetVoidPtrType());
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

	if (!fn)
	{
	    return 0;
	}
	v = builder.CreateCall(fn, argsV, "");
    }
    if (isReadln)
    {
	assert(isText && "File is not text for readln");
	llvm::Constant* fn = GetFunction(Types::GetVoidType(), { fTy }, "__read_nl");
	v = builder.CreateCall(fn, f, "");
    }
    return v;
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

    BasicDebugInfo(this);

    llvm::Value* v = 0;
    for(auto var : vars)
    {
	// Are we declaring global variables  - no function!
	llvm::Type* ty = var.Type()->LlvmType();
	if (!func)
	{
	    if (Types::FieldCollection* fc = llvm::dyn_cast<Types::FieldCollection>(var.Type()))
	    {
		fc->EnsureSized();
	    }

	    assert(ty && "Type should have a value");
	    llvm::Constant* init;
	    llvm::Constant* nullValue = llvm::Constant::getNullValue(ty);
	    Types::ClassDecl* cd = llvm::dyn_cast<Types::ClassDecl>(var.Type());
	    if (cd && cd->VTableType(true))
	    {
		llvm::GlobalVariable* gv = theModule->getGlobalVariable("vtable_" + cd->Name(), true);
		llvm::StructType* sty = llvm::dyn_cast<llvm::StructType>(ty);
		std::vector<llvm::Constant*> vtable(sty->getNumElements());
		vtable[0] = gv;
		unsigned i = 1;
		while(llvm::Constant* c = nullValue->getAggregateElement(i))
		{
		    vtable[i] = c;
		    i++;
		}
		init = llvm::ConstantStruct::get(sty, vtable);
	    }
	    else
	    {
		init = nullValue;
	    }
	    llvm::GlobalValue::LinkageTypes linkage = (var.IsExternal()?
						       llvm::GlobalValue::ExternalLinkage:
						       llvm::Function::InternalLinkage);

	    llvm::GlobalVariable* gv = new llvm::GlobalVariable(*theModule, ty, false, linkage,
								init, var.Name());
	    const llvm::DataLayout dl(theModule);
	    size_t al = dl.getPrefTypeAlignment(ty);
	    al = std::max(size_t(4), al);
	    gv->setAlignment(al);
	    v = gv;
	    if (debugInfo)
	    {
		DebugInfo& di = GetDebugInfo();
		llvm::DIType* debugType = var.Type()->DebugType(di.builder);
		if (!debugType)
		{
		    // Ugly hack until we have all debug types.
		    std::cerr << "Skipping unknown debug type element." << std::endl;
		    var.Type()->dump();
		    goto skip1;
		}
		{
		int lineNum = this->Loc().LineNumber();
		llvm::DIScope* scope = di.cu;
		llvm::DIFile* unit = scope->getFile();

		di.builder->createGlobalVariableExpression(scope, var.Name(), var.Name(), unit, lineNum,
							   debugType, gv->hasInternalLinkage());
		}
	    skip1:;
	    }
	}
	else
	{	
	    v = CreateAlloca(func->Proto()->LlvmFunction(), var);
	    Types::ClassDecl* cd = llvm::dyn_cast<Types::ClassDecl>(var.Type());
	    if (cd && cd->VTableType(true))
	    {
		llvm::GlobalVariable* gv = theModule->getGlobalVariable("vtable_" + cd->Name(), true);
		std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), MakeIntegerConstant(0) };
		llvm::Value* dest = builder.CreateGEP(v, ind, "vtable");
		builder.CreateStore(gv, dest);
	    }
	    if (debugInfo)
	    {
		DebugInfo& di = GetDebugInfo();
		assert(!di.lexicalBlocks.empty() && "Should not have empty lexicalblocks here!");
		llvm::DIScope* sp = di.lexicalBlocks.back();
		llvm::DIType* debugType = var.Type()->DebugType(di.builder);
		if (!debugType)
		{
		    // Ugly hack until we have all debug types.
		    std::cerr << "Skipping unknown debug type element." << std::endl;
		    var.Type()->dump();
		    goto skip;
		}
		{
	        // Create a debug descriptor for the variable.
		int lineNum = this->Loc().LineNumber();
		llvm::DIFile* unit = sp->getFile();
		llvm::DILocalVariable* dv =
		    di.builder->createAutoVariable(sp, var.Name(), unit, lineNum,
						   debugType, true);
		di.builder->insertDeclare(v, dv, di.builder->createExpression(),
					  llvm::DebugLoc::get(lineNum, 0, sp),
					  ::builder.GetInsertBlock());
		}
	    skip: ;
	    }
	}
	if (!variables.Add(var.Name(), v))
	{
	    return ErrorV(this, "Duplicate name " + var.Name() + "!");
	}
    }
    return v;
}

void LabelExprAST::DoDump(std::ostream& out) const
{
    bool first = true;
    for(auto l : labelValues)
    {
	if (!first)
	{
	    out << ", ";
	}
	out << l;
	first = false;
    }
    out << ": ";
    stmt->dump(out);
}

llvm::Value* LabelExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    assert(!stmt && "Expected no statement for 'goto' label expression");
    llvm::BasicBlock* labelBB = CreateGotoTarget(labelValues[0]);
    // Make LLVM-IR valid by jumping to the neew block!
    llvm::Value* v = builder.CreateBr(labelBB);
    builder.SetInsertPoint(labelBB);

    return v;
}

llvm::Value* LabelExprAST::CodeGen(llvm::SwitchInst* sw, llvm::BasicBlock* afterBB, llvm::Type* ty)
{
    TRACE();

    BasicDebugInfo(this);

    assert(stmt && "Expected a statement for 'case' label expression");
    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(theContext, "case", theFunction);

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

void LabelExprAST::accept(ASTVisitor& v)
{
    if (stmt)
    {
	stmt->accept(v);
    }
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

void CaseExprAST::accept(ASTVisitor& v)
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

    BasicDebugInfo(this);

    llvm::Value* v  = expr->CodeGen();
    llvm::Type*  ty = v->getType();
    if (!v->getType()->isIntegerTy())
    {
	return ErrorV(this, "Case selection must be integral type");
    }

    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(theContext, "after", theFunction);
    llvm::BasicBlock* defaultBB = afterBB;
    if (otherwise)
    {
	defaultBB = llvm::BasicBlock::Create(theContext, "default", theFunction);
	
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
    static int index = 1;
    llvm::Type* ty = type->LlvmType();
    assert(ty && "Expect type for set to work");

    Types::SetDecl::ElemType elems[Types::SetDecl::MaxSetWords] = {};
    for(auto v : values)
    {
	if (RangeExprAST* r = llvm::dyn_cast<RangeExprAST>(v))
	{
	    IntegerExprAST* le = llvm::dyn_cast<IntegerExprAST>(r->LowExpr());
	    IntegerExprAST* he = llvm::dyn_cast<IntegerExprAST>(r->HighExpr());
	    int start = type->GetRange()->Start();
	    int low = le->Int() - start;
	    int high = he->Int() - start;
	    low = std::max(0, low);
	    high = std::min((int)type->GetRange()->Size(), high);
	    for(int i = low; i <= high; i++)
	    {
		elems[i >> Types::SetDecl::SetPow2Bits] |= (1 << (i & Types::SetDecl::SetMask));
	    }
	}
	else
	{
	    IntegerExprAST* e = llvm::dyn_cast<IntegerExprAST>(v);
	    int start = type->GetRange()->Start();
	    unsigned i = e->Int() - start;
	    if (i < (unsigned)type->GetRange()->Size())
	    {
		elems[i >> Types::SetDecl::SetPow2Bits] |= (1 << (i & Types::SetDecl::SetMask));
	    }
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
    std::string name("P" + std::to_string(index) + ".set");
    llvm::GlobalVariable* gv = new llvm::GlobalVariable(*theModule, ty, false, linkage,
							init, name);

    delete [] initArr;

    return gv;
}

llvm::Value* SetExprAST::Address()
{
    TRACE();

    assert(type && "No type supplied");

    assert(type->GetRange()->Size() <= Types::SetDecl::MaxSetSize && "Size too large?");

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

    llvm::Value* setV = CreateTempAlloca(type);
    assert(setV && "Expect CreateTempAlloca() to work");

    llvm::Value* tmp = builder.CreateBitCast(setV, Types::GetVoidPtrType());

    size_t size = llvm::dyn_cast<Types::SetDecl>(type)->SetWords();
    builder.CreateMemSet(tmp, MakeConstant(0, Types::GetCharType()),
			 (size * Types::SetDecl::SetBits / 8), 0);

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
	    assert(high && low && "Expected expressions to evalueate");

	    llvm::Function* fn = builder.GetInsertBlock()->getParent();

	    llvm::Value* loopVar = CreateTempAlloca(Types::GetIntegerType());

	    // Adjust for range:
	    Types::Range* range = type->GetRange();

	    low  = builder.CreateSExt(low, Types::GetIntegerType()->LlvmType(), "sext.low");
	    high = builder.CreateSExt(high, Types::GetIntegerType()->LlvmType(), "sext.high");

	    llvm::Value* rangeStart = MakeIntegerConstant(range->Start());
	    low = builder.CreateSub(low, rangeStart);
	    high = builder.CreateSub(high, rangeStart);

	    builder.CreateStore(low, loopVar);

	    llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(theContext, "loop", fn);
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
	    llvm::Value* bitset = builder.CreateLoad(bitsetAddr, "bitset");
	    bitset = builder.CreateOr(bitset, bit);
	    builder.CreateStore(bitset, bitsetAddr);

	    loop = builder.CreateAdd(loop, MakeConstant(1, llvm::dyn_cast<Types::SetDecl>(type)->SubType()),
				     "update");
	    builder.CreateStore(loop, loopVar);

	    llvm::Value* endCond = builder.CreateICmpSLE(loop, high, "loopcond");

	    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(theContext, "afterloop", fn);
	    builder.CreateCondBr(endCond, loopBB, afterBB);

	    builder.SetInsertPoint(afterBB);
	}
	else
	{
	    Types::Range* range = type->GetRange();
	    llvm::Value* rangeStart = MakeIntegerConstant(range->Start());
	    llvm::Value* x = v->CodeGen();
	    assert(x && "Expect codegen to work!");
	    x = builder.CreateZExt(x, Types::GetIntegerType()->LlvmType(), "zext");
	    x = builder.CreateSub(x, rangeStart);

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
	    llvm::Value* bitset = builder.CreateLoad(bitsetAddr, "bitset");
	    bitset = builder.CreateOr(bitset, bit);
	    builder.CreateStore(bitset, bitsetAddr);
	}
    }

    return setV;
}

void WithExprAST::DoDump(std::ostream& out) const
{
    out << "With ... do " << std::endl;
}

llvm::Value* WithExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

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

    assert(expr->Type()->IsIntegral() && "Index is supposed to be integral type");
    llvm::Value* index = expr->CodeGen();

    llvm::Type* ty = index->getType();
    if (int start = range->Start())
    {
	index = builder.CreateSub(index, MakeConstant(start, expr->Type()));
    }
    if (ty->getPrimitiveSizeInBits() < Types::GetIntegerType()->LlvmType()->getPrimitiveSizeInBits())
    {
	if (expr->Type()->IsUnsigned())
	{
	    index = builder.CreateZExt(index, Types::GetIntegerType()->LlvmType(), "zext");
	}
	else
	{
	    index = builder.CreateSExt(index, Types::GetIntegerType()->LlvmType(), "sext");
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
    llvm::Type* intTy = Types::GetIntegerType()->LlvmType();

    int start = range->Start();
    if (start)
    {
	index = builder.CreateSub(index, MakeConstant(start, expr->Type()));
    }
    if (index->getType()->getPrimitiveSizeInBits() < intTy->getPrimitiveSizeInBits())
    {
	if (expr->Type()->IsUnsigned())
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
    llvm::BasicBlock* oorBlock = llvm::BasicBlock::Create(theContext, "out_of_range");
    llvm::BasicBlock* contBlock = llvm::BasicBlock::Create(theContext, "continue",
							   theFunction);
    builder.CreateCondBr(cmp, oorBlock, contBlock);

    theFunction->getBasicBlockList().push_back(oorBlock);
    builder.SetInsertPoint(oorBlock);
    std::vector<llvm::Value*> args = { builder.CreateGlobalStringPtr(Loc().FileName()),
				       MakeIntegerConstant(Loc().LineNumber()),
				       MakeIntegerConstant(start),
				       MakeIntegerConstant(end),
				       orig_index };
    std::vector<llvm::Type*> argTypes = { llvm::PointerType::getUnqual(Types::GetCharType()->LlvmType()),
					  intTy,
					  intTy,
					  intTy,
					  intTy };

    llvm::Constant* fn = GetFunction(Types::GetVoidPtrType(), argTypes, "range_error");

    builder.CreateCall(fn, args, "");
    builder.CreateUnreachable();

    builder.SetInsertPoint(contBlock);
    return index;
}

void TypeCastAST::DoDump(std::ostream& out) const
{
    out << "TypeCast to ";
    type->dump(out);
    out << "(";
    expr->dump(out);
    out << ")";
}

static llvm::Value* ConvertSet(ExprAST* expr, Types::TypeDecl* type)
{
    TRACE();

    Types::TypeDecl* current = expr->Type();
    Types::SetDecl* rty = llvm::dyn_cast<Types::SetDecl>(current);
    Types::SetDecl* lty = llvm::dyn_cast<Types::SetDecl>(type);

    llvm::Value* dest = CreateTempAlloca(type);

    assert(lty && rty && "Expect types on both sides");
    Types::Range* rrange = rty->GetRange();
    Types::Range* lrange = lty->GetRange();

    llvm::Value* w;
    llvm::Value* ind[2] = { MakeIntegerConstant(0), 0 };
    llvm::Value* src = MakeAddressable(expr);
    size_t p = 0;
    for(auto i = lrange->Start(); i < lrange->End(); i += Types::SetDecl::SetBits)
    {
	if (i >= (rrange->Start() & ~Types::SetDecl::SetMask) && i < rrange->End())
	{
	    if (i > rrange->Start())
	    {
		size_t index = i - rrange->Start();
		size_t sp = index >> Types::SetDecl::SetPow2Bits;
		ind[1] = MakeIntegerConstant(sp);
		llvm::Value* srci = builder.CreateGEP(src, ind, "srci");
		w = builder.CreateLoad(srci, "w");
		if (size_t shift = (index & Types::SetDecl::SetMask))
		{
		    w = builder.CreateLShr(w, shift, "wsh");
		    if (sp + 1 < rty->SetWords())
		    {
			ind[1] = MakeIntegerConstant(sp + 1);
			llvm::Value* xp = builder.CreateGEP(src, ind, "srcip1");
			llvm::Value* x = builder.CreateLoad(xp, "x");
			x = builder.CreateShl(x, 32-shift);
			w = builder.CreateOr(w, x);
		    }
		}
	    }
	    else
	    {
		ind[1] = MakeIntegerConstant(0);
		llvm::Value* srci = builder.CreateGEP(src, ind, "srci");
		w = builder.CreateLoad(srci, "w");
		w = builder.CreateShl(w, rrange->Start() - i, "wsh");
	    }
	}
	else
	{
	    w = MakeIntegerConstant(0);
	}
	ind[1] = MakeIntegerConstant(p);
	llvm::Value* desti = builder.CreateGEP(dest, ind);
	builder.CreateStore(w, desti);
	p++;
    }
    return dest;
}

llvm::Value* TypeCastAST::CodeGen()
{
    if (type->Type() == Types::TypeDecl::TK_Real)
    {
	return builder.CreateSIToFP(expr->CodeGen(), type->LlvmType(), "tofp");
    }

    Types::TypeDecl* current = expr->Type();
    if (type->IsIntegral() && current->IsIntegral())
    {
	if (type->IsUnsigned())
	{
	    return builder.CreateZExt(expr->CodeGen(), type->LlvmType());
	}
	return builder.CreateSExt(expr->CodeGen(), type->LlvmType());
    }
    if (llvm::isa<Types::PointerDecl>(type))
    {
	return builder.CreateBitCast(expr->CodeGen(), type->LlvmType());
    }
    if (((current->Type() == Types::TypeDecl::TK_Array &&
	  current->SubType()->Type() == Types::TypeDecl::TK_Char) ||
	 current->Type() == Types::TypeDecl::TK_Char) &&
	type->Type() == Types::TypeDecl::TK_String)
    {
	return MakeStringFromExpr(expr, type);
    }
    if (type->Type() == Types::TypeDecl::TK_Class &&
	current->Type() == Types::TypeDecl::TK_Class)
    {
	llvm::Type* ty = llvm::PointerType::getUnqual(type->LlvmType());
	return builder.CreateLoad(builder.CreateBitCast(Address(), ty));
    }
    if (type->Type() == Types::TypeDecl::TK_Char &&
	current->Type() == Types::TypeDecl::TK_String)
    {
	return builder.CreateLoad(Address(), "char");
    }
    // Sets are compatible anyway...
    if (current->Type() == Types::TypeDecl::TK_Set)
    {
	return builder.CreateLoad(Address(), "set");
    }
    dump();
    assert(0 && "Expected to get something out of this function");
    return 0;
}

llvm::Value* TypeCastAST::Address()
{
    llvm::Value* v = 0;
    Types::TypeDecl* current = expr->Type();
    switch(current->Type())
    {
    case Types::TypeDecl::TK_String:
	v = MakeAddressable(expr);
	break;
    case Types::TypeDecl::TK_Set:
	v = ConvertSet(expr, type);
	break;
    default:
	if (type->Type() == Types::TypeDecl::TK_String)
	{
	    v = MakeStringFromExpr(expr, type);
	}
	else if (AddressableAST* ae = llvm::dyn_cast<AddressableAST>(expr))
	{
	    v = ae->Address();
	}
    }

    assert(v && "Expected to get a value here...");
    if (type->Type() == Types::TypeDecl::TK_Char)
    {
	llvm::Value* ind[2] = { MakeIntegerConstant(0), MakeIntegerConstant(1) };
        return builder.CreateGEP(v, ind);
    }

    return builder.CreateBitCast(v, llvm::PointerType::getUnqual(type->LlvmType()));
}

llvm::Value* SizeOfExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

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
    TRACE();

    BasicDebugInfo(this);

    return bif->CodeGen(builder);
}

void BuiltinExprAST::accept(ASTVisitor& v)
{
    bif->accept(v);
    v.visit(this);
}

void VTableAST::DoDump(std::ostream& out) const
{
    out << "VTable(";
    type->dump();
    out << ")" << std::endl;
}

llvm::Value* VTableAST::CodeGen()
{
    TRACE();

    llvm::StructType* ty = llvm::dyn_cast_or_null<llvm::StructType>(classDecl->VTableType(false));
    assert(ty && "Huh? No vtable?");
    std::vector<llvm::Constant*> vtInit = GetInitializer();

    std::string name = "vtable_" + classDecl->Name();
    llvm::Constant* init = (vtInit.size())?llvm::ConstantStruct::get(ty, vtInit):0;
    vtable = new llvm::GlobalVariable(*theModule, ty, true, llvm::Function::InternalLinkage, init, name);
    if (!init)
    {
	vtableBackPatchList.push_back(this);
    }
    return vtable;
}

std::vector<llvm::Constant*> VTableAST::GetInitializer()
{
    std::vector<llvm::Constant*> vtInit(classDecl->NumVirtFuncs());
    for(size_t i = 0; i < classDecl->MembFuncCount(); i++)
    {
	Types::MemberFuncDecl* m = classDecl->GetMembFunc(i);
	if (m->IsVirtual() || m->IsOverride())
	{
	    if (llvm::Constant* c = theModule->getFunction("P." + m->LongName()))
	    {
		vtInit[m->VirtIndex()] = c;
	    }
	    else
	    {
		vtInit.clear();
		break;
	    }
	}
    }
    return vtInit;
}

void VTableAST::Fixup()
{
    llvm::StructType* ty = llvm::dyn_cast<llvm::StructType>(classDecl->VTableType(true));
    std::vector<llvm::Constant*> vtInit = GetInitializer();
    assert(vtInit.size() && "Should have something to initialize here");

    llvm::Constant* init = llvm::ConstantStruct::get(ty, vtInit);
    vtable->setInitializer(init);
}

VirtFunctionAST::VirtFunctionAST(const Location& w, VariableExprAST* slf, int idx, Types::TypeDecl* ty)
	: AddressableAST(w, EK_VirtFunction, ty), index(idx), self(slf)
{
    assert(index >= 0 && "Index should not be negative!");
}

void VirtFunctionAST::DoDump(std::ostream& out) const
{
    out << "VirtFunctionAST: " << std::endl;
}

llvm::Value* VirtFunctionAST::Address()
{
    llvm::Value* v = self->Address();
    std::vector<llvm::Value*> ind = {MakeIntegerConstant(0), MakeIntegerConstant(0)};
    v = builder.CreateGEP(v, ind, "vptr");
    ind[1] = MakeIntegerConstant(index);
    v = builder.CreateLoad(v, "vtable");
    v = builder.CreateGEP(v, ind, "mfunc");
    return v;
}

void GotoAST::DoDump(std::ostream& out) const
{
    out << "Goto " << dest << std::endl;
}

llvm::Value* GotoAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    llvm::BasicBlock* labelBB = CreateGotoTarget(dest);
    // Make LLVM-IR valid by jumping to the neew block!
    llvm::Value* v = builder.CreateBr(labelBB);
    // FIXME: This should not be needed, semantics should remove code after goto!
    llvm::Function* fn = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* dead = llvm::BasicBlock::Create(theContext, "dead", fn);
    builder.SetInsertPoint(dead);
    return v;
}

void UnitAST::DoDump(std::ostream& out) const
{
    out << "Unit "  << std::endl;
}

void UnitAST::accept(ASTVisitor& v)
{
    for(auto i : code)
    {
	i->accept(v);
    }
    if (initFunc)
    {
	initFunc->accept(v);
    }
}

llvm::Value* UnitAST::CodeGen()
{
    TRACE();

    DebugInfo di;
    if(debugInfo)
    {
	Location loc = Loc();

	// TODO: Fix path and add flags.
	di.builder = new llvm::DIBuilder(*theModule, true);
	llvm::DIFile* file = di.builder->createFile(loc.FileName(), ".");
	di.cu = di.builder->createCompileUnit(llvm::dwarf::DW_LANG_Pascal83, file,
					      "Lacsap", optimization >= O1, "", 0);

	debugStack.push_back(&di);
    }

    for(auto a : code)
    {
	if (!a->CodeGen())
	{
	    return 0;
	}
    }
    if (initFunc)
    {
	initFunc->CodeGen();
	if (initFunc->Proto()->Name() != "__PascalMain")
	{
	    unitInit.push_back(initFunc);
	}
    }
    if (debugInfo)
    {
	debugStack.pop_back();
    }
    return NoOpValue();
}

void ClosureAST::DoDump(std::ostream& out) const
{
    out << "Closure ";
    for(auto c : content)
    {
	c->dump();
    }
    out << std::endl;
}

llvm::Value* ClosureAST::CodeGen()
{
    TRACE();

    std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), 0 };
    llvm::Function* fn = builder.GetInsertBlock()->getParent();
    llvm::Value* closure = CreateNamedAlloca(fn, type, "$$ClosureStruct");
    int index = 0;
    for(auto u : content)
    {
	llvm::Value* v = u->Address();
	ind[1] = MakeIntegerConstant(index);
	llvm::Value* ptr = builder.CreateGEP(closure, ind, u->Name());
	builder.CreateStore(v, ptr);
	index++;
    }
    return closure;
}

void TrampolineAST::DoDump(std::ostream& out) const
{
    out << "Trampoline for ";
    func->DoDump(out);
}

llvm::Value* TrampolineAST::CodeGen()
{
    TRACE();

    llvm::Function* destFn = func->Proto()->LlvmFunction();
    // Temporary memory to store the trampoline itself.
    llvm::Type* trampTy = llvm::ArrayType::get(Types::GetCharType()->LlvmType(), 32);
    llvm::Value* tramp = builder.CreateAlloca(trampTy, 0, "tramp");
    std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), MakeIntegerConstant(0) };
    llvm::Value* tptr = builder.CreateGEP(tramp, ind, "tramp.first");
    llvm::Value* nest = closure->CodeGen();
    llvm::Type* voidPtrTy = Types::GetVoidPtrType();
    nest = builder.CreateBitCast(nest, voidPtrTy, "closure");
    llvm::Value* castFn = builder.CreateBitCast(destFn, voidPtrTy, "destFn");
    llvm::Constant* llvmTramp = GetFunction(Types::GetVoidType()->LlvmType(),
					    { voidPtrTy, voidPtrTy, voidPtrTy },
					    "llvm.init.trampoline");
    builder.CreateCall(llvmTramp, { tptr, castFn, nest });
    llvm::Constant* llvmAdjust = GetFunction(voidPtrTy, { voidPtrTy }, "llvm.adjust.trampoline");
    llvm::Value* ptr = builder.CreateCall(llvmAdjust, { tptr }, "tramp.ptr");

    return builder.CreateBitCast(ptr, funcPtrTy->LlvmType(), "tramp.func");
}

void TrampolineAST::accept(ASTVisitor& v)
{
    func->accept(v);
}

static void BuildUnitInitList()
{
    std::vector<llvm::Constant*> unitList(unitInit.size()+1);
    llvm::Type* vp = Types::GetVoidPtrType();
    int index = 0;
    for(auto v : unitInit)
    {
	llvm::Function* fn = theModule->getFunction("P." + v->Proto()->Name());
	assert(fn && "Expected to find the function!");
	unitList[index] = llvm::ConstantExpr::getBitCast(fn, vp);
	index++;
    }
    unitList[unitInit.size()] = llvm::Constant::getNullValue(vp);
    llvm::ArrayType* arr = llvm::ArrayType::get(vp, unitInit.size()+1);
    llvm::Constant* init = llvm::ConstantArray::get(arr, unitList);
    llvm::Value* unitInitList = new llvm::GlobalVariable(*theModule, arr, true,
							 llvm::GlobalValue::ExternalLinkage, init,
							 "UnitIniList");
    (void) unitInitList;
    assert(unitInitList && "Unit Initializer List not built correctly?");
}

void BackPatch()
{
    for(auto v : vtableBackPatchList)
    {
	v->Fixup();
    }
    BuildUnitInitList();
}
