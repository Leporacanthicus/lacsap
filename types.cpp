#include "types.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DerivedTypes.h>
#include <sstream>

bool Types::IsTypeName(const std::string& name)
{
    if (name == "integer" ||
	name == "real" ||
	name == "boolean" ||
	name == "char")
	return true;
    return false;
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
    /* Need to support pointer, array and record here */
    return GetType(type->GetType());
}

Types::TypeDecl* Types::GetTypeDecl(const std::string& nm)
{
    Types::TypeDecl *ty = types.Find(nm);
    return ty;
}

std::string Types::TypeDecl::to_string() const
{
    std::stringstream ss; 
    ss << "Type: " << (int)base << std::endl;
    return ss.str();
}



Types::Types()
{
    types.NewLevel();
    types.Add("integer", new TypeDecl(Integer));
    types.Add("real", new TypeDecl(Real));
    types.Add("char", new TypeDecl(Char));
    types.Add("boolean", new TypeDecl(Boolean));
}
