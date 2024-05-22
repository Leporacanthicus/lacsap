#include "schema.h"
#include "types.h"

namespace Types
{
    Schema::Schema(const std::vector<VarDef>& args) : vars(args) {}

    void Schema::CopyToNameStack(NameStack& stack)
    {
	for (const auto& v : vars)
	{
	    stack.Add(&v);
	}
    }

    const VarDef* Schema::FindVar(const std::string& name) const
    {
	for (const auto& v : vars)
	{
	    if (v.Name() == name)
	    {
		return &v;
	    }
	}
	return 0;
    }

    int Schema::NameToIndex(const std::string& nm) const
    {
	int index = 0;
	for (auto a : vars)
	{
	    if (a.Name() == nm)
	    {
		return index;
	    }
	    index++;
	}
	return -1;
    }

    void SchemaRange::DoDump() const
    {
	std::cerr << "SchemaRange " << start << ".." << name << std::endl;
    }

    TypeDecl* SchemaRange::Instantiate(const std::vector<int64_t>& vals)
    {
	int64_t lowVal = start;
	if (lowName != "")
	{
	    int idx = schema->NameToIndex(lowName);
	    if (idx < 0)
	    {
		return 0;
	    }
	    lowVal = vals[idx];
	}

	int idx = schema->NameToIndex(highName);
	if (idx < 0)
	{
	    return 0;
	}
	return new RangeDecl(new Range(lowVal, vals[idx]), baseType);
    }

    TypeDecl* SchemaArrayDecl::Instantiate(const std::vector<int64_t>& vals)
    {
	std::vector<RangeBaseDecl*> rv;
	for (auto r : Ranges())
	{
	    if (auto sr = llvm::dyn_cast<SchemaRange>(const_cast<RangeBaseDecl*>(r)))
	    {
		auto rr = llvm::dyn_cast<RangeBaseDecl>(sr->Instantiate(vals));
		rv.push_back(rr);
	    }
	    else
	    {
		rv.push_back(const_cast<RangeBaseDecl*>(r));
	    }
	}
	return new ArrayDecl(baseType, rv);
    }

    TypeDecl* SchemaSetDecl::Instantiate(const std::vector<int64_t>& vals)
    {
	auto rr = llvm::dyn_cast<SchemaRange>(range)->Instantiate(vals);
	return new SetDecl(llvm::cast<RangeBaseDecl>(rr), baseType);
    }

    bool IsSchema(const TypeDecl* ty)
    {
	switch (ty->Type())
	{
	case TypeDecl::TK_SchRange:
	case TypeDecl::TK_SchArray:
	case TypeDecl::TK_SchSet:
	    return true;
	default:
	    return false;
	}
    }

    template<typename T>
    static TypeDecl* Instantiate(const std::vector<int64_t>& vals, TypeDecl* ty)
    {
	T* schTy = llvm::dyn_cast<T>(ty);
	return schTy->Instantiate(vals);
    }

    TypeDecl* Instantiate(const std::vector<int64_t>& vals, TypeDecl* ty)
    {
	switch (ty->Type())
	{
	case TypeDecl::TK_SchRange:
	    return Instantiate<SchemaRange>(vals, ty);
	case TypeDecl::TK_SchSet:
	    return Instantiate<SchemaSetDecl>(vals, ty);
	case TypeDecl::TK_SchArray:
	    return Instantiate<SchemaArrayDecl>(vals, ty);
	default:
	    return 0;
	}
    }
} // namespace Types
