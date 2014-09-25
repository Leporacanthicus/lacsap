#ifndef ASTVISITOR_H
#define ASTVISITOR_H

class ExprAST;

class Visitable
{
public:
    virtual void accept(class Visitor &v) = 0;
    virtual ~Visitable() {}
};

class Visitor
{
public:
    virtual void visit(ExprAST* expr) = 0;
    virtual ~Visitor() {};
};


class PrototypeAST;

class UpdateCallVisitor: public Visitor
{
public:
    UpdateCallVisitor(const PrototypeAST *p) : proto(p) {}
    virtual void visit(ExprAST* expr);
private:
    const PrototypeAST* proto;
};

#endif
