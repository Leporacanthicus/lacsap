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

    void SchemaRange::DoDump() const
    {
	std::cerr << "SchemaRange " << start << ".." << name << std::endl;
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

    bool IsSchema(const TypeDecl* ty)
    {
	switch (ty->Type())
	{
	case TypeDecl::TK_SchRange:
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
	default:
	    return 0;
	}
    }
} // namespace Types
