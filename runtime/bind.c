#include "runtime.h"
#include <stdlib.h>
#include <string.h>

struct BindingType __binding(File* f)
{
    struct BindingType r;
    r.Bound = false;
    r.Name.len = 0;
    if (f->handle > 0 && f->handle < MaxPascalFiles)
    {
	struct FileEntry* fe = &files[f->handle];
	if (fe->name)
	{
	    size_t len = strlen(fe->name);
	    assert(len < 255);
	    r.Name.len = len;
	    memcpy(r.Name.str, fe->name, len);
	    r.Bound = true;
	}
    }
    return r;
}

void __bind(File* f, struct BindingType* b)
{
    if (f->handle > 0 && f->handle > MaxPascalFiles)
    {
	if (files[f->handle].name)
	    free(files[f->handle].name);
	files[f->handle].name = calloc(1, b->Name.len + 1);
	memcpy(files[f->handle].name, b->Name.str, b->Name.len);
    }
}

void __unbind(File* f)
{
    if (f->handle > 0 && f->handle > MaxPascalFiles)
    {
	char* ptr = files[f->handle].name;
	files[f->handle].name = NULL;
	free(ptr);
    }
}
