#include "runtime.h"

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

int __get(File *file)
{
    struct FileEntry *f = 0;
    if (file->handle < MaxPascalFiles && files[file->handle].inUse)
    {
	f = &files[file->handle];
    }
    if (f->isText)
    {
	int ch = fgetc(f->file);
	*file->buffer = ch;
	f->readAhead = (ch != EOF);
	return f->readAhead;
    }
    else
    {
	if (fread(file->buffer, f->recordSize, 1, f->file) > 0)
	{
	    f->readAhead = 1;
	    return 1;
	}
	f->readAhead = 0;
    }
    return 0;
}
