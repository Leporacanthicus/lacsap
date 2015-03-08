#include <string.h>
#include <stdlib.h>
#include "runtime.h"


/*******************************************
 * Global variables
 *******************************************
 */

/*******************************************
 * File Basics, low level I/O.
 *******************************************
 */

void __put(File *file)
{
    struct FileEntry *f = 0;
    if (file->handle < MaxPascalFiles && files[file->handle].inUse)
    {
	f = &files[file->handle];
    }
    fwrite(file->buffer, f->recordSize, 1, f->file);
}

void __get(File *file)
{
    struct FileEntry *f = 0;
    if (file->handle < MaxPascalFiles && files[file->handle].inUse)
    {
	f = &files[file->handle];
    }
    fread(file->buffer, f->recordSize, 1, f->file);
    f->readAhead = 1;
}

/*******************************************
 * File End of {line,file}
 *******************************************
 */
int __eof(File* file)
{
    if (!file)
    {
	file = &input;
    }
    FILE* f = getFile(file, NULL);
    if (!files[file->handle].readAhead)
    {
	__get(file);
    }
    return !!feof(f);
}

int __eoln(File* file)
{
    if (!file)
    {
	file = &input;
    }
    if (!files[file->handle].readAhead)
    {
	__get(file);
    }
    return !!(*file->buffer == '\n' || __eof(file));
}

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
