#ifndef NAMES_H
#define NAMES_H

#include <iostream>

// A set of functions to track different named objects, and their type. 

class NamedObject
{
public:
    NamedObject(const std::string& nm, const std::string& ty) 
	: name(nm), type(ty) {}
    const std::string& Type() const { return type; }
    const std::string& Name() const { return name; }
    
    void dump() { std::cerr << "Name: " << name << " Type:" << type << std::endl; }
private:
    std::string name;
    std::string type;
};

#endif
