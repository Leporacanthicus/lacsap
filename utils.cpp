#include "utils.h"
#include <limits.h>

std::string GetPath(const std::string& filename)
{
    char path[PATH_MAX];
    std::string compiler = realpath(filename.c_str(), path) ;
    std::string::size_type pos = compiler.find_last_of("/");
    return compiler.substr(0, pos);
}
