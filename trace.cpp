#include "trace.h"
#include <iostream>

void trace(const char *file, int line, const char *func)
{
    std::cerr << file << ":" << line << "::" << func << std::endl;
}
