#ifndef UTILS_H
#define UTILS_H

#include <algorithm>
#include <string>

inline void strlower(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
}

std::string GetPath(const std::string& fileName);

[[noreturn]] void InternalCompilerError(const char* file, int line, const std::string& msg);
[[noreturn]] void InternalCompilerError(const char* file, int line, const std::string& condStr,
                                        const std::string& msg);

inline void iceIf(const char* file, int line, bool cond, const std::string& condStr, const std::string& msg)
{
    if (cond)
    {
	InternalCompilerError(file, line, condStr, msg);
    }
}

#define ICE(msg) InternalCompilerError(__FILE__, __LINE__, msg)
#define ICE_IF(cond, msg) (__FILE__, __LINE__, cond, #cond, msg)

#endif
