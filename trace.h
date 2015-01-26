#ifndef TRACE_H
#define TRACE_H

#include "options.h"

void trace(const char *file, int line, const char *func);

#define TRACE() do { if (verbosity) trace(__FILE__, __LINE__, __PRETTY_FUNCTION__); } while(0)

#endif


