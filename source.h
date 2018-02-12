#ifndef SOURCE_H
#define SOURCE_H

#include "location.h"
#include <fstream>
#include <string>

class Source
{
public:
    virtual ~Source() {}
    virtual char Get() = 0;
    virtual operator bool () const = 0;
    virtual operator Location () const = 0;
};

class FileSource : public Source
{
public:
    FileSource(const std::string &name) 
	: name(name), input(name), column(1), lineNo(1) { }
    char Get() override;
    operator bool() const override { return (bool)input; }
    operator Location() const override { return Location(name, lineNo, column); }
    
private:
    std::string name;
    std::ifstream input;
    uint32_t column;
    uint32_t lineNo;
};

#endif
