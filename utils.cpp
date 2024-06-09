#include "utils.h"
#include <iostream>
#include <limits.h>

std::string GetPath(const std::string& filename)
{
    char                   path[PATH_MAX];
    std::string            compiler = realpath(filename.c_str(), path);
    std::string::size_type pos = compiler.find_last_of("/");
    return compiler.substr(0, pos);
}

// TODO: Do we want a source location too?
void InternalCompilerError(const char* file, int line, const std::string& msg)
{
    std::cerr << "Internal compiler error at " << file << ":" << line << ":\n" << msg << "\n" << std::endl;
    std::abort();
}
