#ifndef SCHEMA_H_
#define SCHEMA_H_

#include "namedobject.h"
#include "stack.h"
#include "types.h"

namespace Types
{
    bool      IsSchema(const TypeDecl* ty);
    TypeDecl* Instantiate(const std::vector<int64_t>& values, TypeDecl* ty);
    class Schema
    {
    public:
	Schema(const std::vector<VarDef>& args);
	void          CopyToNameStack(NameStack& stack);
	const VarDef* FindVar(const std::string& name) const;
	int           NameToIndex(const std::string& name) const;
	virtual ~Schema() = default;

    private:
	std::vector<VarDef> vars;
    };

    class SchemaRange : public RangeBaseDecl
    {
    public:
	SchemaRange(int s, const std::string& nm, TypeDecl* base, const Schema* sc)
	    : RangeBaseDecl(TK_SchRange, base), start(s), name(nm), schema(sc)
	{
	}
	void      DoDump() const override;
	TypeDecl* Instantiate(const std::vector<int64_t>& vals);

    private:
	int64_t                        start;
	std::string                    name;
	[[maybe_unused]] const Schema* schema;
    };
}; // namespace Types
#endif
