#ifndef ASTVISITOR_H
#define ASTVISITOR_H

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

#endif
