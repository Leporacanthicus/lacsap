#include <string.h>
#include "runtime.h"

void __read_bin(File* file, void *val)
{
    struct FileEntry *f = 0;
    if (file->handle < MaxPascalFiles && files[file->handle].inUse)
    {
	f = &files[file->handle];
    }
    if (!f)
    {
	fprintf(stderr, "Invalid file used for read binary\n");
	return;
    }
    memcpy(val, file->buffer, file->recordSize);
    __get(file);
}
