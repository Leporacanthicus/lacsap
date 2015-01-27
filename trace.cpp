#include "trace.h"
#include <iostream>
#include <chrono>


class TimeTraceImpl
{
public:
    TimeTraceImpl(const char *func) : func(func) 
    {
	start = std::chrono::steady_clock::now();
    }

    ~TimeTraceImpl()
    {
	end = std::chrono::steady_clock::now();
	long elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
	std::cerr << "Time for " << func << " " << elapsed << " microseconds" << std::endl;
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> start, end;
    const char* func;
};


void trace(const char *file, int line, const char *func)
{
    std::cerr << file << ":" << line << "::" << func << std::endl;
}

void TimeTrace::createImpl(const char *func)
{
    impl = new TimeTraceImpl(func);
}


void TimeTrace::destroyImpl()
{
    delete impl;
}

