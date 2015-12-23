#ifndef OPTIONS_H
#define OPTIONS_H

#include <string>

enum EmitType
{
    Exe, // Default
    LlvmIr,
};

enum OptLevel
{
    O0,
    O1,
    O2,
};

enum Model
{
    m32,
    m64,
};

extern int         verbosity;
extern bool        timetrace;
extern bool        disableMemcpyOpt;
extern bool        rangeCheck;
extern bool        debugInfo;
extern OptLevel    optimization;
extern Model       model;
extern bool        caseInsensitive;
extern std::string libpath;
#endif
