#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "source.h"
#include <vector>

class ExprAST;
class SemaFixup;

class Semantics
{
public:
    Semantics() : errors(0) {}
    ~Semantics() {}

    void Analyse(Source& src, ExprAST* ast);
    int  GetErrors() { return errors; }
    void AddError() { errors++; }
    void AddFixup(SemaFixup* f);
    void RunFixups();

private:
    int                     errors;
    std::vector<SemaFixup*> fixups;
};

#endif
