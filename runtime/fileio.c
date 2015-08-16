#include <string.h>
#include <stdlib.h>
#include "runtime.h"

/*******************************************
 * File Basics, low level I/O.
 *******************************************
 */

static void FileError(const char *op)
{
    fprintf(stderr, "Attempt to %s file failed\n", op);
}

void __reset(File* f)
{
    if (!f->handle) 
    {
	__assign_unnamed(f);
    }
    if (files[f->handle].inUse && files[f->handle].file == NULL)
    {
	files[f->handle].file = fopen(files[f->handle].name, "r");
	if (files[f->handle].file)
	{
	    __get(f);
	    return;
	}
    }
    FileError("open");
}

void __rewrite(File* f)
{
    if (!f->handle) 
    {
	__assign_unnamed(f);
    }
    if (files[f->handle].inUse && files[f->handle].file == NULL)
    {
	files[f->handle].file = fopen(files[f->handle].name, "w");
	if (files[f->handle].file)
	{
	    return;
	}
    }
    FileError("open");
}

void __append(File* f)
{
    if (!f->handle) 
    {
	__assign_unnamed(f);
    }
    if (files[f->handle].inUse && files[f->handle].file == NULL)
    {
	files[f->handle].file = fopen(files[f->handle].name, "a");
	if (files[f->handle].file)
	{
	    return;
	}
    }
    FileError("append");
}

void __close(File* f)
{
    if (files[f->handle].inUse && files[f->handle].file != NULL)
    {
	fclose(files[f->handle].file);
	files[f->handle].file = NULL;
	return;
    }
    FileError("close");
}
