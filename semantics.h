#ifndef SEMANTICS_H
#define SEMANTICS_H

#include <vector>

class ExprAST;

class Semantics
{
public:
    Semantics() : errors(0) {}

    void Analyse(std::vector<ExprAST*>& ast);
    int GetErrors() { return errors; }

private:
    int errors;
};


#endif
