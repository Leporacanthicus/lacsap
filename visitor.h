#ifndef ASTVISITOR_H
#define ASTVISITOR_H

template<typename T>
class Visitor
{
public:
    virtual void visit(T* elem) = 0;
    virtual ~Visitor() {};
};

template<typename T>
class Visitable
{
public:
    virtual void accept(Visitor<T> &v) = 0;
    virtual ~Visitable() {}
};

class ExprAST;
using ASTVisitor = Visitor<ExprAST>;

#endif
