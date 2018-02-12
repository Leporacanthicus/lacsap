#ifndef UTILS_H
#define UTILS_H

#include <algorithm>
#include <string>

inline void strlower(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
}

std::string GetPath(const std::string& fileName);

#endif
