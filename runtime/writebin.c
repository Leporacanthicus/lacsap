#include "runtime.h"
#include <string.h>

void __write_bin(File* file, void* val)
{
    struct FileEntry* f = 0;
    if (file->handle < MaxPascalFiles && files[file->handle].inUse)
    {
	f = &files[file->handle];
    }
    if (!f)
    {
	fprintf(stderr, "Invalid file used for write binary file\n");
	return;
    }
    memcpy(file->buffer, val, file->recordSize);
    __put(file);
}
