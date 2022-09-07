#include "runtime.h"
#include <stdlib.h>
#include <string.h>

/*******************************************
 * File Basics, low level I/O.
 *******************************************
 */

static void FileError(const char* op)
{
    fprintf(stderr, "Attempt to %s file failed\n", op);
    exit(1);
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

static void OpenFile(File* f, int recSize, int isText, const char* mode)
{
    if (!f->handle)
    {
	__assign_unnamed(f);
    }
    SetupFile(f, recSize, isText);
    if (files[f->handle].inUse)
    {
	if (files[f->handle].file)
	{
	    __close(f);
	}
	files[f->handle].file = fopen(files[f->handle].name, mode);
	if (files[f->handle].file)
	{
	    return;
	}
    }
    FileError("open");
}

void __reset(File* f, int recSize, int isText)
{
    OpenFile(f, recSize, isText, "r");
    __get(f);
}

void __rewrite(File* f, int recSize, int isText)
{
    OpenFile(f, recSize, isText, "w");
}

void __append(File* f, int recSize, int isText)
{
    OpenFile(f, recSize, isText, "a");
}
