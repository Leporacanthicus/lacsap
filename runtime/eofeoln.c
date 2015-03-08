#include "runtime.h"

/*******************************************
 * File End of {line,file}
 *******************************************
 */
int __eof(File* file)
{
    FILE* f = getFile(file);
    if (!files[file->handle].readAhead)
    {
	__get(file);
    }
    return !!feof(f);
}

int __eoln(File* file)
{
    if (!files[file->handle].readAhead)
    {
	if (!__get(file))
	{
	    return 1;
	}
    }
    return *file->buffer == '\n';
}
