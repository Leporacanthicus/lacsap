#include "source.h"
#include <iostream>

char FileSource::Get()
{
    char ch = input.get();
    if (ch == '\n')
    {
	lineNo++;
	lineStart[lineNo] = input.tellg();
	column = 1;
    }
    else
    {
	column++;
    }
    return ch;
}

void FileSource::PrintSource(uint32_t line)
{
    std::fstream::pos_type here = input.tellg();
    char                   ch;
    input.seekg(lineStart[line]);
    while ((ch = input.get()) && ch != '\n' && input)
	std::cerr << ch;
    std::cerr << std::endl;
    input.seekg(here);
}
