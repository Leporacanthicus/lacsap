#include "location.h"
#include <algorithm>


Location::Location(const std::string& file, int line, int col)
{
    fname   = file;
    lineNum = line;
    column  = col;
}

std::string Location::to_string() const
{
    return fname+ ":" + std::to_string(lineNum) + ":" + std::to_string(column); 
}

std::ostream& operator<<(std::ostream &os, const Location& loc)
{
    os << loc.to_string();
    return os;
}
