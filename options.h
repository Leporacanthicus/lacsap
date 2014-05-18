#ifndef OPTIONS_H
#define OPTIONS_H

enum EmitType
{
    Exe, // Default
    LlvmIr,
};

extern int verbosity;
extern EmitType emitType;

#endif
