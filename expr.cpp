#include "expr.h"
#include "builtin.h"
#include "options.h"
#include "stack.h"
#include "trace.h"
#include "types.h"
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APSInt.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_os_ostream.h>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <sstream>

#if !NDEBUG
template<>
void Stack<llvm::Value*>::dump() const
{
    int n = 0;
    for (auto s : stack)
    {
	std::cerr << "Level " << n << std::endl;
	n++;
	for (auto v : s)
	{
	    std::cerr << v.first << ": ";
	    v.second->dump();
	    std::cerr << std::endl;
	}
    }
}
#endif

class DebugInfo
{
public:
    DebugInfo() : cu(0), builder(0) {}
    llvm::DICompileUnit*        cu;
    llvm::DIBuilder*            builder;
    std::vector<llvm::DIScope*> lexicalBlocks;
    void                        EmitLocation(Location loc);
    ~DebugInfo();
};

class Label
{
public:
    Label(llvm::BasicBlock* b, int lab) : bb(b), label(lab) {}
    void              dump() { std::cerr << "label: " << label << std::endl; }
    llvm::BasicBlock* GetBB() { return bb; }

private:
    llvm::BasicBlock* bb;
    int               label;
};

typedef Stack<Label*>              LabelStack;
typedef StackWrapper<Label*>       LabelWrapper;
typedef Stack<llvm::Value*>        VarStack;
typedef StackWrapper<llvm::Value*> VarStackWrapper;

const size_t MEMCPY_THRESHOLD = 16;

extern llvm::Module* theModule;

static VarStack                  variables;
static LabelStack                labels;
llvm::LLVMContext                theContext;
static llvm::IRBuilder<>         builder(theContext);
static int                       errCnt;
static std::vector<VTableAST*>   vtableBackPatchList;
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
    llvm::DILocation* diloc = llvm::DILocation::get(scope->getContext(), loc.LineNumber(), loc.Column(),
                                                    scope);
    ::builder.SetCurrentDebugLocation(llvm::DebugLoc(diloc));
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
    llvm::Function*   theFunction = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(theContext, name, theFunction);
    labels.Add(name, new Label(bb, label));
    return bb;
}

std::string ShortName(const std::string& name)
{
    std::string            shortname = name;
    std::string::size_type pos = name.find_last_of('$');
    if (pos != std::string::npos)
    {
	shortname = shortname.substr(pos + 1);
    }
    return shortname;
}

llvm::FunctionCallee GetFunction(llvm::Type* resTy, const std::vector<llvm::Type*>& args,
                                 const std::string& name)
{
    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, args, false);
    return theModule->getOrInsertFunction(name, ft);
}

llvm::FunctionCallee GetFunction(Types::TypeDecl* res, const std::vector<llvm::Type*>& args,
                                 llvm::Value* callee)
{
    llvm::Type*         resTy = res->LlvmType();
    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, args, false);
    return llvm::FunctionCallee(ft, callee);
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

static llvm::AllocaInst* CreateNamedAlloca(llvm::Function* fn, Types::TypeDecl* ty, const std::string& name)
{
    TRACE();
    // Save where we were...
    llvm::BasicBlock*          bb = builder.GetInsertBlock();
    llvm::BasicBlock::iterator ip = builder.GetInsertPoint();

    llvm::IRBuilder<> bld(&fn->getEntryBlock(), fn->getEntryBlock().begin());

    assert(ty && "Must have type passed in");
    llvm::Type* type = ty->LlvmType();

    llvm::AllocaInst* a = bld.CreateAlloca(type, 0, name);
    size_t            align = std::max(ty->AlignSize(), MIN_ALIGN);
    if (a->getAlignment() < align)
    {
	a->setAlignment(llvm::Align(align));
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

llvm::AllocaInst* CreateTempAlloca(Types::TypeDecl* ty)
{
    // Get the "entry" block
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

void ExprAST::dump() const
{
    std::cerr << "Node=" << reinterpret_cast<const void*>(this) << ": ";
    DoDump();
    std::cerr << std::endl;
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
    return MakeConstant(val, Types::Get<Types::IntegerDecl>());
}

llvm::Constant* MakeBooleanConstant(int val)
{
    return MakeConstant(val, Types::Get<Types::BoolDecl>());
}

static llvm::Constant* MakeCharConstant(int val)
{
    return MakeConstant(val, Types::Get<Types::CharDecl>());
}

static llvm::Type* MakeVoidType()
{
    return llvm::Type::getVoidTy(theContext);
}

static llvm::Value* TempStringFromStringExpr(llvm::Value* dest, StringExprAST* rhs)
{
    TRACE();
    std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), MakeIntegerConstant(0) };
    llvm::Value*              dest1 = builder.CreateGEP(dest, ind, "str_0");

    ind[1] = MakeIntegerConstant(1);
    llvm::Value* dest2 = builder.CreateGEP(dest, ind, "str_1");

    llvm::Value* v = rhs->CodeGen();
    builder.CreateStore(MakeCharConstant(rhs->Str().size()), dest1);

    llvm::Align dest_align{ std::max(AlignOfType(dest2->getType()), MIN_ALIGN) };
    llvm::Align src_align{ std::max(AlignOfType(v->getType()), MIN_ALIGN) };
    return builder.CreateMemCpy(dest2, dest_align, v, src_align, rhs->Str().size());
}

static llvm::Value* TempStringFromChar(llvm::Value* dest, ExprAST* rhs)
{
    TRACE();
    assert(llvm::isa<Types::CharDecl>(rhs->Type()) && "Expected char value");
    std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), MakeIntegerConstant(0) };
    llvm::Value*              dest1 = builder.CreateGEP(dest, ind, "str_0");

    ind[1] = MakeIntegerConstant(1);
    llvm::Value* dest2 = builder.CreateGEP(dest, ind, "str_1");

    builder.CreateStore(MakeCharConstant(1), dest1);
    return builder.CreateStore(rhs->CodeGen(), dest2);
}

static llvm::Value* LoadOrMemcpy(llvm::Value* src, Types::TypeDecl* ty)
{
    llvm::Value* dest = CreateTempAlloca(ty);
    size_t       size = ty->Size();
    if (!disableMemcpyOpt && size >= MEMCPY_THRESHOLD)
    {
	llvm::Align dest_align{ std::max(AlignOfType(dest->getType()), MIN_ALIGN) };
	llvm::Align src_align{ std::max(AlignOfType(src->getType()), MIN_ALIGN) };
	builder.CreateMemCpy(dest, dest_align, src, src_align, size);
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

void RealExprAST::DoDump() const
{
    std::cerr << "Real: " << val;
}

llvm::Value* RealExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    return llvm::ConstantFP::get(theContext, llvm::APFloat(val));
}

void IntegerExprAST::DoDump() const
{
    std::cerr << "Integer: " << val;
}

llvm::Value* IntegerExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    return MakeConstant(val, type);
}

void NilExprAST::DoDump() const
{
    std::cerr << "Nil:";
}

llvm::Value* NilExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    return llvm::ConstantPointerNull::get(llvm::dyn_cast<llvm::PointerType>(Types::GetVoidPtrType()));
}

