#ifndef UTILS_H
#define UTILS_H

#include <algorithm>

inline void strlower(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
}

#endif
