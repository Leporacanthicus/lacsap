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

enum Standard
{
    none,
    iso7185,
    iso10206,
};

extern int         verbosity;
extern bool        timetrace;
extern bool        disableMemcpyOpt;
extern bool        rangeCheck;
extern bool        debugInfo;
extern bool        callGraph;
extern OptLevel    optimization;
extern Model       model;
extern bool        caseInsensitive;
extern EmitType    emitType;
extern Standard    standard;
extern std::string libpath;
#endif
