#ifndef OPTIONS_H
#define OPTIONS_H

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

enum OptFlags
{
    Fmemcpy,
};

extern int verbosity;
extern bool timetrace;
extern bool disableMemcpyOpt;
#endif
