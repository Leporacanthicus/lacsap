#include "source.h"

char FileSource::Get()
{
    char ch = input.get();
    if (ch == '\n')
    {
	lineNo++;
	column = 1;
    }
    else
    {
	column++;
    }
    return ch;
}
