#include "expr.h"
#include "stack.h"
#include "builtin.h"
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

#if 1
#define TRACE() std::cerr << __FILE__ << ":" << __LINE__ << "::" << __PRETTY_FUNCTION__ << std::endl
#else
#define TRACE()
#endif


bool FileInfo(llvm::Value*f, int& recSize, bool& isText)
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

void ExprAST::Dump(std::ostream& out) const
{
    out << "Node=" << reinterpret_cast<const void *>(this) << ": "; 
    DoDump(out); 
    out << std::endl;
}	

void ExprAST::Dump(void) const
{ 
    Dump(std::cerr);
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

llvm::Value* MakeConstant(int val, llvm::Type* ty)
{
    return llvm::ConstantInt::get(ty, val);
}

llvm::Value* MakeIntegerConstant(int val)
{
    return MakeConstant(val, Types::GetType(Types::Integer));
}

static llvm::Value* MakeBooleanConstant(int val)
{
    return MakeConstant(val, Types::GetType(Types::Boolean));
}

static llvm::Value* MakeCharConstant(int val)
{
    return MakeConstant(val, Types::GetType(Types::Char));
}

static llvm::AllocaInst* CreateAlloca(llvm::Function* fn, const VarDef& var)
{
    llvm::IRBuilder<> bld(&fn->getEntryBlock(), fn->getEntryBlock().end());
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

std::string ExprAST::ToString()
{
    std::stringstream ss;
    Dump(ss);
    return ss.str();
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
    llvm::Value *v;
    if (type == 0)
    {
	v = MakeIntegerConstant(val);
    }
    else
    {
	v = MakeConstant(val, type);
    }
    return v;
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
	i->Dump(out);
    }
}

llvm::Value* ArrayExprAST::Address()
{
    TRACE();
    llvm::Value* v = expr->Address();
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
    llvm::Value* v = expr->Address();
    std::vector<llvm::Value*> ind;
    ind.push_back(MakeIntegerConstant(0));
    ind.push_back(MakeIntegerConstant(element));
    v = builder.CreateGEP(v, ind, "valueindex");
    return v;
}

void PointerExprAST::DoDump(std::ostream& out) const
{
    out << "Pointer:";
    pointer->Dump(out);
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
    pointer->Dump(out);
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
    VariableExprAST* vptr = dynamic_cast<VariableExprAST*>(pointer);
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
    std::cerr << "Actual Name: "  << actualName << std::endl;
    return theModule->getFunction(actualName);
}

llvm::Value* FunctionExprAST::Address()
{
    assert(0 && "Don't expect this to be called...");
    return 0;
}

void BinaryExprAST::DoDump(std::ostream& out) const
{ 
    out << "BinaryOp: ";
    lhs->Dump(out);
    oper.Dump(out);
    rhs->Dump(out); 
}

static llvm::Value* CallSetFunc(const std::string& name, 
				llvm::Value* lhs, 
				llvm::Value* rhs, 
				llvm::Type *resTy)
{
    std::string func = std::string("__Set") + name;
    std::vector<llvm::Type*> argTypes;
    
    llvm::Type* ty = Types::TypeForSet();
    if (!ty)
    {
	return 0;
    }
    llvm::Value* rV = CreateTempAlloca(ty);
    llvm::Value* lV = CreateTempAlloca(ty);
    if (!rV || !lV)
    {
	return 0;
    }

    builder.CreateStore(lhs, lV);
    builder.CreateStore(rhs, rV);

    llvm::Type* pty = llvm::PointerType::getUnqual(Types::TypeForSet());
    argTypes.push_back(pty);
    argTypes.push_back(pty);

    llvm::FunctionType* ft = llvm::FunctionType::get(resTy, argTypes, false);
    llvm::Constant* f = theModule->getOrInsertFunction(func, ft);

    return builder.CreateCall2(f, lV, rV, "calltmp");
}

llvm::Value* BinaryExprAST::CodeGen()
{
    TRACE();
    llvm::Value *l = lhs->CodeGen();
    llvm::Value *r = rhs->CodeGen();
    
    if (l == 0 || r == 0) 
    {
	return 0;
    }

    llvm::Type::TypeID rty = r->getType()->getTypeID();
    llvm::Type::TypeID lty = l->getType()->getTypeID();

    bool rToFloat = false;
    bool lToFloat = false;

    /* Convert right hand side to double if left is double, and right is integer */
    if (rty == llvm::Type::IntegerTyID)
    {
	if (lty == llvm::Type::DoubleTyID || oper.GetToken() == Token::Divide)
	{
	    rToFloat = true;
	}
    }
    if (lty == llvm::Type::IntegerTyID)
    {
	if (rty == llvm::Type::DoubleTyID || oper.GetToken() == Token::Divide)
	{
	    lToFloat = true;
	}
    }

    if (rToFloat)
    {
	r = builder.CreateSIToFP(r, Types::GetType(Types::Real), "tofp");
	rty = r->getType()->getTypeID();
    }

    if (lToFloat)
    {
	l = builder.CreateSIToFP(l, Types::GetType(Types::Real), "tofp");
	lty = r->getType()->getTypeID();
    }

    if (r->getType() == Types::TypeForSet() && lty == llvm::Type::IntegerTyID)
    {
	if (oper.GetToken() == Token::In)
	{
	    std::vector<llvm::Value*> ind;
	    AddressableAST* rhsA = dynamic_cast<AddressableAST*>(rhs);
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
    }

    if (rty != lty)
    {
	std::cout << "Different types..." << std::endl;
	l->dump();
	r->dump();
	assert(0 && "Different types...");
	return 0;
    }

    if (r->getType() == Types::TypeForSet())
    {
	llvm::Type* resTy = Types::GetType(Types::Boolean);
	switch(oper.GetToken())
	{
	case Token::Minus:
	    resTy = Types::TypeForSet();
	    return CallSetFunc("Diff", l, r, resTy);

	case Token::Plus:
	    resTy = Types::TypeForSet();
	    return CallSetFunc("Union", l, r, resTy);
	    
	case Token::Multiply:
	    resTy = Types::TypeForSet();
	    return CallSetFunc("Intersect", l, r, resTy);
	    
	case Token::Equal:
	    return CallSetFunc("Equal", l, r, resTy);

	case Token::NotEqual:
	{
	    llvm::Value *res = CallSetFunc("Equal", l, r, resTy);
	    return builder.CreateNot(res, "notEqual");
	}

	case Token::LessOrEqual:
	    return CallSetFunc("Contains", l, r, resTy);

	case Token::GreaterOrEqual:
	    // Note reverse order to avoid having to write 
	    // another function
	    return CallSetFunc("Contains", r, l, resTy);

	default:
	    return ErrorV("Unknown operator on set");
	}
    }
    else if (rty == llvm::Type::IntegerTyID)
    {
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

	case Token::Equal:
	    return builder.CreateICmpEQ(l, r, "eq");
	case Token::NotEqual:
	    return builder.CreateICmpNE(l, r, "ne");
	case Token::LessThan:
	    return builder.CreateICmpSLT(l, r, "lt");
	case Token::LessOrEqual:
	    return builder.CreateICmpSLE(l, r, "le");
	case Token::GreaterThan:
	    return builder.CreateICmpSGT(l, r, "gt");
	case Token::GreaterOrEqual:
	    return builder.CreateICmpSGE(l, r, "ge");

	case Token::And:
	    return builder.CreateAnd(l, r, "and");

	case Token::Or:
	    return builder.CreateOr(l, r, "or");
	    
	default:
	    return ErrorV(std::string("Unknown token: ") + oper.ToString());
	}
    }
    else if (rty == llvm::Type::DoubleTyID)
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
	oper.Dump(std::cout);
	r->dump();
	return ErrorV("Huh?");
    }
}

