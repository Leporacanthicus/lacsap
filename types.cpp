#include "types.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DerivedTypes.h>

bool Types::IsTypeName(const std::string& name)
{
    if (name == "integer" ||
	name == "real" ||
	name == "boolean" ||
	name == "char")
	return true;
    return false;
}


llvm::Type* Types::GetType(const std::string& name)
{
    if (name == "integer")
    {
	return llvm::Type::getInt32Ty(llvm::getGlobalContext());
    }

    if (name == "real")
    {
	return llvm::Type::getDoubleTy(llvm::getGlobalContext());
    }

    if (name == "char")
    {
	return llvm::Type::getInt8Ty(llvm::getGlobalContext());
    }

    if (name == "void")
    {
	return llvm::Type::getVoidTy(llvm::getGlobalContext());
    }

    if (name == "boolean")
    {
	return llvm::Type::getInt1Ty(llvm::getGlobalContext());
    }

    assert(0 && "Type not yet supported");
    return 0;
}

Types::Types()
{
}
