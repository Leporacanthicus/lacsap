#ifndef TRACE_H
#define TRACE_H

#include "options.h"

class TimeTraceImpl;

class TimeTrace
{
public:
    TimeTrace(const char *func) : impl(0)
    {
	if(timetrace)
	{
	    createImpl(func);
	}
    }
    ~TimeTrace() { if(impl) destroyImpl(); }

private:
    void createImpl(const char *func);
    void destroyImpl();
    TimeTraceImpl *impl;
};

void trace(const char *file, int line, const char *func);

#define TRACE() do { if (verbosity) trace(__FILE__, __LINE__, __PRETTY_FUNCTION__); } while(0)

#define TIME_TRACE()  TimeTrace timeTraceInstance(__FUNCTION__);

#endif


