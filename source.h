#ifndef SOURCE_H
#define SOURCE_H

#include "location.h"
#include <cstdint>
#include <fstream>
#include <map>
#include <string>

class Source
{
public:
    virtual ~Source() {}
    virtual char Get() = 0;
    virtual      operator bool() const = 0;
    virtual      operator Location() const = 0;
    virtual void PrintSource(uint32_t line) {}
};

class FileSource : public Source
{
public:
    FileSource(const std::string& name) : name(name), input(name), column(1), lineNo(1) { lineStart[1] = 0; }
    char Get() override;
         operator bool() const override { return (bool)input; }
         operator Location() const override { return Location(name, lineNo, column); }
         void PrintSource(uint32_t line) override;

     private:
         std::string                                name;
         std::ifstream                              input;
         uint32_t                                   column;
         uint32_t                                   lineNo;
         std::map<uint32_t, std::fstream::pos_type> lineStart;
};

#endif