void UnaryExprAST::DoDump(std::ostream& out) const
{ 
    out << "Unary: " << oper.ToString();
    rhs->Dump(out);
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
	i->Dump(out);
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
	    VariableExprAST* vi = dynamic_cast<VariableExprAST*>(i);
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
    if (proto->ResultType()->Type() == Types::Void) 
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
	i->Dump(out);
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
	p->Dump(out);
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
    if (namePrefix != "")
    {
	actualName = namePrefix + "." + name;
    }
    else
    {
	actualName = name;
    }

    if (!mangles.Add(name, new MangleMap(actualName)))
    {
	return ErrorF(std::string("Name ") + name + " already in use?");
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
    return CodeGen("");
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
    proto->Dump(out);
    out << "Function body:" << std::endl;
    body->Dump(out);
}

llvm::Function* FunctionAST::CodeGen(const std::string& namePrefix)
{
    VarStackWrapper w(variables);
    TRACE();
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

    variables.Dump();
    mangles.Dump();
    
    builder.SetInsertPoint(bb, ip);
    llvm::Value *block = body->CodeGen();
    if (!block && !body->IsEmpty())
    {
	return 0;
    }

    if (proto->ResultType()->Type() == Types::Void)
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
//    fpm->run(*theFunction);
    theFunction->dump();
    return theFunction;
}

llvm::Function* FunctionAST::CodeGen()
{
    return CodeGen("");
}

void FunctionAST::SetUsedVars(const std::vector<NamedObject*>& varsUsed, 
			      const std::vector<NamedObject*>& localVars)
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

    for(auto l : localVars)
    {
	nonLocal.erase(l->Name());
    }
    std::cerr << "Used variables: ";
    for(auto n : nonLocal)
    {
	VarDef* v = dynamic_cast<VarDef*>(n.second);
	if (v)
	{
	    v->dump();
	    usedVariables.push_back(*v);
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
    lhs->Dump(out);
    out << ":=";
    rhs->Dump(out);
}

llvm::Value* AssignExprAST::CodeGen()
{
    TRACE();
    VariableExprAST* lhsv = dynamic_cast<VariableExprAST*>(lhs);
    if (!lhsv)
    {
	lhs->Dump(std::cerr);
	return ErrorV("Left hand side of assignment must be a variable");
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

    if (rty == llvm::Type::IntegerTyID  &&
	lty == llvm::Type::DoubleTyID)
    {
	v = builder.CreateSIToFP(v, Types::GetType(Types::Real), "tofp");
	rty = v->getType()->getTypeID();
    }	
    assert(rty == lty && 
	   "Types must be the same in assignment.");
    
    builder.CreateStore(v, dest);
    
    return v;
}

void IfExprAST::DoDump(std::ostream& out) const
{
    out << "if: " << std::endl;
    cond->Dump(out);
    out << "then: ";
    if (then)
    {
	then->Dump(out);
    }
    if (other)
    {
	out << " else::";
	other->Dump(out);
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

    if (condV->getType()->getTypeID() ==  llvm::Type::IntegerTyID)
    {
	condV = builder.CreateICmpNE(condV, MakeBooleanConstant(0), "ifcond");
    }
    else
    {
	assert(0 && "Only integer expressions allowed in if-statement");
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
    start->Dump(out);
    if (stepDown)
	out << " downto ";
    else
	out << " to ";
    end->Dump(out);
    out << " do ";
    body->Dump(out);
}

llvm::Value* ForExprAST::CodeGen()
{
    TRACE();

    llvm::Function* theFunction = builder.GetInsertBlock()->getParent();
    llvm::Value* var = variables.Find(varName);

    llvm::Value* startV = start->CodeGen();
    if (!startV)
    {
	return 0;
    }

    builder.CreateStore(startV, var); 

    llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "loop", theFunction);    
    builder.CreateBr(loopBB);
    builder.SetInsertPoint(loopBB);

    if (!body->CodeGen())
    {
	return 0;
    }
    llvm::Value* stepVal =MakeConstant((stepDown)?-1:1, startV->getType());
    llvm::Value* curVar = builder.CreateLoad(var, varName.c_str());
    llvm::Value* nextVar = builder.CreateAdd(curVar, stepVal, "nextvar");

    builder.CreateStore(nextVar, var);
    
    llvm::Value* endCond;
    llvm::Value* endV = end->CodeGen();
    if (stepDown) 
    {
	endCond = builder.CreateICmpSGE(nextVar, endV, "loopcond");
    }
    else
    {
	endCond = builder.CreateICmpSLE(nextVar, endV, "loopcond");
    }

    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "afterloop", 
							 theFunction);
    
    builder.CreateCondBr(endCond, loopBB, afterBB);
    
    builder.SetInsertPoint(afterBB);

    return afterBB;
}

void WhileExprAST::DoDump(std::ostream& out) const
{
    out << "While: ";
    cond->Dump(out);
    out << " Do: ";
    body->Dump(out);
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
    body->Dump(out);
    out << " until: ";
    cond->Dump(out);
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
	a.expr->Dump(out);
	if (a.width)
	{
	    out << ":";
	    a.width->Dump(out);
	}
	if (a.precision)
	{
	    out << ":";
	    a.precision->Dump(out);
	}
    }
    out << ")";
}

static llvm::Constant *CreateWriteFunc(llvm::Type* ty, llvm::Type* fty)
{
    std::string suffix;
    std::vector<llvm::Type*> argTypes;
    llvm::Type* resTy = Types::GetType(Types::Void);
    argTypes.push_back(fty);
    if (ty)
    {
	if (ty == Types::GetType(Types::Char))
	{
	    argTypes.push_back(ty);
	    argTypes.push_back(Types::GetType(Types::Integer));
	    suffix = "char";
	}
	else if (ty == Types::GetType(Types::Boolean))
	{
	    argTypes.push_back(ty);
	    argTypes.push_back(Types::GetType(Types::Integer));
	    suffix = "bool";
	}
	else if (ty->isIntegerTy())
	{
	    // Make args of two integers. 
	    argTypes.push_back(ty);
	    argTypes.push_back(ty);
	    suffix = "int";
	}
	else if (ty->isDoubleTy())
	{
	    // Args: double, int, int
	    argTypes.push_back(ty);
	    llvm::Type* t = Types::GetType(Types::Integer); 
	    argTypes.push_back(t);
	    argTypes.push_back(t);
	    suffix = "real";
	}
	else if (ty->isPointerTy())
	{
	    if (ty->getContainedType(0) != Types::GetType(Types::Char))
	    {
		return ErrorF("Invalid type argument for write");
	    }
	    argTypes.push_back(ty);
	    llvm::Type* t = Types::GetType(Types::Integer); 
	    argTypes.push_back(t);
	    suffix = "str";
	}
	else
	{
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
	    v = arg.expr->CodeGen();
	    if (!v)
	    {
		return ErrorV("Argument codegen failed");
	    }
	    argsV.push_back(v);
	    llvm::Type*     ty = v->getType();
	    fn = CreateWriteFunc(ty, f->getType());
	    llvm::Value*    w;
	    if (!arg.width)
	    {
		if (ty == Types::GetType(Types::Integer))
		{
		    w = MakeIntegerConstant(13);
		}
		else if (ty->isDoubleTy())
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
	    if (ty->isDoubleTy())
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
	    VariableExprAST* vexpr = dynamic_cast<VariableExprAST*>(arg.expr);
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
	a->Dump(out);
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
	VariableExprAST* vexpr = dynamic_cast<VariableExprAST*>(arg);
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
	    llvm::Type     *ty = var.Type()->LlvmType();
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
    stmt->Dump(out);
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
    expr->Dump(out);
    out << " of " << std::endl;
    for(auto l : labels)
    {
	l->Dump(out);
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

    llvm::SwitchInst* sw = builder.CreateSwitch(v, afterBB, labels.size());
    for(auto ll : labels)
    {
	ll->CodeGen(sw, afterBB, ty);
    }

    builder.SetInsertPoint(afterBB);
    
    return afterBB;
}

void RangeExprAST::DoDump(std::ostream& out) const
{
    out << "Range:";
    low->Dump(out); 
    out << "..";
    high->Dump(out);
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
	v->Dump(out);
    }
    out << "]";
}

llvm::Value* SetExprAST::Address()
{
    TRACE();

    llvm::Type* ty = Types::TypeForSet();
    if (!ty)
    {
	return 0;
    }
    llvm::Value* setV = CreateTempAlloca(ty);
    if (!setV)
    {
	return 0;
    }

    llvm::Value* tmp = builder.CreateBitCast(setV, Types::GetVoidPtrType());

    builder.CreateMemSet(tmp, MakeConstant(0, Types::GetType(Types::Char)), 
			 Types::SetDecl::MaxSetWords * 4, 0);

    // TODO: For optimisation, we may want to pass through the vector and see if the values 
    // are constants, and if so, be clever about it
    for(auto v : values)
    {
	// If we have a "range", then make a loop. 
	RangeExprAST* r = dynamic_cast<RangeExprAST*>(v);
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
	    builder.CreateOr(bitset, bit);
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
	    builder.CreateOr(bitset, bit);
	    builder.CreateStore(bitset, bitsetAddr);
	}
    }
    builder.GetInsertBlock()->getParent()->dump();
    
    return setV;
}

llvm::Value* SetExprAST::CodeGen()
{
    llvm::Value* v = Address();
    return builder.CreateLoad(v);
}


int GetErrors(void)
{
    return errCnt;
}
