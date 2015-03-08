#include <string.h>
#include <stdlib.h>
#include "runtime.h"

/*******************************************
 * File Basics, low level I/O.
 *******************************************
 */
void __reset(File* f)
{
    if (files[f->handle].inUse && files[f->handle].file == NULL)
    {
	files[f->handle].file = fopen(files[f->handle].name, "r");
	if (files[f->handle].file)
	{
	    __get(f);
	    return;
	}
    }
    fprintf(stderr, "Attempt to open file failed\n");
}

void __rewrite(File* f)
{
    if (files[f->handle].inUse && files[f->handle].file == NULL)
    {
	files[f->handle].file = fopen(files[f->handle].name, "w");
	if (files[f->handle].file)
	{
	    return;
	}
    }
    fprintf(stderr, "Attempt to open file failed\n");
}

void __append(File* f)
{
    if (files[f->handle].inUse && files[f->handle].file == NULL)
    {
	files[f->handle].file = fopen(files[f->handle].name, "a");
	if (files[f->handle].file)
	{
	    return;
	}
    }
    fprintf(stderr, "Attempt to append file failed\n");
}

void __close(File* f)
{
    if (files[f->handle].inUse && files[f->handle].file != NULL)
    {
	fclose(files[f->handle].file);
	files[f->handle].file = NULL;
	free(f->buffer);
	f->buffer = NULL;
	return;
    }
    fprintf(stderr, "Attempt to close file failed\n");
}
