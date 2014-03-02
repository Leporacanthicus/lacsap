#ifndef TYPES_H
#define TYPES_H

#include <llvm/IR/Type.h>
#include <string>
#include <map>

class Types
{
public:
    enum SimpleTypes
    {
	Integer,
	Real,
	Char,
	UserDefined,
	Unknown
    };
    static bool IsTypeName(const std::string& name);
    static llvm::Type* GetType(const std::string& name);
    Types();
private:
    std::map<std::string, int> typeNames;
};

#endif
