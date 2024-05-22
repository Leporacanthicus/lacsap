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
	    : RangeBaseDecl(TK_SchRange, base), start(s), lowName(""), highName(nm), schema(sc)
	{
	}
	SchemaRange(const std::string& lnm, const std::string& hnm, TypeDecl* base, const Schema* sc)
	    : RangeBaseDecl(TK_SchRange, base), start(0), lowName(lnm), highName(hnm), schema(sc)
	{
	}
	void      DoDump() const override;
	TypeDecl* Instantiate(const std::vector<int64_t>& vals);
	static bool classof(const TypeDecl* e) { return e->getKind() == TK_SchRange; }

    private:
	int64_t                        start;
	std::string                    lowName;
	std::string                    highName;
	const Schema*                  schema;
    };

    class SchemaArrayDecl : public ArrayDecl
    {
    public:
	SchemaArrayDecl(TypeDecl* b, const std::vector<RangeBaseDecl*>& r) : ArrayDecl(TK_SchArray, b, r) {}
	// No need for DoDump, it should work with ArrayDecl::DoDump()
	TypeDecl* Instantiate(const std::vector<int64_t>& vals);
    };

    class SchemaSetDecl : public SetDecl
    {
    public:
	SchemaSetDecl(RangeBaseDecl* r, TypeDecl* ty) : SetDecl(TK_SchSet, r, ty) {}
	TypeDecl* Instantiate(const std::vector<int64_t>& vals);
    };
}; // namespace Types
#endif
