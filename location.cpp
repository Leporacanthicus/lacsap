#include "location.h"
//#include <algorithm>

std::string Location::to_string() const
{
    return fname+ ":" + std::to_string(lineNum) + ":" + std::to_string(column) + ":";
}

std::ostream& operator<<(std::ostream &os, const Location& loc)
{
    os << loc.to_string();
    return os;
}