void CharExprAST::DoDump() const
{
    std::cerr << "Char: '";
    if (isprint(val))
	std::cerr << char(val);
    else
	std::cerr << "\\0x" << std::hex << val << std::dec;
    std::cerr << "'";
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

void VariableExprAST::DoDump() const
{
    std::cerr << "Variable: " << name << " ";
    Type()->dump();
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

ArrayExprAST::ArrayExprAST(const Location& loc, ExprAST* v, const std::vector<ExprAST*>& inds,
                           const std::vector<Types::RangeDecl*>& r, Types::TypeDecl* ty)
    : AddressableAST(loc, EK_ArrayExpr, ty), expr(v), indices(inds), ranges(r)
{
    size_t mul = 1;
    for (auto j = ranges.end() - 1; j >= ranges.begin(); j--)
    {
	indexmul.push_back(mul);
	mul *= (*j)->GetRange()->Size();
    }
    std::reverse(indexmul.begin(), indexmul.end());
}

void ArrayExprAST::DoDump() const
{
    std::cerr << "Array: ";
    expr->dump();
    std::cerr << "[";
    bool first = true;
    for (auto i : indices)
    {
	if (!first)
	{
	    std::cerr << ", ";
	}
	first = false;
	i->dump();
    }
}

llvm::Value* ArrayExprAST::Address()
{
    TRACE();
    llvm::Value* v = MakeAddressable(expr);
    assert(v && "Expected variable to have an address");
    EnsureSized();
    llvm::Value* totalIndex = 0;
    for (size_t i = 0; i < indices.size(); i++)
    {
	auto range = llvm::dyn_cast<RangeReduceAST>(indices[i]);
	assert(range && "Expected a range here");
	llvm::Value* index = range->CodeGen();
	assert(index && "Expression failed for index");
	if (indexmul[i] != 1)
	{
	    index = builder.CreateMul(index, MakeConstant(indexmul[i], range->Type()));
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
    for (auto i : indices)
    {
	i->accept(v);
    }
    expr->accept(v);
    v.visit(this);
}

DynArrayExprAST::DynArrayExprAST(const Location& loc, ExprAST* v, ExprAST* idx, Types::DynRangeDecl* r,
                                 Types::TypeDecl* ty)
    : AddressableAST(loc, EK_DynArrayExpr, ty), expr(v), index(idx), range(r)
{
}

llvm::Value* DynArrayExprAST::Address()
{
    llvm::Value* v = MakeAddressable(expr);
    assert(v && "Expected variable to have an address");
    EnsureSized();
    llvm::Value* idx = index->CodeGen();

    v = builder.CreateGEP(v, { MakeIntegerConstant(0), MakeIntegerConstant(0) }, "ptr");
    v = builder.CreateLoad(v, "ptrLoad");
    return builder.CreateGEP(v, idx, "valueIndex");
}

void DynArrayExprAST::accept(ASTVisitor& v)
{
    index->accept(v);
    expr->accept(v);
    v.visit(this);
}

void FieldExprAST::DoDump() const
{
    std::cerr << "Field " << element << std::endl;
    expr->DoDump();
}

llvm::Value* FieldExprAST::Address()
{
    TRACE();
    EnsureSized();
    if (llvm::Value* v = MakeAddressable(expr))
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

void VariantFieldExprAST::DoDump() const
{
    std::cerr << "Variant " << element << std::endl;
    expr->DoDump();
}

llvm::Value* VariantFieldExprAST::Address()
{
    TRACE();
    EnsureSized();
    llvm::Value*              v = MakeAddressable(expr);
    std::vector<llvm::Value*> ind{ MakeIntegerConstant(0), MakeIntegerConstant(element) };
    v = builder.CreateGEP(v, ind, "valueindex");
    return builder.CreateBitCast(v, llvm::PointerType::getUnqual(Type()->LlvmType()));
}

void PointerExprAST::DoDump() const
{
    std::cerr << "Pointer:";
    pointer->dump();
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

void FilePointerExprAST::DoDump() const
{
    std::cerr << "FilePointer:";
    pointer->dump();
}

llvm::Value* FilePointerExprAST::Address()
{
    TRACE();
    VariableExprAST* vptr = llvm::dyn_cast<VariableExprAST>(pointer);
    assert(vptr && "Expected variable expression!");
    llvm::Value*              v = vptr->Address();
    std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), MakeIntegerConstant(Types::FileDecl::Buffer) };
    v = builder.CreateGEP(v, ind, "bufptr");
    return builder.CreateLoad(v, "buffer");
}

void FilePointerExprAST::accept(ASTVisitor& v)
{
    pointer->accept(v);
    v.visit(this);
}

void FunctionExprAST::DoDump() const
{
    std::cerr << "Function " << name;
}

llvm::Value* FunctionExprAST::CodeGen()
{
    assert(proto->Function());

    BasicDebugInfo(this);
    return proto->LlvmFunction();
}

static int StringishScore(ExprAST* e)
{
    if (llvm::isa<CharExprAST>(e) || llvm::isa<Types::CharDecl>(e->Type()))
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

void BinaryExprAST::DoDump() const
{
    std::cerr << "BinaryOp: ";
    lhs->dump();
    oper.dump();
    rhs->dump();
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
    if (name == "SymDiff")
    {
	return builder.CreateXor(res, src);
    }
    if (name == "Diff")
    {
	src = builder.CreateNot(src);
	return builder.CreateAnd(res, src);
    }
    return 0;
}

llvm::Value* BinaryExprAST::InlineSetFunc(const std::string& name)
{
    if (optimization >= O1 && (name == "Union" || name == "Intersect" || name == "Diff" || name == "SymDiff"))
    {
	Types::TypeDecl* type = rhs->Type();

	assert(*type == *lhs->Type() && "Expect same types");
	llvm::Value* rV = MakeAddressable(rhs);
	llvm::Value* lV = MakeAddressable(lhs);

	size_t words = llvm::dyn_cast<Types::SetDecl>(type)->SetWords();
	assert(rV && lV && "Should have generated values for left and right set");

	llvm::Value* v = CreateTempAlloca(type);
	for (size_t i = 0; i < words; i++)
	{
	    std::vector<llvm::Value*> ind{ MakeIntegerConstant(0), MakeIntegerConstant(i) };
	    llvm::Value*              lAddr = builder.CreateGEP(lV, ind, "leftSet");
	    llvm::Value*              rAddr = builder.CreateGEP(rV, ind, "rightSet");
	    llvm::Value*              vAddr = builder.CreateGEP(v, ind, "resSet");
	    llvm::Value*              res = builder.CreateLoad(lAddr, "laddr");
	    llvm::Value*              tmp = builder.CreateLoad(rAddr, "raddr");
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

    if (llvm::Value* inl = InlineSetFunc(name))
    {
	return inl;
    }

    Types::SetDecl* type = llvm::dyn_cast<Types::SetDecl>(rhs->Type());
    assert(*type == *lhs->Type() && "Expect both sides to have same type");
    assert(type && "Expect to get a type");

    llvm::Value* rV = MakeAddressable(rhs);
    llvm::Value* lV = MakeAddressable(lhs);
    assert(rV && lV && "Should have generated values for left and right set");

    llvm::Type*  pty = llvm::PointerType::getUnqual(type->LlvmType());
    llvm::Value* setWords = MakeIntegerConstant(type->SetWords());
    llvm::Type*  intTy = setWords->getType();
    if (resTyIsSet)
    {
	llvm::FunctionCallee f = GetFunction(MakeVoidType(), { pty, pty, pty, intTy }, "__Set" + name);

	llvm::Value*              v = CreateTempAlloca(type);
	std::vector<llvm::Value*> args = { v, lV, rV, setWords };
	builder.CreateCall(f, args);
	return builder.CreateLoad(v, "set");
    }

    llvm::FunctionCallee f = GetFunction(Types::Get<Types::BoolDecl>()->LlvmType(), { pty, pty, intTy },
                                         "__Set" + name);
    return builder.CreateCall(f, { lV, rV, setWords }, "calltmp");
}

llvm::Value* MakeStringFromExpr(ExprAST* e, Types::TypeDecl* ty)
{
    TRACE();
    if (llvm::isa<Types::CharDecl>(e->Type()))
    {
	llvm::Value* v = CreateTempAlloca(Types::Get<Types::StringDecl>(255));
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

llvm::Value* CallStrFunc(const std::string& name, ExprAST* lhs, ExprAST* rhs, Types::TypeDecl* resTy,
                         const std::string& twine)
{
    TRACE();
    llvm::Value* rV = MakeStringFromExpr(rhs, rhs->Type());
    llvm::Value* lV = MakeStringFromExpr(lhs, lhs->Type());

    llvm::FunctionCallee f = GetFunction(resTy->LlvmType(), { lV->getType(), rV->getType() }, "__Str" + name);
    return builder.CreateCall(f, { lV, rV }, twine);
}

static llvm::Value* CallStrCat(ExprAST* lhs, ExprAST* rhs)
{
    TRACE();
    llvm::Value* rV = MakeStringFromExpr(rhs, rhs->Type());
    llvm::Value* lV = MakeStringFromExpr(lhs, lhs->Type());

    llvm::Type* strTy = Types::Get<Types::StringDecl>(255)->LlvmType();
    llvm::Type* pty = llvm::PointerType::getUnqual(strTy);

    llvm::FunctionCallee f = GetFunction(MakeVoidType(), { pty, lV->getType(), rV->getType() },
                                         "__StrConcat");

    llvm::Value* dest = CreateTempAlloca(Types::Get<Types::StringDecl>(255));
    builder.CreateCall(f, { dest, lV, rV });
    return dest;
}

llvm::Value* BinaryExprAST::CallStrFunc(const std::string& name)
{
    TRACE();
    return ::CallStrFunc(name, lhs, rhs, Types::Get<Types::IntegerDecl>(), "calltmp");
}

llvm::Value* BinaryExprAST::CallArrFunc(const std::string& name, size_t size)
{
    TRACE();

    llvm::Value* rV = MakeAddressable(rhs);
    llvm::Value* lV = MakeAddressable(lhs);

    assert(rV && lV && "Expect to get values here...");

    // Result is integer.
    llvm::Type* resTy = Types::Get<Types::IntegerDecl>()->LlvmType();
    llvm::Type* pty = llvm::PointerType::getUnqual(Types::Get<Types::CharDecl>()->LlvmType());

    lV = builder.CreateBitCast(lV, pty);
    rV = builder.CreateBitCast(rV, pty);

    llvm::FunctionCallee f = GetFunction(resTy, { pty, pty, resTy }, "__Arr" + name);

    std::vector<llvm::Value*> args = { lV, rV, MakeIntegerConstant(size) };
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
	return Types::Get<Types::BoolDecl>();
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
	llvm::Value*     l = lhs->CodeGen();
	llvm::Value*     setV = MakeAddressable(rhs);
	Types::TypeDecl* type = rhs->Type();
	int              start = type->GetRange()->Start();
	l = builder.CreateZExt(l, Types::Get<Types::IntegerDecl>()->LlvmType(), "zext.l");
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
	llvm::Value*              offset = builder.CreateAnd(l, MakeIntegerConstant(Types::SetDecl::SetMask));
	std::vector<llvm::Value*> ind{ MakeIntegerConstant(0), index };
	llvm::Value*              bitsetAddr = builder.CreateGEP(setV, ind, "valueindex");

	llvm::Value* bitset = builder.CreateLoad(bitsetAddr, "bitsetaddr");
	llvm::Value* bit = builder.CreateLShr(bitset, offset);
	return builder.CreateTrunc(bit, Types::Get<Types::BoolDecl>()->LlvmType());
    }

    if (llvm::isa<SetExprAST>(lhs) || (lhs->Type() && llvm::isa<Types::SetDecl>(lhs->Type())))
    {
	switch (oper.GetToken())
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

	case Token::SymDiff:
	    return CallSetFunc("SymDiff", true);

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

llvm::Value* MakeStrCompare(Token::TokenType oper, llvm::Value* v)
{
    llvm::Constant* zero = MakeIntegerConstant(0);
    switch (oper)
    {
    case Token::Equal:
	return builder.CreateICmpEQ(v, zero, "eq");

    case Token::NotEqual:
	return builder.CreateICmpNE(v, zero, "ne");

    case Token::GreaterOrEqual:
	return builder.CreateICmpSGE(v, zero, "ge");

    case Token::LessOrEqual:
	return builder.CreateICmpSLE(v, zero, "le");

    case Token::GreaterThan:
	return builder.CreateICmpSGT(v, zero, "gt");

    case Token::LessThan:
	return builder.CreateICmpSLT(v, zero, "lt");

    default:
	return ErrorV(0, "Invalid operand for char arrays");
    }
}

static llvm::Value* ShortCtOr(ExprAST* lhs, ExprAST* rhs)
{
    llvm::Value*      l = lhs->CodeGen();
    llvm::BasicBlock* originBlock = builder.GetInsertBlock();
    llvm::Function*   theFunction = originBlock->getParent();
    llvm::BasicBlock* falseBB = llvm::BasicBlock::Create(theContext, "false", theFunction);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(theContext, "merge", theFunction);
    llvm::Value*      bfalse = MakeBooleanConstant(0);
    llvm::Value*      btrue = MakeBooleanConstant(1);

    llvm::Value* condl = builder.CreateICmpEQ(l, bfalse, "condl");
    builder.CreateCondBr(condl, falseBB, mergeBB);
    builder.SetInsertPoint(falseBB);
    llvm::Value* r = rhs->CodeGen();
    llvm::Value* condr = builder.CreateICmpNE(r, bfalse, "condr");

    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(mergeBB);
    llvm::PHINode* phi = builder.CreatePHI(Types::Get<Types::BoolDecl>()->LlvmType(), 2, "phi");
    phi->addIncoming(btrue, originBlock);
    phi->addIncoming(condr, falseBB);
    return phi;
}

static llvm::Value* ShortCtAnd(ExprAST* lhs, ExprAST* rhs)
{
    llvm::Value*      l = lhs->CodeGen();
    llvm::BasicBlock* originBlock = builder.GetInsertBlock();
    llvm::Function*   theFunction = originBlock->getParent();
    llvm::BasicBlock* trueBB = llvm::BasicBlock::Create(theContext, "true", theFunction);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(theContext, "merge", theFunction);
    llvm::Value*      bfalse = MakeBooleanConstant(0);

    llvm::Value* condl = builder.CreateICmpNE(l, bfalse, "condl");
    builder.CreateCondBr(condl, trueBB, mergeBB);
    builder.SetInsertPoint(trueBB);
    llvm::Value* r = rhs->CodeGen();
    llvm::Value* condr = builder.CreateICmpNE(r, bfalse, "condr");

    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(mergeBB);
    llvm::PHINode* phi = builder.CreatePHI(Types::Get<Types::BoolDecl>()->LlvmType(), 2, "phi");
    phi->addIncoming(bfalse, originBlock);
    phi->addIncoming(condr, trueBB);
    return phi;
}

// Calculate base ^ exp.
//    if (e < 0) {
//	if (b == -1 && (e & 1)) return b;
//	if (b > 1) return 0;
//    }
//    int res = 1;
//    while (e > 0) {
//	if (e & 1) { res *= b; e--; }
//	else { res *= b * b; e -= 2; }
//    }
//    return res;
static llvm::Value* PowerInt(llvm::Value* base, llvm::Value* exp, Types::TypeDecl* ty)
{
    llvm::BasicBlock* originBlock = builder.GetInsertBlock();
    llvm::Function*   theFunction = originBlock->getParent();
    llvm::BasicBlock* expNegBB = llvm::BasicBlock::Create(theContext, "expNeg", theFunction);
    llvm::BasicBlock* oddEvenBB = llvm::BasicBlock::Create(theContext, "oddeven", theFunction);
    llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(theContext, "loop", theFunction);
    llvm::BasicBlock* moreBB = llvm::BasicBlock::Create(theContext, "more", theFunction);
    llvm::BasicBlock* moreOddBB = llvm::BasicBlock::Create(theContext, "moreOdd", theFunction);
    llvm::BasicBlock* moreEvenBB = llvm::BasicBlock::Create(theContext, "moreEven", theFunction);
    llvm::BasicBlock* moreMergeBB = llvm::BasicBlock::Create(theContext, "moreMerge", theFunction);
    llvm::BasicBlock* doneBB = llvm::BasicBlock::Create(theContext, "powdone", theFunction);

    llvm::Constant* one = MakeIntegerConstant(1);
    llvm::Constant* zero = MakeIntegerConstant(0);
    llvm::Value*    res = CreateTempAlloca(ty);
    builder.CreateStore(one, res);

    llvm::Value* expneg = builder.CreateICmpSLT(exp, zero, "expneg");
    builder.CreateCondBr(expneg, expNegBB, loopBB);
    // e < 0 case:
    builder.SetInsertPoint(expNegBB);
    llvm::Value* bIsMinus1 = builder.CreateICmpEQ(base, MakeIntegerConstant(-1), "isNeg1");
    builder.CreateCondBr(bIsMinus1, oddEvenBB, doneBB);

    builder.SetInsertPoint(oddEvenBB);
    llvm::Value* isOdd = builder.CreateICmpEQ(builder.CreateAnd(exp, one, "odd"), one, "isOdd");
    // b = -1 && e & 1 => return b;
    builder.CreateCondBr(isOdd, doneBB, loopBB);

    builder.SetInsertPoint(loopBB);
    llvm::PHINode* phiLoop = builder.CreatePHI(ty->LlvmType(), 2, "phi");
    phiLoop->addIncoming(exp, originBlock);
    phiLoop->addIncoming(exp, oddEvenBB);
    llvm::Value* expgt0 = builder.CreateICmpSLE(phiLoop, zero, "expgt0");
    llvm::Value* curVal = builder.CreateLoad(res);
    builder.CreateCondBr(expgt0, doneBB, moreBB);

    builder.SetInsertPoint(moreBB);
    llvm::Value* isOdd1 = builder.CreateICmpEQ(builder.CreateAnd(exp, one, "odd1"), one, "isOdd1");
    builder.CreateCondBr(isOdd1, moreOddBB, moreEvenBB);

    builder.SetInsertPoint(moreOddBB);
    llvm::Value* valOdd = builder.CreateMul(curVal, base);
    builder.CreateBr(moreMergeBB);

    builder.SetInsertPoint(moreEvenBB);
    llvm::Value* valEven = builder.CreateMul(curVal, builder.CreateMul(base, base));
    builder.CreateBr(moreMergeBB);

    builder.SetInsertPoint(moreMergeBB);
    llvm::PHINode* phiMerge = builder.CreatePHI(ty->LlvmType(), 2, "phi");
    phiMerge->addIncoming(valEven, moreEvenBB);
    phiMerge->addIncoming(valOdd, moreOddBB);
    llvm::PHINode* phiMerge2 = builder.CreatePHI(ty->LlvmType(), 2, "phi");
    phiMerge2->addIncoming(MakeIntegerConstant(2), moreEvenBB);
    phiMerge2->addIncoming(one, moreOddBB);
    builder.CreateStore(phiMerge, res);
    phiLoop->addIncoming(builder.CreateSub(phiLoop, phiMerge2), moreMergeBB);
    builder.CreateBr(loopBB);

    builder.SetInsertPoint(doneBB);
    llvm::PHINode* phi = builder.CreatePHI(ty->LlvmType(), 3, "phi");
    // e < 0, b != -1
    phi->addIncoming(zero, expNegBB);
    // e < 0, e is odd, b = -1
    phi->addIncoming(base, oddEvenBB);
    // all other cases.
    phi->addIncoming(curVal, loopBB);
    return phi;
}

static llvm::Value* IntegerBinExpr(llvm::Value* l, llvm::Value* r, const Token& oper, Types::TypeDecl* ty,
                                   bool isUnsigned)
{
    switch (oper.GetToken())
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
    case Token::Pow:
	assert(ty && "Execpted to have a type here");
	return PowerInt(l, r, ty);

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
	break;
    }
    return 0;
}

static llvm::Value* DoubleBinExpr(llvm::Value* l, llvm::Value* r, const Token& oper, Types::TypeDecl* type)
{
    switch (oper.GetToken())
    {
    case Token::Plus:
	return builder.CreateFAdd(l, r, "addtmp");
    case Token::Minus:
	return builder.CreateFSub(l, r, "subtmp");
    case Token::Multiply:
	return builder.CreateFMul(l, r, "multmp");
    case Token::Divide:
	return builder.CreateFDiv(l, r, "divtmp");
    case Token::Power:
    {
	llvm::Type*               ty = type->LlvmType();
	llvm::FunctionCallee      f = GetFunction(ty, { ty, ty }, "llvm.pow.f64");
	std::vector<llvm::Value*> args = { l, r };

	return builder.CreateCall(f, args, "powtmp");
    }

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
	break;
    }
    return 0;
}

llvm::Value* ComplexBinExpr(llvm::Value* l, llvm::Value* r, const Token& oper)
{
    llvm::Constant* zero = MakeIntegerConstant(0);
    llvm::Constant* one = MakeIntegerConstant(1);
    switch (oper.GetToken())
    {
    case Token::Plus:
    {
	llvm::Value* res = CreateTempAlloca(Types::Get<Types::ComplexDecl>());
	llvm::Value* lr = builder.CreateLoad(builder.CreateGEP(l, { zero, zero }, "lr"));
	llvm::Value* rr = builder.CreateLoad(builder.CreateGEP(r, { zero, zero }, "rr"));
	llvm::Value* li = builder.CreateLoad(builder.CreateGEP(l, { zero, one }, "li"));
	llvm::Value* ri = builder.CreateLoad(builder.CreateGEP(r, { zero, one }, "ri"));
	llvm::Value* resr = builder.CreateFAdd(lr, rr, "addr");
	llvm::Value* resi = builder.CreateFAdd(li, ri, "addi");
	builder.CreateStore(resr, builder.CreateGEP(res, { zero, zero }));
	builder.CreateStore(resi, builder.CreateGEP(res, { zero, one }));

	return builder.CreateLoad(res);
    }
    case Token::Minus:
    {
	llvm::Value* res = CreateTempAlloca(Types::Get<Types::ComplexDecl>());
	llvm::Value* lr = builder.CreateLoad(builder.CreateGEP(l, { zero, zero }, "lr"));
	llvm::Value* rr = builder.CreateLoad(builder.CreateGEP(r, { zero, zero }, "rr"));
	llvm::Value* li = builder.CreateLoad(builder.CreateGEP(l, { zero, one }, "li"));
	llvm::Value* ri = builder.CreateLoad(builder.CreateGEP(r, { zero, one }, "ri"));
	llvm::Value* resr = builder.CreateFSub(lr, rr, "subr");
	llvm::Value* resi = builder.CreateFSub(li, ri, "subi");
	builder.CreateStore(resr, builder.CreateGEP(res, { zero, zero }));
	builder.CreateStore(resi, builder.CreateGEP(res, { zero, one }));

	return builder.CreateLoad(res);
    }
    case Token::Multiply:
    {
	// res = l * r = (l.r*r.r - l.i*r.i) + (l.r*r.i + l.i*r.r)i
	llvm::Value* res = CreateTempAlloca(Types::Get<Types::ComplexDecl>());
	llvm::Value* lr = builder.CreateLoad(builder.CreateGEP(l, { zero, zero }, "lr"));
	llvm::Value* rr = builder.CreateLoad(builder.CreateGEP(r, { zero, zero }, "rr"));
	llvm::Value* li = builder.CreateLoad(builder.CreateGEP(l, { zero, one }, "li"));
	llvm::Value* ri = builder.CreateLoad(builder.CreateGEP(r, { zero, one }, "ri"));
	llvm::Value* lrxrr = builder.CreateFMul(lr, rr, "lrxrr");
	llvm::Value* lixri = builder.CreateFMul(li, ri, "lixri");
	llvm::Value* resr = builder.CreateFSub(lrxrr, lixri, "subr");
	llvm::Value* lrxri = builder.CreateFMul(lr, ri, "lrxri");
	llvm::Value* lixrr = builder.CreateFMul(li, rr, "lixrr");
	llvm::Value* resi = builder.CreateFAdd(lrxri, lixrr, "addi");
	builder.CreateStore(resr, builder.CreateGEP(res, { zero, zero }));
	builder.CreateStore(resi, builder.CreateGEP(res, { zero, one }));

	return builder.CreateLoad(res);
    }
    case Token::Divide:
    {
	// res = l * r = (l.r*r.r + l.i*r.i) + (l.i*r.r - l.r*r.i)i / (r.r^2 + r.i^2)
	llvm::Value* res = CreateTempAlloca(Types::Get<Types::ComplexDecl>());
	llvm::Value* lr = builder.CreateLoad(builder.CreateGEP(l, { zero, zero }, "lr"));
	llvm::Value* rr = builder.CreateLoad(builder.CreateGEP(r, { zero, zero }, "rr"));
	llvm::Value* li = builder.CreateLoad(builder.CreateGEP(l, { zero, one }, "li"));
	llvm::Value* ri = builder.CreateLoad(builder.CreateGEP(r, { zero, one }, "ri"));
	llvm::Value* rr2 = builder.CreateFMul(rr, rr, "rr2");
	llvm::Value* ri2 = builder.CreateFMul(ri, ri, "ri2");
	llvm::Value* rr2ri2 = builder.CreateFAdd(rr2, ri2, "rr2ri2");

	llvm::Value* lrxrr = builder.CreateFMul(lr, rr, "lrxrr");
	llvm::Value* lixri = builder.CreateFMul(li, ri, "lixri");
	llvm::Value* resr = builder.CreateFAdd(lrxrr, lixri, "subr");
	resr = builder.CreateFDiv(resr, rr2ri2, "resi");

	llvm::Value* lixrr = builder.CreateFMul(li, rr, "lixrr");
	llvm::Value* lrxri = builder.CreateFMul(lr, ri, "lrxri");
	llvm::Value* resi = builder.CreateFSub(lixrr, lrxri, "subi");
	resi = builder.CreateFDiv(resi, rr2ri2, "resi");

	builder.CreateStore(resr, builder.CreateGEP(res, { zero, zero }));
	builder.CreateStore(resi, builder.CreateGEP(res, { zero, one }));

	return builder.CreateLoad(res);
    }

    case Token::Power:
    {
	llvm::Type*               cty = Types::Get<Types::ComplexDecl>()->LlvmType();
	llvm::Type*               rty = Types::Get<Types::RealDecl>()->LlvmType();
	
	llvm::FunctionCallee      f = GetFunction(cty, { cty, rty }, "__cpow");
	std::vector<llvm::Value*> args = { builder.CreateLoad(l, "lh"), r };

	return builder.CreateCall(f, args, "powtmp");
    }

    case Token::Equal:
    {
	llvm::Value* lr = builder.CreateLoad(builder.CreateGEP(l, { zero, zero }, "lr"));
	llvm::Value* rr = builder.CreateLoad(builder.CreateGEP(r, { zero, zero }, "rr"));
	llvm::Value* re = builder.CreateFCmpOEQ(lr, rr, "eqr");
	llvm::Value* li = builder.CreateLoad(builder.CreateGEP(l, { zero, one }, "li"));
	llvm::Value* ri = builder.CreateLoad(builder.CreateGEP(r, { zero, one }, "ri"));
	llvm::Value* ie = builder.CreateFCmpOEQ(li, ri, "eqi");
	return builder.CreateAnd(re, ie, "res");
    }

    case Token::NotEqual:
    {
	llvm::Value* lr = builder.CreateLoad(builder.CreateGEP(l, { zero, zero }, "lr"));
	llvm::Value* rr = builder.CreateLoad(builder.CreateGEP(r, { zero, zero }, "rr"));
	llvm::Value* re = builder.CreateFCmpONE(lr, rr, "eqr");
	llvm::Value* li = builder.CreateLoad(builder.CreateGEP(l, { zero, one }, "li"));
	llvm::Value* ri = builder.CreateLoad(builder.CreateGEP(r, { zero, one }, "ri"));
	llvm::Value* ie = builder.CreateFCmpONE(li, ri, "eqi");
	return builder.CreateOr(re, ie, "res");
    }

    default:
	break;
    }
    return 0;
}

llvm::Value* BinaryExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    if (verbosity)
    {
	lhs->dump();
	oper.dump();
	rhs->dump();
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

	// We don't need to do this of both sides are char - then it's just a simple comparison
	if (!llvm::isa<Types::CharDecl>(lhs->Type()) || !llvm::isa<Types::CharDecl>(rhs->Type()))
	{
	    return MakeStrCompare(oper.GetToken(), CallStrFunc("Compare"));
	}
    }

    Types::ArrayDecl* ar = llvm::dyn_cast<Types::ArrayDecl>(rhs->Type());
    Types::ArrayDecl* al = llvm::dyn_cast<Types::ArrayDecl>(lhs->Type());
    if (al && ar)
    {
	// Comparison operators are allowed for old style Pascal strings (char arrays)
	// But only if they are the same size.
	if (llvm::isa<Types::CharDecl>(al->SubType()) && llvm::isa<Types::CharDecl>(ar->SubType()))
	{

	    std::vector<Types::RangeDecl*> rr = ar->Ranges();
	    std::vector<Types::RangeDecl*> rl = al->Ranges();

	    if (rr.size() == 1 && rl.size() == 1 && rr[0]->Size() == rl[0]->Size())
	    {
		return MakeStrCompare(oper.GetToken(), CallArrFunc("Compare", rr[0]->Size()));
	    }
	}
    }

    switch (oper.GetToken())
    {
    case Token::And_Then:
	return ShortCtAnd(lhs, rhs);
    case Token::Or_Else:
	return ShortCtOr(lhs, rhs);
    default:
	break;
    }

    if (llvm::isa<Types::ComplexDecl>(lhs->Type()))
    {
	auto lhsa = llvm::dyn_cast<AddressableAST>(lhs);
	llvm::Value* la = lhsa->Address();
	llvm::Value* ra;
	assert(lhsa && "Expect to complex values addressable");
	if (llvm::isa<Types::ComplexDecl>(rhs->Type()))	{
	    auto rhsa = llvm::dyn_cast<AddressableAST>(rhs);
	    assert(rhsa && "Expect to complex values addressable");
	    ra = rhsa->Address();
	}
	else
	{
	    ra = rhs->CodeGen();
	}    
	
	if (auto v = ComplexBinExpr(la, ra, oper))
	{
	    return v;
	}
	return ErrorV(this, "Unknown token: " + oper.ToString());
    }

    llvm::Value* l = lhs->CodeGen();
    llvm::Value* r = rhs->CodeGen();

    assert(l && r && "Should have a value for both sides");

    llvm::Type* lty = l->getType();
    llvm::Type* rty = r->getType();
    (void)lty;
    assert(rty == lty && "Expect same types");

    // Can compare for (in)equality with pointers and integers
    if (rty->isIntegerTy() || rty->isPointerTy())
    {
	switch (oper.GetToken())
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
	bool isUnsigned = rhs->Type()->IsUnsigned();
	if (auto v = IntegerBinExpr(l, r, oper, Type(), isUnsigned))
	{
	    return v;
	}
	return ErrorV(this, "Unknown token: " + oper.ToString());
    }
    if (rty->isDoubleTy())
    {
	if (auto v = DoubleBinExpr(l, r, oper, Type()))
	{
	    return v;
	}
	return ErrorV(this, "Unknown token: " + oper.ToString());
    }

#if !NDEBUG
    l->dump();
    oper.dump();
    r->dump();
    assert(0 && "Should not get here!");
#endif
    return 0;
}

void UnaryExprAST::DoDump() const
{
    std::cerr << "Unary: " << oper.ToString();
    rhs->dump();
}

llvm::Value* UnaryExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    llvm::Value*       r = rhs->CodeGen();
    llvm::Type::TypeID rty = r->getType()->getTypeID();
    if (rty == llvm::Type::IntegerTyID)
    {
	switch (oper.GetToken())
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
	if (oper.GetToken() == Token::Minus)
	{
	    return builder.CreateFNeg(r, "minus");
	}
    }
    return ErrorV(this, "Unknown operation: " + oper.ToString());
}

void CallExprAST::DoDump() const
{
    std::cerr << "call: " << proto->Name() << "(";
    for (auto i : args)
    {
	i->dump();
    }
    std::cerr << ")";
}

static std::vector<llvm::Value*> CreateArgList(const std::vector<ExprAST*>& args,
                                               const std::vector<VarDef>&   vdef)
{
    std::vector<llvm::Value*> argsV;
    unsigned                  index = 0;

    for (auto i : args)
    {
	llvm::Value* v = 0;

	if (ClosureAST* ca = llvm::dyn_cast<ClosureAST>(i))
	{
	    v = ca->CodeGen();
	}
	else
	{
	    AddressableAST* vi = llvm::dyn_cast<AddressableAST>(i);
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
		    v = tc->Address();
		    assert(v && "Expected an address to be generated");
		}
	    }
	    else
	    {
		if (llvm::isa<Types::FuncPtrDecl>(vdef[index].Type()))
		{
		    v = i->CodeGen();
		    assert(v && "Expected CodeGen to work");
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
			    llvm::Value* x = i->CodeGen();
			    assert(x && "Expected CodeGen to work");
			    builder.CreateStore(x, v);
			}
		    }
		    else
		    {
			v = i->CodeGen();
		    }
		}
	    }
	}
	assert(v && "Expect argument here");
	if (llvm::isa<Types::DynArrayDecl>(vdef[index].Type()))
	{
	    auto aty = llvm::dyn_cast<Types::ArrayDecl>(i->Type());
	    assert(aty && "This should be an array declaration");
	    llvm::Constant* zero = MakeIntegerConstant(0);
	    llvm::Constant* one = MakeIntegerConstant(1);
	    llvm::Constant* two = MakeIntegerConstant(2);
	    llvm::Value*    x = CreateTempAlloca(vdef[index].Type());
	    llvm::Value*    arrPtr = builder.CreateGEP(x, { zero, zero });
	    llvm::Value*    arrStart = builder.CreateGEP(v, { zero, zero });
	    builder.CreateStore(arrStart, arrPtr);
	    llvm::Value* low = MakeIntegerConstant(aty->Ranges()[0]->Start());
	    llvm::Value* high = MakeIntegerConstant(aty->Ranges()[0]->End());
	    llvm::Value* lPtr = builder.CreateGEP(x, { zero, one });
	    llvm::Value* hPtr = builder.CreateGEP(x, { zero, two });
	    builder.CreateStore(low, lPtr);
	    builder.CreateStore(high, hPtr);
	    v = x;
	}
	argsV.push_back(v);
	index++;
    }
    return argsV;
}

static llvm::AttributeList CreateAttrList(const std::vector<VarDef>& args)
{
    llvm::AttributeList attrList;
    unsigned            index = 0;
    for (auto i : args)
    {
	if (i.IsClosure())
	{
	    assert(index == 0 && "Expect argument index to be zero");
	    llvm::AttrBuilder ab;
	    ab.addAttribute(llvm::Attribute::Nest);
	    attrList = attrList.addAttributes(theModule->getContext(),
	                                      index + llvm::AttributeList::FirstArgIndex, ab);
	}
	else
	{
	    if (!i.IsRef() && i.Type()->IsCompound())
	    {
		llvm::AttrBuilder ab;
		ab.addByValAttr(i.Type()->LlvmType());
		attrList = attrList.addAttributes(theModule->getContext(),
		                                  index + llvm::AttributeList::FirstArgIndex, ab);
	    }
	}
	index++;
    }
    return attrList;
}

static std::vector<llvm::Type*> CreateArgTypes(const std::vector<VarDef>& args)
{
    std::vector<llvm::Type*> argTypes;
    for (auto i : args)
    {
	assert(i.Type() && "Invalid type for argument");
	llvm::Type* argTy = i.Type()->LlvmType();

	if (i.IsRef() || i.Type()->IsCompound())
	{
	    argTy = llvm::PointerType::getUnqual(argTy);
	}

	argTypes.push_back(argTy);
    }
    return argTypes;
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

    std::vector<llvm::Type*>  argTypes = CreateArgTypes(vdef);
    std::vector<llvm::Value*> argsV = CreateArgList(args, vdef);
    llvm::AttributeList       attrList = CreateAttrList(vdef);

    const char*      res = "";
    Types::TypeDecl* resType = proto->Type();
    if (!llvm::isa<Types::VoidDecl>(resType))
    {
	res = "calltmp";
    }
    llvm::FunctionCallee f = GetFunction(resType, argTypes, calleF);
    llvm::CallInst*      inst = builder.CreateCall(f, argsV, res);
    inst->setAttributes(attrList);

    return inst;
}

void CallExprAST::accept(ASTVisitor& v)
{
    callee->accept(v);
    for (auto i : args)
    {
	i->accept(v);
    }
    v.visit(this);
}

void BlockAST::DoDump() const
{
    std::cerr << "Block: Begin " << std::endl;
    for (auto p : content)
    {
	p->dump();
    }
    std::cerr << "Block End;" << std::endl;
}

void BlockAST::accept(ASTVisitor& v)
{
    for (auto i : content)
    {
	i->accept(v);
    }
}

llvm::Value* BlockAST::CodeGen()
{
    TRACE();

    for (auto e : content)
    {
	[[maybe_unused]] llvm::Value* v = e->CodeGen();
	assert(v && "Expect codegen to work!");
    }
    return NoOpValue();
}

void PrototypeAST::DoDump() const
{
    std::cerr << "Prototype: name: " << name << "(" << std::endl;
    for (auto i : args)
    {
	i.dump();
	std::cerr << std::endl;
    }
    std::cerr << ")";
}

static llvm::Function* CreateFunction(const std::string& name, const std::vector<VarDef>& args,
                                      Types::TypeDecl* resultType)
{
    std::vector<llvm::Type*> argTypes = CreateArgTypes(args);
    llvm::AttributeList      attrList = CreateAttrList(args);

    llvm::Type*          resTy = resultType->LlvmType();
    llvm::FunctionCallee fc = GetFunction(resTy, argTypes, name);
    llvm::Function*      llvmFunc = llvm::dyn_cast<llvm::Function>(fc.getCallee());
    assert(llvmFunc && "Should have found a function here!");
    if (!llvmFunc->empty())
    {
	return ErrorF(nullptr, "redefinition of function: " + name);
    }

    assert(llvmFunc->arg_size() == args.size() && "Expect number of arguments to match");

    auto a = args.begin();
    for (auto& arg : llvmFunc->args())
    {
	arg.setName(a->Name());
	a++;
    }

    llvmFunc->setAttributes(attrList);
    // TODO: Allow this to be disabled.
    llvmFunc->addFnAttr("no-frame-pointer-elim", "true");
    return llvmFunc;
}

llvm::Function* PrototypeAST::Create(const std::string& namePrefix)
{
    TRACE();

    assert(namePrefix != "" && "Prefix should never be empty");
    if (llvmFunc)
    {
	return llvmFunc;
    }

    std::string actualName;
    // Don't mangle our 'main' functions name, as we call that from C
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

    llvmFunc = CreateFunction(actualName, args, type);
    return llvmFunc;
}

void PrototypeAST::CreateArgumentAlloca()
{
    TRACE();

    unsigned                     offset = 0;
    llvm::Function::arg_iterator ai = llvmFunc->arg_begin();
    if (Types::TypeDecl* closureType = Function()->ClosureType())
    {
	assert(closureType == args[0].Type() && "Expect type to match here");
	// Skip over the closure argument in the loop below.
	offset = 1;

	Types::RecordDecl* rd = llvm::dyn_cast<Types::RecordDecl>(closureType);
	assert(rd && "Expected a record for closure type!");
	std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), 0 };
	for (int i = 0; i < rd->FieldCount(); i++)
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
    for (unsigned idx = offset; idx < args.size(); idx++, ai++)
    {
	llvm::Value* a;
	if (args[idx].IsRef() || args[idx].Type()->IsCompound())
	{
	    a = &*ai;
	    if (auto dty = llvm::dyn_cast<Types::DynArrayDecl>(args[idx].Type()))
	    {
		Types::DynRangeDecl* dr = dty->Range();
		llvm::Value* low = builder.CreateGEP(&*ai, { MakeIntegerConstant(0), MakeIntegerConstant(1) },
		                                     dr->LowName());

		llvm::Value* high = builder.CreateGEP(
		    &*ai, { MakeIntegerConstant(0), MakeIntegerConstant(2) }, dr->HighName());

		if (!variables.Add(dr->LowName(), low))
		{
		    ErrorF(this, "Duplicate Variable name");
		}
		if (!variables.Add(dr->HighName(), high))
		{
		    ErrorF(this, "Duplicate Variable name");
		}
	    }
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
	    llvm::DIType*  debugType = args[idx].Type()->DebugType(di.builder);
	    if (!debugType)
	    {
		// Ugly hack until we have all debug types.
		std::cerr << "Skipping unknown debug type element." << std::endl;
		args[idx].Type()->dump();
	    }
	    else
	    {
		// Create a debug descriptor for the variable.
		int                    lineNum = function->Loc().LineNumber();
		llvm::DIFile*          unit = sp->getFile();
		llvm::DILocalVariable* dv = di.builder->createParameterVariable(
		    sp, args[idx].Name(), idx + 1, unit, lineNum, debugType, true);
		llvm::DILocation* diloc = llvm::DILocation::get(sp->getContext(), lineNum, 0, sp);
		di.builder->insertDeclare(a, dv, di.builder->createExpression(), llvm::DebugLoc(diloc),
		                          ::builder.GetInsertBlock());
	    }
	}
    }
    if (!llvm::isa<Types::VoidDecl>(type))
    {
	std::string       shortname = ShortName(name);
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
    for (auto v : extra)
    {
	newArgs.push_back(v);
    }
    for (auto v : args)
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
    for (size_t i = 0; i < args.size(); i++)
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
    // Don't allow comparison with ourselves
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
    for (size_t i = 0; i < rhs->args.size(); i++)
    {
	if (*args[i + 1].Type() != *rhs->args[i].Type())
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
    for (auto vv : varDecls)
    {
	vv->SetFunction(this);
    }
}

void FunctionAST::DoDump() const
{
    std::cerr << "Function: " << std::endl;
    proto->dump();
    std::cerr << "Function body:" << std::endl;
    body->dump();
}

void FunctionAST::accept(ASTVisitor& v)
{
    v.visit(this);
    for (auto d : varDecls)
    {
	d->accept(v);
    }
    if (body)
    {
	body->accept(v);
    }
    for (auto i : subFunctions)
    {
	i->accept(v);
    }
}

static llvm::DISubroutineType* CreateFunctionType(DebugInfo& di, PrototypeAST* proto)
{
    std::vector<llvm::Metadata*> eltTys;

    eltTys.push_back(proto->Type()->DebugType(di.builder));

    for (auto a : proto->Args())
    {
	eltTys.push_back(a.Type()->DebugType(di.builder));
    }
    return di.builder->createSubroutineType(di.builder->getOrCreateTypeArray(eltTys));
}

llvm::Function* FunctionAST::CodeGen(const std::string& namePrefix)
{
    TRACE();
    VarStackWrapper w(variables);
    LabelWrapper    l(labels);
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
	DebugInfo&              di = GetDebugInfo();
	Location                loc = body->Loc();
	llvm::DIFile*           unit = di.builder->createFile(di.cu->getFilename(), di.cu->getDirectory());
	llvm::DIScope*          fnContext = unit;
	llvm::DISubroutineType* st = CreateFunctionType(di, proto);
	std::string             name = proto->Name();
	int                     lineNum = loc.LineNumber();
	llvm::DISubprogram* sp = di.builder->createFunction(fnContext, name, "", unit, lineNum, st, lineNum);

	theFunction->setSubprogram(sp);
	di.lexicalBlocks.push_back(sp);
	di.EmitLocation(Location());
    }
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(theContext, "entry", theFunction);
    builder.SetInsertPoint(bb);

    proto->CreateArgumentAlloca();
    for (auto d : varDecls)
    {
	d->CodeGen();
    }

    llvm::BasicBlock::iterator ip = builder.GetInsertPoint();

    std::string newPrefix = namePrefix + "." + proto->Name();
    for (auto fn : subFunctions)
    {
	fn->CodeGen(newPrefix);
    }

#if !NDEBUG
    if (verbosity > 1)
    {
	variables.dump();
    }
#endif

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
    if (llvm::isa<Types::VoidDecl>(proto->Type()))
    {
	builder.CreateRetVoid();
    }
    else
    {
	std::string  shortname = ShortName(proto->Name());
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
	for (auto u : usedVariables)
	{
	    Types::TypeDecl* ty = new Types::PointerDecl(u.Type());
	    vf.push_back(new Types::FieldDecl(u.Name(), ty, false));
	}
	closureType = new Types::RecordDecl(vf, 0);
    }
    return closureType;
}

void StringExprAST::DoDump() const
{
    std::cerr << "String: '" << val << "'" << std::endl;
}

llvm::Value* StringExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    return builder.CreateGlobalStringPtr(val, "_string", 0, theModule);
}

llvm::Value* StringExprAST::Address()
{
    TRACE();

    BasicDebugInfo(this);

    return builder.CreateGlobalStringPtr(val, "_string");
}

void AssignExprAST::DoDump() const
{
    std::cerr << "Assign: " << std::endl;
    lhs->dump();
    std::cerr << ":=";
    rhs->dump();
}

llvm::Value* AssignExprAST::AssignStr()
{
    TRACE();
    auto lhsv = llvm::dyn_cast<AddressableAST>(lhs);
    assert(lhsv && "Expect variable in lhs");
    assert(llvm::isa<Types::StringDecl>(lhsv->Type()) && "Expect string type in lhsv->Type()");

    if (StringExprAST* srhs = llvm::dyn_cast<StringExprAST>(rhs))
    {
	llvm::Value* dest = lhsv->Address();
	return TempStringFromStringExpr(dest, srhs);
    }

    assert(llvm::isa<Types::StringDecl>(rhs->Type()));
    return CallStrFunc("Assign", lhs, rhs, Types::Get<Types::VoidDecl>(), "");
}

llvm::Value* AssignExprAST::AssignSet()
{
    if (llvm::Value* v = rhs->CodeGen())
    {
	auto         lhsv = llvm::dyn_cast<AddressableAST>(lhs);
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

    AddressableAST* lhsv = llvm::dyn_cast<AddressableAST>(lhs);
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

    if (llvm::isa<StringExprAST>(rhs) && Types::IsCharArray(lhs->Type()))
    {
	StringExprAST* str = llvm::dyn_cast<StringExprAST>(rhs);
	assert(rhs && "Expected string to convert correctly");
	llvm::Value*              dest = lhsv->Address();
	std::vector<llvm::Value*> ind{ MakeIntegerConstant(0), MakeIntegerConstant(0) };
	llvm::Value*              dest1 = builder.CreateGEP(dest, ind, "str_0");
	llvm::Value*              v = rhs->CodeGen();
	llvm::Align               dest_align{ std::max(AlignOfType(dest1->getType()), MIN_ALIGN) };
	llvm::Align               src_align{ std::max(AlignOfType(v->getType()), MIN_ALIGN) };
	return builder.CreateMemCpy(dest1, dest_align, v, src_align, str->Str().size());
    }

    llvm::Value* dest = lhsv->Address();
    if (!dest)
    {
	return ErrorV(this, "Unknown variable name '" + lhsv->Name() + "'");
    }

    // If rhs is a simple variable, and "large", then use memcpy on it!
    if (auto rhsv = llvm::dyn_cast<AddressableAST>(rhs))
    {
	if (rhsv->Type() == lhsv->Type())
	{
	    size_t size = rhsv->Type()->Size();
	    if (!disableMemcpyOpt && size >= MEMCPY_THRESHOLD)
	    {
		llvm::Value* src = rhsv->Address();
		llvm::Align  dest_align{ std::max(AlignOfType(dest->getType()), MIN_ALIGN) };
		llvm::Align  src_align{ std::max(AlignOfType(src->getType()), MIN_ALIGN) };
		return builder.CreateMemCpy(dest, dest_align, src, src_align, size);
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

void IfExprAST::DoDump() const
{
    std::cerr << "if: " << std::endl;
    cond->dump();
    std::cerr << "then: ";
    if (then)
    {
	then->dump();
    }
    if (other)
    {
	std::cerr << " else::";
	other->dump();
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
    assert(llvm::isa<Types::BoolDecl>(cond->Type()) && "Only boolean expressions allowed in if-statement");

    llvm::Value* condV = cond->CodeGen();
    if (!condV)
    {
	return 0;
    }

    llvm::Function*   theFunction = builder.GetInsertBlock()->getParent();
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

	llvm::Value* elseV = other->CodeGen();
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

void ForExprAST::DoDump() const
{
    std::cerr << "for: " << std::endl;
    start->dump();
    if (stepDown)
    {
	std::cerr << " downto ";
    }
    else
    {
	std::cerr << " to ";
    }
    end->dump();
    std::cerr << " do ";
    body->dump();
}

void ForExprAST::accept(ASTVisitor& v)
{
    start->accept(v);
    if (end)
    {
	end->accept(v);
    }
    body->accept(v);
    v.visit(this);
}

llvm::Value* ForExprAST::ForInGen()
{
    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();

    llvm::Value* words = 0;
    llvm::Value* setV = MakeAddressable(start);
    int          rangeStart = 0;
    if (auto sd = llvm::dyn_cast<Types::SetDecl>(start->Type()))
    {
	words = MakeIntegerConstant(sd->SetWords());
	rangeStart = sd->GetRange()->Start();
    }
    else
    {
	assert(0 && "Hmnm, shouldn't we have a SetDecl here?");
	return 0;
    }

    llvm::Value* one = MakeIntegerConstant(1);
    llvm::Value* zero = MakeIntegerConstant(0);
    llvm::Value* shift = MakeIntegerConstant(Types::SetDecl::SetPow2Bits);
    llvm::Value* var = variable->Address();
    assert(var && "Expected variable here");

    llvm::BasicBlock* beforeBB = llvm::BasicBlock::Create(theContext, "before", theFunction);
    llvm::BasicBlock* preOuterLoopBB = llvm::BasicBlock::Create(theContext, "outerloop", theFunction);
    llvm::BasicBlock* preLoopBB = llvm::BasicBlock::Create(theContext, "preloop", theFunction);
    llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(theContext, "loop", theFunction);
    llvm::BasicBlock* nextLoopBB = llvm::BasicBlock::Create(theContext, "nextLoop", theFunction);
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(theContext, "afterloop", theFunction);

    builder.CreateBr(beforeBB);
    builder.SetInsertPoint(beforeBB);

    llvm::Value* index = zero;
    builder.CreateBr(preOuterLoopBB);

    builder.SetInsertPoint(preOuterLoopBB);

    // before:
    // index = 0;
    // preOuterLoop:
    // while(index < words)
    //    preLoop:
    //    while ((bit = find_first_set(set[index])) != 32)
    //      loopBB:
    //      loopvar = bit + setbits * index + rangestart;
    //      loop-body;
    //    after:

    llvm::PHINode* idxPhi = builder.CreatePHI(index->getType(), 2, "idxPhi");
    idxPhi->addIncoming(index, beforeBB);
    llvm::Value* bitsetAddr = builder.CreateGEP(setV, { zero, idxPhi }, "valueindex");
    llvm::Value* bitset = builder.CreateLoad(bitsetAddr);

    llvm::Value* cmp = builder.CreateICmpSLT(idxPhi, words);
    builder.CreateCondBr(cmp, preLoopBB, afterBB);

    builder.SetInsertPoint(preLoopBB);
    llvm::Type*          ty = Types::Get<Types::IntegerDecl>()->LlvmType();
    llvm::FunctionCallee cttz = GetFunction(ty, { ty, Types::Get<Types::BoolDecl>()->LlvmType() },
                                            "llvm.cttz.i32");
    llvm::Value*         isZero = builder.CreateICmpEQ(bitset, zero);
    builder.CreateCondBr(isZero, nextLoopBB, loopBB);

    builder.SetInsertPoint(loopBB);
    llvm::PHINode* phi = builder.CreatePHI(bitset->getType(), 2, "phi");
    phi->addIncoming(bitset, preLoopBB);

    llvm::Value* pos = builder.CreateCall(cttz, { phi, MakeBooleanConstant(true) }, "pos");

    llvm::Value* sumVar = builder.CreateAdd(builder.CreateAdd(pos, builder.CreateShl(idxPhi, shift)),
                                            MakeIntegerConstant(rangeStart));
    llvm::Value* sumT = builder.CreateTrunc(sumVar, variable->Type()->LlvmType());
    builder.CreateStore(sumT, var);

    if (!body->CodeGen())
    {
	return 0;
    }
    bitset = builder.CreateAnd(phi, builder.CreateNot(builder.CreateShl(one, pos)));
    isZero = builder.CreateICmpEQ(bitset, zero);
    phi->addIncoming(bitset, loopBB);
    builder.CreateCondBr(isZero, nextLoopBB, loopBB);

    builder.SetInsertPoint(nextLoopBB);
    index = builder.CreateAdd(idxPhi, one);
    idxPhi->addIncoming(index, nextLoopBB);
    builder.CreateBr(preOuterLoopBB);

    builder.SetInsertPoint(afterBB);
    BasicDebugInfo(this);
    return afterBB;
}

llvm::Value* ForExprAST::CodeGen()
{
    TRACE();
    BasicDebugInfo(this);

    // for x in set has no end.
    if (!end)
    {
	return ForInGen();
    }

    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();
    llvm::Value*    var = variable->Address();
    assert(var && "Expected variable here");

    llvm::Value* startV = start->CodeGen();
    assert(startV && "Expected start to generate code");
    llvm::Value* endV = end->CodeGen();
    assert(endV && "Expected end to generate code");
    llvm::Value* stepVal = MakeConstant((stepDown) ? -1 : 1, start->Type());
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
    builder.CreateCondBr(endCond, loopBB, afterBB);

    BasicDebugInfo(this);

    builder.SetInsertPoint(afterBB);

    return afterBB;
}

void WhileExprAST::DoDump() const
{
    std::cerr << "While: ";
    cond->dump();
    std::cerr << " Do: ";
    body->dump();
}

llvm::Value* WhileExprAST::CodeGen()
{
    TRACE();
    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();

    // We will need a "prebody" before the loop, a "body" and an "after" basic block
    llvm::BasicBlock* preBodyBB = llvm::BasicBlock::Create(theContext, "prebody", theFunction);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(theContext, "body", theFunction);
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(theContext, "after", theFunction);

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

void RepeatExprAST::DoDump() const
{
    std::cerr << "Repeat: ";
    body->dump();
    std::cerr << " until: ";
    cond->dump();
}

llvm::Value* RepeatExprAST::CodeGen()
{
    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();

    // We will need a "body" and an "after" basic block
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(theContext, "body", theFunction);
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(theContext, "after", theFunction);

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

void WriteAST::DoDump() const
{
    if (isWriteln)
    {
	std::cerr << "Writeln(";
    }
    else
    {
	std::cerr << "Write(";
    }
    bool first = true;
    for (auto a : args)
    {
	if (!first)
	{
	    std::cerr << ", ";
	}
	first = false;
	a.expr->dump();
	if (a.width)
	{
	    std::cerr << ":";
	    a.width->dump();
	}
	if (a.precision)
	{
	    std::cerr << ":";
	    a.precision->dump();
	}
    }
    std::cerr << ")";
}

void WriteAST::accept(ASTVisitor& v)
{
    file->accept(v);
    for (auto a : args)
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

static llvm::FunctionCallee CreateWriteFunc(Types::TypeDecl* ty, llvm::Type* fty)
{
    std::string              suffix;
    llvm::Type*              intTy = Types::Get<Types::IntegerDecl>()->LlvmType();
    std::vector<llvm::Type*> argTypes{ fty };
    if (auto rd = llvm::dyn_cast<Types::RangeDecl>(ty))
    {
	ty = rd->SubType();
    }

    if (llvm::isa<Types::CharDecl>(ty))
    {
	argTypes.push_back(ty->LlvmType());
	argTypes.push_back(intTy);
	suffix = "char";
    }
    else if (llvm::isa<Types::BoolDecl>(ty))
    {
	argTypes.push_back(ty->LlvmType());
	argTypes.push_back(intTy);
	suffix = "bool";
    }
    else if (llvm::isa<Types::IntegerDecl>(ty) || llvm::isa<Types::Int64Decl>(ty))
    {
	argTypes.push_back(ty->LlvmType());
	argTypes.push_back(intTy);
	suffix = "int" + std::to_string(ty->Bits());
    }
    else if (llvm::isa<Types::RealDecl>(ty))
    {
	argTypes.push_back(ty->LlvmType());
	argTypes.push_back(intTy);
	argTypes.push_back(intTy);
	suffix = "real";
    }
    else if (llvm::isa<Types::StringDecl>(ty))
    {
	llvm::Type* pty = llvm::PointerType::getUnqual(Types::Get<Types::CharDecl>()->LlvmType());
	argTypes.push_back(pty);
	argTypes.push_back(intTy);
	suffix = "str";
    }
    else if (IsCharArray(ty))
    {
	llvm::Type* pty = llvm::PointerType::getUnqual(Types::Get<Types::CharDecl>()->LlvmType());
	argTypes.push_back(pty);
	argTypes.push_back(intTy);
	argTypes.push_back(intTy);
	suffix = "chars";
    }
    else
    {
	ty->dump();
	assert(0);
	return ErrorF(0, "Invalid type argument for write");
    }
    return GetFunction(MakeVoidType(), argTypes, "__write_" + suffix);
}

static llvm::FunctionCallee CreateWriteBinFunc(llvm::Type* ty, llvm::Type* fty)
{
    assert(ty && "Type should not be NULL!");
    assert(ty->isPointerTy() && "Expected pointer argument");
    llvm::Type* voidPtrTy = Types::GetVoidPtrType();

    return GetFunction(MakeVoidType(), { fty, voidPtrTy }, "__write_bin");
}

llvm::Value* WriteAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    llvm::Value* f = file->Address();
    llvm::Value* v = 0;
    bool         isText = llvm::isa<Types::TextDecl>(file->Type());
    if (isText && args.empty() && !isWriteln)
    {
	return NoOpValue();
    }
    for (auto arg : args)
    {
	std::vector<llvm::Value*> argsV;
	llvm::FunctionCallee      fn;
	argsV.push_back(f);
	if (isText)
	{
	    Types::TypeDecl* type = arg.expr->Type();
	    assert(type && "Expected type here");
	    if (!(fn = CreateWriteFunc(type, f->getType())))
	    {
		return 0;
	    }
	    if (llvm::isa<Types::StringDecl>(type))
	    {
		if (AddressableAST* a = llvm::dyn_cast<AddressableAST>(arg.expr))
		{
		    v = a->Address();
		}
		else
		{
		    v = MakeAddressable(arg.expr);
		}

		std::vector<llvm::Value*> ind{ MakeIntegerConstant(0), MakeIntegerConstant(0) };
		v = builder.CreateGEP(v, ind, "str_addr");
	    }
	    else if (Types::IsCharArray(type))
	    {
		if (llvm::isa<StringExprAST>(arg.expr) || llvm::isa<BuiltinExprAST>(arg.expr))
		{
		    v = arg.expr->CodeGen();
		}
		else
		{
		    AddressableAST* a = llvm::dyn_cast<AddressableAST>(arg.expr);
		    assert(a && "Expected addressable value");
		    v = a->Address();
		    std::vector<llvm::Value*> ind{ MakeIntegerConstant(0), MakeIntegerConstant(0) };
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
	    if (llvm::isa<Types::RealDecl>(type))
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
	llvm::FunctionCallee fc = GetFunction(MakeVoidType(), { f->getType() }, "__write_nl");
	v = builder.CreateCall(fc, f, "");
    }
    return v;
}

void ReadAST::DoDump() const
{
    if (isReadln)
    {
	std::cerr << "Readln(";
    }
    else
    {
	std::cerr << "Read(";
    }
    bool first = true;
    for (auto a : args)
    {
	if (!first)
	{
	    std::cerr << ", ";
	}
	first = false;
	a->dump();
    }
    std::cerr << ")";
}

void ReadAST::accept(ASTVisitor& v)
{
    file->accept(v);
    for (auto a : args)
    {
	a->accept(v);
    }
    v.visit(this);
}

static llvm::FunctionCallee CreateReadFunc(Types::TypeDecl* ty, llvm::Type* fty)
{
    std::string              suffix;
    llvm::Type*              lty = llvm::PointerType::getUnqual(ty->LlvmType());
    std::vector<llvm::Type*> argTypes = { fty, lty };
    if (auto rd = llvm::dyn_cast<Types::RangeDecl>(ty))
    {
	ty = rd->SubType();
    }
    if (llvm::isa<Types::CharDecl>(ty))
    {
	suffix = "chr";
    }
    else if (llvm::isa<Types::IntegerDecl>(ty) || llvm::isa<Types::Int64Decl>(ty))
    {
	suffix = "int" + std::to_string(ty->Bits());
    }
    else if (llvm::isa<Types::RealDecl>(ty))
    {
	suffix = "real";
    }
    else if (llvm::isa<Types::StringDecl>(ty))
    {
	suffix = "str";
    }
    else if (IsCharArray(ty))
    {
	suffix = "chars";
    }
    else
    {
	return ErrorF(0, "Invalid type argument for read");
    }
    return GetFunction(MakeVoidType(), argTypes, "__read_" + suffix);
}

static llvm::FunctionCallee CreateReadBinFunc(Types::TypeDecl* ty, llvm::Type* fty)
{
    return GetFunction(MakeVoidType(), { fty, Types::GetVoidPtrType() }, "__read_bin");
}

llvm::Value* ReadAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    llvm::Value* f = file->Address();
    llvm::Value* v;
    bool         isText = llvm::isa<Types::TextDecl>(file->Type());
    llvm::Type*  fTy = f->getType();
    if (isText && args.empty() && !isReadln)
    {
	return NoOpValue();
    }
    for (auto arg : args)
    {
	std::vector<llvm::Value*> argsV = { f };
	AddressableAST*           vexpr = llvm::dyn_cast<AddressableAST>(arg);
	assert(vexpr && "Argument for read/readln should be a variable");

	Types::TypeDecl* ty = vexpr->Type();
	v = vexpr->Address();
	assert(v && "Could not evaluate address of expression for read");

	if (!isText)
	{
	    v = builder.CreateBitCast(v, Types::GetVoidPtrType());
	}
	argsV.push_back(v);
	llvm::FunctionCallee fn;
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
	llvm::FunctionCallee fn = GetFunction(MakeVoidType(), { fTy }, "__read_nl");
	v = builder.CreateCall(fn, f, "");
    }
    return v;
}

void VarDeclAST::DoDump() const
{
    std::cerr << "Var ";
    for (auto v : vars)
    {
	v.dump();
	std::cerr << std::endl;
    }
}

llvm::Value* VarDeclAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    llvm::Value* v = 0;
    for (auto var : vars)
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
	    llvm::Constant*   init;
	    llvm::Constant*   nullValue = llvm::Constant::getNullValue(ty);
	    Types::ClassDecl* cd = llvm::dyn_cast<Types::ClassDecl>(var.Type());
	    if (cd && cd->VTableType(true))
	    {
		llvm::GlobalVariable*        gv = theModule->getGlobalVariable("vtable_" + cd->Name(), true);
		llvm::StructType*            sty = llvm::dyn_cast<llvm::StructType>(ty);
		std::vector<llvm::Constant*> vtable(sty->getNumElements());
		vtable[0] = gv;
		unsigned i = 1;
		while (llvm::Constant* c = nullValue->getAggregateElement(i))
		{
		    vtable[i] = c;
		    i++;
		}
		init = llvm::ConstantStruct::get(sty, vtable);
	    }
	    else if (ExprAST* iv = var.Init())
	    {
		init = llvm::dyn_cast<llvm::Constant>(iv->CodeGen());
	    }
	    else
	    {
		init = nullValue;
	    }
	    llvm::GlobalValue::LinkageTypes linkage = (var.IsExternal() ? llvm::GlobalValue::ExternalLinkage
	                                                                : llvm::Function::InternalLinkage);

	    llvm::GlobalVariable*  gv = new llvm::GlobalVariable(*theModule, ty, false, linkage, init,
                                                                var.Name());
	    const llvm::DataLayout dl(theModule);
	    size_t                 al = dl.getPrefTypeAlignment(ty);
	    al = std::max(size_t(4), al);
	    gv->setAlignment(llvm::Align(al));
	    v = gv;
	    if (debugInfo)
	    {
		DebugInfo&    di = GetDebugInfo();
		llvm::DIType* debugType = var.Type()->DebugType(di.builder);
		if (!debugType)
		{
		    // Ugly hack until we have all debug types.
		    std::cerr << "Skipping unknown debug type element." << std::endl;
		    var.Type()->dump();
		}
		else
		{
		    int            lineNum = this->Loc().LineNumber();
		    llvm::DIScope* scope = di.cu;
		    llvm::DIFile*  unit = scope->getFile();

		    di.builder->createGlobalVariableExpression(scope, var.Name(), var.Name(), unit, lineNum,
		                                               debugType, gv->hasInternalLinkage());
		}
	    }
	}
	else
	{
	    v = CreateAlloca(func->Proto()->LlvmFunction(), var);
	    Types::ClassDecl* cd = llvm::dyn_cast<Types::ClassDecl>(var.Type());
	    if (cd && cd->VTableType(true))
	    {
		llvm::GlobalVariable*     gv = theModule->getGlobalVariable("vtable_" + cd->Name(), true);
		std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), MakeIntegerConstant(0) };
		llvm::Value*              dest = builder.CreateGEP(v, ind, "vtable");
		builder.CreateStore(gv, dest);
	    }
	    else if (ExprAST* iv = var.Init())
	    {
		llvm::Value* init = iv->CodeGen();
		builder.CreateStore(init, v);
	    }
	    if (debugInfo)
	    {
		DebugInfo& di = GetDebugInfo();
		assert(!di.lexicalBlocks.empty() && "Should not have empty lexicalblocks here!");
		llvm::DIScope* sp = di.lexicalBlocks.back();
		llvm::DIType*  debugType = var.Type()->DebugType(di.builder);
		if (!debugType)
		{
		    // Ugly hack until we have all debug types.
		    std::cerr << "Skipping unknown debug type element." << std::endl;
		    var.Type()->dump();
		}
		else
		{
		    // Create a debug descriptor for the variable.
		    int                    lineNum = this->Loc().LineNumber();
		    llvm::DIFile*          unit = sp->getFile();
		    llvm::DILocalVariable* dv = di.builder->createAutoVariable(sp, var.Name(), unit, lineNum,
		                                                               debugType, true);
		    llvm::DILocation*      diloc = llvm::DILocation::get(sp->getContext(), lineNum, 0, sp);
		    di.builder->insertDeclare(v, dv, di.builder->createExpression(), llvm::DebugLoc(diloc),
		                              ::builder.GetInsertBlock());
		}
	    }
	}
	if (!variables.Add(var.Name(), v))
	{
	    return ErrorV(this, "Duplicate name " + var.Name() + "!");
	}
    }
    return v;
}

void LabelExprAST::DoDump() const
{
    bool first = true;
    for (auto l : labelValues)
    {
	if (!first)
	{
	    std::cerr << ", ";
	}
	std::cerr << l.first;
	if (l.first != l.second)
	{
	    std::cerr << ".." << l.second;
	}
	first = false;
    }
    std::cerr << ": ";
    stmt->dump();
}

llvm::Value* LabelExprAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    assert(!stmt && "Expected no statement for 'goto' label expression");
    assert(labelValues.size() == 1 && labelValues[0].first == labelValues[0].second);
    llvm::BasicBlock* labelBB = CreateGotoTarget(labelValues[0].first);
    // Make LLVM-IR valid by jumping to the neew block!
    llvm::Value* v = builder.CreateBr(labelBB);
    builder.SetInsertPoint(labelBB);

    return v;
}

llvm::Value* LabelExprAST::CodeGen(llvm::BasicBlock* caseBB, llvm::BasicBlock* afterBB)
{
    TRACE();

    BasicDebugInfo(this);

    assert(stmt && "Expected a statement for 'case' label expression");
    builder.SetInsertPoint(caseBB);
    stmt->CodeGen();
    builder.CreateBr(afterBB);

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

void CaseExprAST::DoDump() const
{
    std::cerr << "Case ";
    expr->dump();
    std::cerr << " of " << std::endl;
    for (auto l : labels)
    {
	l->dump();
    }
    if (otherwise)
    {
	std::cerr << "otherwise: ";
	otherwise->dump();
    }
}

void CaseExprAST::accept(ASTVisitor& v)
{
    TRACE();
    expr->accept(v);
    for (auto l : labels)
    {
	l->accept(v);
    }
    if (otherwise)
    {
	otherwise->accept(v);
    }
    v.visit(this);
}

static uint64_t Distance(std::pair<int, int> p)
{
    assert(p.first <= p.second && "Expect ordered pair");
    if (p.first > 0)
    {
	return p.second - p.first;
    }
    if (p.first < 0 && p.second > 0)
    {
	return (uint64_t)-p.first + p.second;
    }
    // Both are negative.
    return -p.first - -p.second;
}

llvm::Value* CaseExprAST::CodeGen()
{
    TRACE();

    const int MaxRangeInSwitch = 32;

    BasicDebugInfo(this);

    llvm::Value* v = expr->CodeGen();
    llvm::Type*  ty = v->getType();

    llvm::BasicBlock* bb = builder.GetInsertBlock();

    llvm::Function*   theFunction = bb->getParent();
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(theContext, "after", theFunction);
    llvm::BasicBlock* defaultBB = afterBB;
    if (otherwise)
    {
	defaultBB = llvm::BasicBlock::Create(theContext, "default", theFunction);
    }
    std::vector<std::pair<LabelExprAST*, llvm::BasicBlock*>> labbb;
    for (auto ll : labels)
    {
	llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(theContext, "case", theFunction);
	ll->CodeGen(caseBB, afterBB);
	labbb.push_back({ ll, caseBB });
    }

    builder.SetInsertPoint(bb);
    for (auto ll : labbb)
    {
	for (auto val : ll.first->LabelValues())
	{
	    if (Distance(val) > MaxRangeInSwitch)
	    {
		llvm::BasicBlock* next = llvm::BasicBlock::Create(theContext, "next", theFunction);
		llvm::BasicBlock* maybe = llvm::BasicBlock::Create(theContext, "maybe", theFunction);
		llvm::Value*      lt = builder.CreateICmpSLT(v, MakeIntegerConstant(val.first), "lt");
		builder.CreateCondBr(lt, next, maybe);
		builder.SetInsertPoint(maybe);
		llvm::Value* gt = builder.CreateICmpSGT(v, MakeIntegerConstant(val.second), "gt");
		builder.CreateCondBr(gt, next, ll.second);
		builder.SetInsertPoint(next);
	    }
	}
    }

    llvm::SwitchInst* sw = builder.CreateSwitch(v, defaultBB, labels.size());

    for (auto ll : labbb)
    {
	for (auto val : ll.first->LabelValues())
	{
	    if (Distance(val) <= MaxRangeInSwitch)
	    {
		for (int i = val.first; i <= val.second; i++)
		{
		    llvm::IntegerType* intTy = llvm::dyn_cast<llvm::IntegerType>(ty);
		    sw->addCase(llvm::ConstantInt::get(intTy, i), ll.second);
		}
	    }
	}
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

void RangeExprAST::DoDump() const
{
    std::cerr << "Range:";
    low->dump();
    std::cerr << "..";
    high->dump();
}

bool RangeExprAST::IsConstant()
{
    return llvm::isa<IntegerExprAST>(low) && llvm::isa<IntegerExprAST>(high);
}

void SetExprAST::DoDump() const
{
    std::cerr << "Set :[";
    bool first = true;
    for (auto v : values)
    {
	if (!first)
	{
	    std::cerr << ", ";
	}
	first = false;
	v->dump();
    }
    std::cerr << "]";
}

llvm::Constant* SetExprAST::MakeConstantSetArray()
{
    llvm::Type* ty = type->LlvmType();

    Types::SetDecl::ElemType elems[Types::SetDecl::MaxSetWords] = {};
    for (auto v : values)
    {
	if (RangeExprAST* r = llvm::dyn_cast<RangeExprAST>(v))
	{
	    IntegerExprAST* le = llvm::dyn_cast<IntegerExprAST>(r->LowExpr());
	    IntegerExprAST* he = llvm::dyn_cast<IntegerExprAST>(r->HighExpr());
	    int             start = type->GetRange()->Start();
	    int             low = le->Int() - start;
	    int             high = he->Int() - start;
	    low = std::max(0, low);
	    high = std::min((int)type->GetRange()->Size(), high);
	    for (int i = low; i <= high; i++)
	    {
		elems[i >> Types::SetDecl::SetPow2Bits] |= (1 << (i & Types::SetDecl::SetMask));
	    }
	}
	else
	{
	    IntegerExprAST* e = llvm::dyn_cast<IntegerExprAST>(v);
	    int             start = type->GetRange()->Start();
	    unsigned        i = e->Int() - start;
	    if (i < (unsigned)type->GetRange()->Size())
	    {
		elems[i >> Types::SetDecl::SetPow2Bits] |= (1 << (i & Types::SetDecl::SetMask));
	    }
	}
    }

    Types::SetDecl*  setType = llvm::dyn_cast<Types::SetDecl>(type);
    size_t           size = setType->SetWords();
    llvm::Constant*  initArr[Types::SetDecl::MaxSetWords];
    llvm::ArrayType* aty = llvm::dyn_cast<llvm::ArrayType>(ty);
    llvm::Type*      eltTy = aty->getElementType();

    for (size_t i = 0; i < size; i++)
    {
	initArr[i] = llvm::ConstantInt::get(eltTy, elems[i]);
    }

    llvm::Constant* init = llvm::ConstantArray::get(aty, llvm::ArrayRef<llvm::Constant*>(initArr, size));
    return init;
}

llvm::Value* SetExprAST::MakeConstantSet()
{
    static int  index = 1;
    llvm::Type* ty = type->LlvmType();
    assert(ty && "Expect type for set to work");

    llvm::Constant* init = MakeConstantSetArray();

    llvm::GlobalValue::LinkageTypes linkage = llvm::Function::InternalLinkage;
    std::string                     name("P" + std::to_string(index) + ".set");
    llvm::GlobalVariable*           gv = new llvm::GlobalVariable(*theModule, ty, false, linkage, init, name);
    return gv;
}

llvm::Value* SetExprAST::Address()
{
    TRACE();

    assert(type && "No type supplied");

    assert(type->GetRange()->Size() <= Types::SetDecl::MaxSetSize && "Size too large?");

    // Check if ALL the values involved are constants.
    bool allConstants = true;
    for (auto v : values)
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
	return MakeConstantSet();
    }

    llvm::Value* setV = CreateTempAlloca(type);
    assert(setV && "Expect CreateTempAlloca() to work");

    llvm::Value* tmp = builder.CreateBitCast(setV, Types::GetVoidPtrType());

    size_t size = llvm::dyn_cast<Types::SetDecl>(type)->SetWords();
    builder.CreateMemSet(tmp, MakeConstant(0, Types::Get<Types::CharDecl>()),
                         (size * Types::SetDecl::SetBits / 8), llvm::Align(1));

    // TODO: For optimisation, we may want to pass through the vector and see if the values
    // are constants, and if so, be clever about it.
    // Also, we should combine stores to the same word!
    for (auto v : values)
    {
	// If we have a "range", then make a loop.
	if (RangeExprAST* r = llvm::dyn_cast<RangeExprAST>(v))
	{
	    llvm::Value* low = r->Low();
	    llvm::Value* high = r->High();
	    assert(high && low && "Expected expressions to evalueate");

	    llvm::Function* fn = builder.GetInsertBlock()->getParent();

	    llvm::Value* loopVar = CreateTempAlloca(Types::Get<Types::IntegerDecl>());

	    // Adjust for range:
	    Types::Range* range = type->GetRange();

	    low = builder.CreateSExt(low, Types::Get<Types::IntegerDecl>()->LlvmType(), "sext.low");
	    high = builder.CreateSExt(high, Types::Get<Types::IntegerDecl>()->LlvmType(), "sext.high");

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
	    std::vector<llvm::Value*> ind{ MakeIntegerConstant(0), index };
	    llvm::Value*              bitsetAddr = builder.CreateGEP(setV, ind, "bitsetaddr");
	    llvm::Value*              bitset = builder.CreateLoad(bitsetAddr, "bitset");
	    bitset = builder.CreateOr(bitset, bit);
	    builder.CreateStore(bitset, bitsetAddr);

	    loop = builder.CreateAdd(loop, MakeIntegerConstant(1), "update");
	    builder.CreateStore(loop, loopVar);

	    llvm::Value* endCond = builder.CreateICmpSLE(loop, high, "loopcond");

	    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(theContext, "afterloop", fn);
	    builder.CreateCondBr(endCond, loopBB, afterBB);

	    builder.SetInsertPoint(afterBB);
	}
	else
	{
	    Types::Range* range = type->GetRange();
	    llvm::Value*  rangeStart = MakeIntegerConstant(range->Start());
	    llvm::Value*  x = v->CodeGen();
	    assert(x && "Expect codegen to work!");
	    x = builder.CreateZExt(x, Types::Get<Types::IntegerDecl>()->LlvmType(), "zext");
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
	    std::vector<llvm::Value*> ind{ MakeIntegerConstant(0), index };
	    llvm::Value*              bitsetAddr = builder.CreateGEP(setV, ind, "bitsetaddr");
	    llvm::Value*              bitset = builder.CreateLoad(bitsetAddr, "bitset");
	    bitset = builder.CreateOr(bitset, bit);
	    builder.CreateStore(bitset, bitsetAddr);
	}
    }

    return setV;
}

void WithExprAST::DoDump() const
{
    std::cerr << "With ... do " << std::endl;
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

void RangeReduceAST::DoDump() const
{
    std::cerr << "RangeReduce: ";
    expr->dump();
    std::cerr << "[";
    range->dump();
    std::cerr << "]";
}

llvm::Value* RangeReduceAST::CodeGen()
{
    TRACE();

    assert(expr->Type()->IsIntegral() && "Index is supposed to be integral type");
    llvm::Value* index = expr->CodeGen();

    llvm::Type* ty = index->getType();
    if (auto dr = llvm::dyn_cast<Types::DynRangeDecl>(range))
    {
	size_t       level;
	llvm::Value* low = variables.Find(dr->LowName(), level);
	assert(level == variables.MaxLevel() && "Expect local variable");
	low = builder.CreateLoad(low, "low");
	index = builder.CreateSub(index, low);
    }
    else
    {
	auto rr = llvm::dyn_cast<Types::RangeDecl>(range);
	assert(rr && "This should be a RangeDecl");
	if (int start = rr->Start())
	{
	    index = builder.CreateSub(index, MakeConstant(start, expr->Type()));
	}
	if (ty->getPrimitiveSizeInBits() <
	    Types::Get<Types::IntegerDecl>()->LlvmType()->getPrimitiveSizeInBits())
	{
	    if (expr->Type()->IsUnsigned())
	    {
		index = builder.CreateZExt(index, Types::Get<Types::IntegerDecl>()->LlvmType(), "zext");
	    }
	    else
	    {
		index = builder.CreateSExt(index, Types::Get<Types::IntegerDecl>()->LlvmType(), "sext");
	    }
	}
    }
    return index;
}

void RangeCheckAST::DoDump() const
{
    std::cerr << "RangeCheck: ";
    expr->dump();
    std::cerr << "[";
    range->dump();
    std::cerr << "]";
}

llvm::Value* RangeCheckAST::CodeGen()
{
    TRACE();

    llvm::Value* index = expr->CodeGen();
    assert(index && "Expected expression to generate code");
    assert(index->getType()->isIntegerTy() && "Index is supposed to be integral type");

    llvm::Value* orig_index = index;
    llvm::Type*  intTy = Types::Get<Types::IntegerDecl>()->LlvmType();

    auto rr = llvm::dyn_cast<Types::RangeDecl>(range);
    assert(rr && "Expect a rangedecl here");
    int start = rr->Start();
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
    int               end = rr->GetRange()->Size();
    llvm::Value*      cmp = builder.CreateICmpUGE(index, MakeIntegerConstant(end), "rangecheck");
    llvm::Function*   theFunction = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* oorBlock = llvm::BasicBlock::Create(theContext, "out_of_range");
    llvm::BasicBlock* contBlock = llvm::BasicBlock::Create(theContext, "continue", theFunction);
    builder.CreateCondBr(cmp, oorBlock, contBlock);

    theFunction->getBasicBlockList().push_back(oorBlock);
    builder.SetInsertPoint(oorBlock);
    std::vector<llvm::Value*> args = { builder.CreateGlobalStringPtr(Loc().FileName()),
	                               MakeIntegerConstant(Loc().LineNumber()), MakeIntegerConstant(start),
	                               MakeIntegerConstant(end), orig_index };
    std::vector<llvm::Type*>  argTypes = {
        llvm::PointerType::getUnqual(Types::Get<Types::CharDecl>()->LlvmType()), intTy, intTy, intTy, intTy
    };

    llvm::FunctionCallee fn = GetFunction(MakeVoidType(), argTypes, "range_error");

    builder.CreateCall(fn, args, "");
    builder.CreateUnreachable();

    builder.SetInsertPoint(contBlock);
    return index;
}

void TypeCastAST::DoDump() const
{
    std::cerr << "TypeCast to ";
    type->dump();
    std::cerr << "(";
    expr->dump();
    std::cerr << ")";
}

static llvm::Value* ConvertSet(ExprAST* expr, Types::TypeDecl* type)
{
    TRACE();

    Types::TypeDecl* current = expr->Type();
    Types::SetDecl*  rty = llvm::dyn_cast<Types::SetDecl>(current);
    Types::SetDecl*  lty = llvm::dyn_cast<Types::SetDecl>(type);

    llvm::Value* dest = CreateTempAlloca(type);

    assert(lty && rty && "Expect types on both sides");
    Types::Range* rrange = rty->GetRange();
    Types::Range* lrange = lty->GetRange();

    llvm::Value* w;
    llvm::Value* ind[2] = { MakeIntegerConstant(0), 0 };
    llvm::Value* src = MakeAddressable(expr);
    size_t       p = 0;
    for (auto i = lrange->Start(); i < lrange->End(); i += Types::SetDecl::SetBits)
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
			x = builder.CreateShl(x, 32 - shift);
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
    if (llvm::isa<Types::RealDecl>(type))
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
    if ((Types::IsCharArray(current) || llvm::isa<Types::CharDecl>(current)) &&
        llvm::isa<Types::StringDecl>(type))
    {
	return MakeStringFromExpr(expr, type);
    }
    if (llvm::isa<Types::ClassDecl>(type) && llvm::isa<Types::ClassDecl>(current))
    {
	llvm::Type* ty = llvm::PointerType::getUnqual(type->LlvmType());
	return builder.CreateLoad(builder.CreateBitCast(Address(), ty));
    }
    if (llvm::isa<Types::CharDecl>(type) && llvm::isa<Types::CharDecl>(current))
    {
	return builder.CreateLoad(Address(), "char");
    }
    // Sets are compatible anyway...
    if (llvm::isa<Types::SetDecl>(current))
    {
	return builder.CreateLoad(Address(), "set");
    }
    dump();
    assert(0 && "Expected to get something out of this function");
    return 0;
}

llvm::Value* TypeCastAST::Address()
{
    llvm::Value*     v = 0;
    Types::TypeDecl* current = expr->Type();
    if (llvm::isa<Types::StringDecl>(current))
    {
	v = MakeAddressable(expr);
    }
    else if (llvm::isa<Types::SetDecl>(current))
    {
	v = ConvertSet(expr, type);
    }
    else
    {
	if (llvm::isa<Types::StringDecl>(type))
	{
	    v = MakeStringFromExpr(expr, type);
	}
	else if (AddressableAST* ae = llvm::dyn_cast<AddressableAST>(expr))
	{
	    v = ae->Address();
	}
    }

    assert(v && "Expected to get a value here...");
    if (llvm::isa<Types::CharDecl>(type))
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

void SizeOfExprAST::DoDump() const
{
    std::cerr << "Sizeof(";
    type->dump();
    std::cerr << ")" << std::endl;
}

void BuiltinExprAST::DoDump() const
{
    std::cerr << "Builtin(";
    type->dump();
    std::cerr << ")" << std::endl;
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

void VTableAST::DoDump() const
{
    std::cerr << "VTable(";
    type->dump();
    std::cerr << ")" << std::endl;
}

llvm::Value* VTableAST::CodeGen()
{
    TRACE();

    llvm::StructType* ty = llvm::dyn_cast_or_null<llvm::StructType>(classDecl->VTableType(false));
    assert(ty && "Huh? No vtable?");
    std::vector<llvm::Constant*> vtInit = GetInitializer();

    std::string     name = "vtable_" + classDecl->Name();
    llvm::Constant* init = (vtInit.size()) ? llvm::ConstantStruct::get(ty, vtInit) : 0;
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
    for (size_t i = 0; i < classDecl->MembFuncCount(); i++)
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
    llvm::StructType*            ty = llvm::dyn_cast<llvm::StructType>(classDecl->VTableType(true));
    std::vector<llvm::Constant*> vtInit = GetInitializer();
    assert(vtInit.size() && "Should have something to initialize here");

    llvm::Constant* init = llvm::ConstantStruct::get(ty, vtInit);
    vtable->setInitializer(init);
}

VirtFunctionAST::VirtFunctionAST(const Location& w, ExprAST* slf, int idx, Types::TypeDecl* ty)
    : AddressableAST(w, EK_VirtFunction, ty), index(idx), self(slf)
{
    assert(index >= 0 && "Index should not be negative!");
}

void VirtFunctionAST::DoDump() const
{
    std::cerr << "VirtFunctionAST: " << std::endl;
}

llvm::Value* VirtFunctionAST::Address()
{
    llvm::Value*              v = MakeAddressable(self);
    std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), MakeIntegerConstant(0) };
    v = builder.CreateGEP(v, ind, "vptr");
    ind[1] = MakeIntegerConstant(index);
    v = builder.CreateLoad(v, "vtable");
    v = builder.CreateGEP(v, ind, "mfunc");
    return v;
}

void GotoAST::DoDump() const
{
    std::cerr << "Goto " << dest << std::endl;
}

llvm::Value* GotoAST::CodeGen()
{
    TRACE();

    BasicDebugInfo(this);

    llvm::BasicBlock* labelBB = CreateGotoTarget(dest);
    // Make LLVM-IR valid by jumping to the neew block!
    llvm::Value* v = builder.CreateBr(labelBB);
    // FIXME: This should not be needed, semantics should remove code after goto!
    llvm::Function*   fn = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* dead = llvm::BasicBlock::Create(theContext, "dead", fn);
    builder.SetInsertPoint(dead);
    return v;
}

void UnitAST::DoDump() const
{
    std::cerr << "Unit " << std::endl;
}

void UnitAST::accept(ASTVisitor& v)
{
    for (auto i : code)
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
    if (debugInfo)
    {
	Location loc = Loc();

	// TODO: Fix path and add flags.
	di.builder = new llvm::DIBuilder(*theModule, true);
	llvm::DIFile* file = di.builder->createFile(loc.FileName(), ".");
	di.cu = di.builder->createCompileUnit(llvm::dwarf::DW_LANG_Pascal83, file, "Lacsap",
	                                      optimization >= O1, "", 0);

	debugStack.push_back(&di);
    }

    for (auto a : code)
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

void ClosureAST::DoDump() const
{
    std::cerr << "Closure ";
    for (auto c : content)
    {
	c->dump();
    }
    std::cerr << std::endl;
}

llvm::Value* ClosureAST::CodeGen()
{
    TRACE();

    std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), 0 };
    llvm::Function*           fn = builder.GetInsertBlock()->getParent();
    llvm::Value*              closure = CreateNamedAlloca(fn, type, "$$ClosureStruct");
    int                       index = 0;
    for (auto u : content)
    {
	llvm::Value* v = u->Address();
	ind[1] = MakeIntegerConstant(index);
	llvm::Value* ptr = builder.CreateGEP(closure, ind, u->Name());
	builder.CreateStore(v, ptr);
	index++;
    }
    return closure;
}

void TrampolineAST::DoDump() const
{
    std::cerr << "Trampoline for ";
    func->DoDump();
}

llvm::Value* TrampolineAST::CodeGen()
{
    TRACE();

    llvm::Function* destFn = func->Proto()->LlvmFunction();

    // Temporary memory to store the trampoline itself.
    llvm::Type*               trampTy = llvm::ArrayType::get(Types::Get<Types::CharDecl>()->LlvmType(), 32);
    llvm::Value*              tramp = builder.CreateAlloca(trampTy, 0, "tramp");
    std::vector<llvm::Value*> ind = { MakeIntegerConstant(0), MakeIntegerConstant(0) };
    llvm::Value*              tptr = builder.CreateGEP(tramp, ind, "tramp.first");
    llvm::Value*              nest = closure->CodeGen();
    llvm::Type*               voidPtrTy = Types::GetVoidPtrType();
    nest = builder.CreateBitCast(nest, voidPtrTy, "closure");
    llvm::Value*         castFn = builder.CreateBitCast(destFn, voidPtrTy, "destFn");
    llvm::FunctionCallee llvmTramp = GetFunction(MakeVoidType(), { voidPtrTy, voidPtrTy, voidPtrTy },
                                                 "llvm.init.trampoline");
    builder.CreateCall(llvmTramp, { tptr, castFn, nest });
    llvm::FunctionCallee llvmAdjust = GetFunction(voidPtrTy, { voidPtrTy }, "llvm.adjust.trampoline");
    llvm::Value*         ptr = builder.CreateCall(llvmAdjust, { tptr }, "tramp.ptr");

    return builder.CreateBitCast(ptr, funcPtrTy->LlvmType(), "tramp.func");
}

void TrampolineAST::accept(ASTVisitor& v)
{
    func->accept(v);
}

llvm::Value* InitValueAST::CodeGen()
{
    if (auto set = llvm::dyn_cast<SetExprAST>(values[0]))
    {
	return set->MakeConstantSetArray();
    }
    if (auto sd = llvm::dyn_cast<Types::StringDecl>(type))
    {
	size_t      size = type->Size();
	auto        se = llvm::dyn_cast<StringExprAST>(values[0]);
	std::string str;
	str.resize(size);
	assert(se && "Expected this to be a string expression");
	const std::string& val = se->Str();
	str[0] = val.size();
	for (size_t i = 1; i <= val.size(); i++)
	{
	    str[i] = val[i - 1];
	}

	return llvm::ConstantDataArray::getRaw(str, size, sd->SubType()->LlvmType());
    }
    if (type->IsStringLike() && llvm::isa<Types::ArrayDecl>(type))
    {
	auto                         arrty = llvm::dyn_cast<llvm::ArrayType>(type->LlvmType());
	llvm::Type*                  ty = arrty->getElementType();
	size_t                       size = type->Size();
	std::vector<llvm::Constant*> initArr(size);

	initArr[0] = llvm::dyn_cast<llvm::Constant>(values[0]->CodeGen());
	for (size_t i = 1; i < size; i++)
	{
	    initArr[i] = llvm::Constant::getNullValue(ty);
	}

	llvm::Constant* init = llvm::ConstantArray::get(
	    arrty, llvm::ArrayRef<llvm::Constant*>(initArr.data(), size));
	return init;
    }
    return values[0]->CodeGen();
}

void InitValueAST::DoDump() const
{
    std::cerr << "Init: ";
    for (auto v : values)
    {
	v->dump();
    }
}

llvm::Value* InitArrayAST::CodeGen()
{
    auto aty = llvm::dyn_cast<Types::ArrayDecl>(type);
    assert(aty && "Expected array type here");
    assert(aty->Ranges().size() == 1 && "Expect only 1D arrays right now");
    Types::Range*                range = aty->Ranges()[0]->GetRange();
    size_t                       size = range->Size();
    std::vector<llvm::Constant*> initArr(size);

    for (auto v : values)
    {
	llvm::Constant* c = llvm::dyn_cast<llvm::Constant>(v.Value()->CodeGen());
	if (v.IsRange())
	{
	    for (int i = v.Start(); i <= v.End(); i++)
	    {
		initArr[i - range->Start()] = c;
	    }
	}
	else
	{
	    initArr[v.Start() - range->Start()] = c;
	}
    }
    llvm::ArrayType* arrty = llvm::dyn_cast<llvm::ArrayType>(type->LlvmType());

    llvm::Constant* init = llvm::ConstantArray::get(arrty,
                                                    llvm::ArrayRef<llvm::Constant*>(initArr.data(), size));
    return init;
}

void InitArrayAST::DoDump() const
{
    std::cerr << "InitArray: ";
    for (auto v : values)
    {
	std::cerr << v.Start() << ".." << v.End() << ":";
	v.Value()->dump();
    }
}

llvm::Value* InitRecordAST::CodeGen()
{
    auto fty = llvm::dyn_cast<Types::FieldCollection>(type);
    assert(fty && "Expected field collection type here");
    std::vector<llvm::Constant*> initArr(fty->FieldCount());

    for (auto v : values)
    {
	llvm::Constant* c = llvm::dyn_cast<llvm::Constant>(v.Value()->CodeGen());
	for (auto e : v.Elements())
	{
	    initArr[e] = c;
	}
    }
    llvm::StructType* ty = llvm::dyn_cast<llvm::StructType>(type->LlvmType());

    llvm::Constant* init = llvm::ConstantStruct::get(ty, initArr);
    return init;
}

void InitRecordAST::DoDump() const
{
    std::cerr << "InitRecord: ";
    for (auto v : values)
    {
	char sep = ' ';
	for (auto e : v.Elements())
	{
	    std::cerr << sep << e;
	    sep = ',';
	}
	std::cerr << ":";
	v.Value()->dump();
    }
}

llvm::Value *ArraySliceAST::Address()
{
    auto         aty = llvm::dyn_cast<Types::ArrayDecl>(origType);
    llvm::Value* v = MakeAddressable(expr);
    llvm::Value* low = range->Low();
    int start = aty->Ranges()[0]->Start();
    llvm::Value *index = builder.CreateSub(low, MakeIntegerConstant(start));
    llvm::Value* ptr = builder.CreateGEP(v, { MakeIntegerConstant(0), index });

    return builder.CreateBitCast(ptr, llvm::PointerType::getUnqual(type->LlvmType()));
}

void ArraySliceAST::DoDump() const
{
    expr->dump();
    range->dump();
}

static void BuildUnitInitList()
{
    std::vector<llvm::Constant*> unitList(unitInit.size() + 1);
    llvm::Type*                  vp = Types::GetVoidPtrType();
    int                          index = 0;
    for (auto v : unitInit)
    {
	llvm::Function* fn = theModule->getFunction("P." + v->Proto()->Name());
	assert(fn && "Expected to find the function!");
	unitList[index] = llvm::ConstantExpr::getBitCast(fn, vp);
	index++;
    }
    unitList[unitInit.size()] = llvm::Constant::getNullValue(vp);
    llvm::ArrayType* arr = llvm::ArrayType::get(vp, unitInit.size() + 1);
    llvm::Constant*  init = llvm::ConstantArray::get(arr, unitList);
    llvm::Value*     unitInitList = new llvm::GlobalVariable(
        *theModule, arr, true, llvm::GlobalValue::ExternalLinkage, init, "UnitIniList");
    (void)unitInitList;
    assert(unitInitList && "Unit Initializer List not built correctly?");
}

void BackPatch()
{
    for (auto v : vtableBackPatchList)
    {
	v->Fixup();
    }
    BuildUnitInitList();
}
