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

#define ICE(msg) InternalCompilerError(__FILE__, __LINE__, msg)

#endif
