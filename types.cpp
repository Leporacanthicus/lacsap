#include "types.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DerivedTypes.h>
#include <sstream>

bool Types::IsTypeName(const std::string& name)
{
    return !!types.Find(name);
}

llvm::Type* Types::GetType(Types::SimpleTypes type)
{
    switch(type)
    {
    case Types::Integer:
	return llvm::Type::getInt32Ty(llvm::getGlobalContext());

    case Types::Real:
	return llvm::Type::getDoubleTy(llvm::getGlobalContext());

    case Types::Char:
	return llvm::Type::getInt8Ty(llvm::getGlobalContext());

    case Types::Boolean:
	return llvm::Type::getInt1Ty(llvm::getGlobalContext());

    case Types::Void:
	return llvm::Type::getVoidTy(llvm::getGlobalContext());
	
    default:
	break;
    }
    return 0;
}

llvm::Type* Types::GetType(const Types::TypeDecl* type)
{
    if (type->GetType() == Types::Array)
    {
	const Types::ArrayDecl* a = dynamic_cast<const Types::ArrayDecl*>(type);
	assert(a && "Huh? Couldn't convert type that says it's an array to ArrayDecl");
	size_t nelems = 0;
	for(auto r : a->Ranges())
	{
	    
	    if (!nelems)
	    {
		nelems = r->Size();
	    }
	    else
	    {
		nelems *= r->Size();
	    }
	}
	assert(nelems && "Expect number of elements to be non-zero!");
	const Types::TypeDecl* base = a->BaseType();
	llvm::Type* ty = GetType(base);
	assert(ty && "Expected to get a type back!");
	return llvm::ArrayType::get(ty, nelems);
    }
    llvm::Type* ty = GetType(type->GetType());
    assert(ty && "Expect basic type to return a Type*");
    return ty;
}

Types::TypeDecl* Types::GetTypeDecl(const std::string& nm)
{
    Types::TypeDecl *ty = types.Find(nm);
    return ty;
}

std::string Types::TypeDecl::to_string() const
{
    std::stringstream ss; 
    ss << "Type: " << (int)type << std::endl;
    return ss.str();
}

bool Types::TypeDecl::isIntegral() const
{
    switch(type)
    {
    case Array:
    case Record:
    case Set:
    case Real:
    case Void:
    case Function:
    case Procedure:
	return false;
    default:
	return true;
    }
}

Types::Types()
{
    types.NewLevel();
    types.Add("integer", new TypeDecl(Integer));
    types.Add("real", new TypeDecl(Real));
    types.Add("char", new TypeDecl(Char));
    types.Add("boolean", new TypeDecl(Boolean));
}
