#include "types.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DerivedTypes.h>
#include <sstream>
#include <climits>

#define TRACE() std::cerr << __FILE__ << ":" << __LINE__ << "::" << __PRETTY_FUNCTION__ << std::endl

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
    return type->LlvmType();
}

Types::TypeDecl* Types::GetTypeDecl(const std::string& nm)
{
    Types::TypeDecl *ty = types.Find(nm);
    return ty;
}

bool Types::IsTypeName(const std::string& name)
{
    return !!types.Find(name);
}

bool Types::IsEnumValue(const std::string& name)
{
    return !!enums.Find(name);
}

bool Types::Add(const std::string& nm, TypeDecl* ty)
{
    Types::EnumDecl* ed = dynamic_cast<EnumDecl*>(ty);
    if (ed)
    {
	for(auto v : ed->Values())
	{
	    if (!enums.Add(v.name, new Types::EnumValue(v)))
	    {
		std::cerr << "Enumerated value by name " << v.name << " already exists..." << std::endl;
	    }
	}
    }
    return types.Add(nm, ty);
}

void Types::FixUpIncomplete(Types::PointerDecl *p)
{
    TypeDecl *ty = types.Find(p->Name());
    if(!ty)
    {
	std::cerr << "Forward declared pointer type not declared: " << p->Name() << std::endl;
	return;
    }
    p->SetSubType(ty);
}

Types::EnumValue* Types::FindEnumValue(const std::string& nm)
{
    return enums.Find(nm);
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
    case Integer:
    case Char:
	return true;
    default:
	return true;
    }
}

Types::Range* Types::TypeDecl::GetRange() const
{
    assert(isIntegral());
    switch(type)
    {
    case Char:
	return new Range(0, UCHAR_MAX);
    case Integer:
	return new Range(INT_MIN, INT_MAX);
    default:
	return 0;
    }
}

static const char* TypeToStr(Types::SimpleTypes t)
{
    switch(t)
    {
    case Types::Integer:
	return "Integer";
    case Types::Real:
	return "Real";
    case Types::Char:
	return "Char";
    case Types::Boolean:
	return "Boolean";
    case Types::Array:
	return "Array";
    case Types::Function:
	return "Function";
    case Types::Procedure:
	return "Procedure";
    case Types::Record:
	return "Record";
    case Types::Set:
	return "Set";
    case Types::SubRange:
	return "SubRange";
    case Types::Enum:
	return "Enum";
    case Types::Pointer:
	return "Pointer";
    case Types::PointerIncomplete:
	return "PointerIncomplete";
    case Types::Void:
	return "Void";
    case Types::Field:
	return "Field";
    }
}

void Types::TypeDecl::dump()
{
    std::cerr << "Type: " << TypeToStr(type);
}

llvm::Type* Types::TypeDecl::LlvmType() const
{
    llvm::Type* ty = GetType(type);
    return ty;
}

void Types::PointerDecl::dump()
{
    std::cerr << "Pointer to: ";
    baseType->dump();
}

llvm::Type* Types::PointerDecl::LlvmType() const
{
    llvm::Type* ty = llvm::PointerType::getUnqual(GetType(baseType));
    return ty;
}

void Types::ArrayDecl::dump()
{
    std::cerr << "Array ";
    for(auto r : ranges)
    {
	r->dump();
    }
    std::cerr << " of "; 
    baseType->dump();
}

llvm::Type* Types::ArrayDecl::LlvmType() const
{
    size_t nelems = 0;
    for(auto r : ranges)
    {
	assert(r->Size() && "Expectig range to have a non-zero size!");
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
    llvm::Type* ty = baseType->LlvmType();
    assert(ty && "Expected to get a type back!");
    return llvm::ArrayType::get(ty, nelems);
}

void Types::Range::dump()
{
    std::cerr << "[" << start << ".." << end << "]";
}

void Types::RangeDecl::dump()
{
    std::cerr << "RangeDecl: " << TypeToStr(baseType) << " " << range << std::endl;
}

llvm::Type* Types::RangeDecl::LlvmType() const
{
    return GetType(baseType);
}

void Types::EnumDecl::SetValues(const std::vector<std::string>& nmv)
{
    unsigned int v = 0;
    for(auto n : nmv)
    {
	EnumValue e(n, v);
	values.push_back(e);
	v++;
    }
}

void Types::EnumDecl::dump()
{
    std::cerr << "EnumDecl:";
    for(auto v : values)
    {
	std::cerr << "   " << v.name << ": " << v.value;
    }
}

llvm::Type* Types::EnumDecl::LlvmType() const
{
    return GetType(Integer);
}

void Types::FieldDecl::dump()
{
    std::cerr << "Field " << name << ": ";
    baseType->dump();
}

llvm::Type* Types::FieldDecl::LlvmType() const
{
    return baseType->LlvmType();
}

void Types::RecordDecl::dump()
{
    std::cerr << "Record ";
    for(auto f : fields)
    {
	f.dump();
    }
}

llvm::Type* Types::RecordDecl::LlvmType() const
{
    std::vector<llvm::Type*> fv;
    for(auto f : fields)
    {
	fv.push_back(f.LlvmType());
    }
    return llvm::StructType::create(fv);
}

// This is a very basic algorithm, but I think it's good enough for 
// most structures - there's unlikely to be a HUGE number of them. 
int Types::RecordDecl::Element(const std::string& name) const
{
    int i = 0;
    for(auto f : fields)
    {
	if (f.Name() == name)
	{
	    return i;
	}
	i++;
    }
    return -1;
}

void Types::Dump()
{
    types.Dump(std::cerr);
}

Types::Types()
{
    types.NewLevel();
    types.Add("integer", new TypeDecl(Integer));
    types.Add("real", new TypeDecl(Real));
    types.Add("char", new TypeDecl(Char));
    types.Add("boolean", new TypeDecl(Boolean));
}

