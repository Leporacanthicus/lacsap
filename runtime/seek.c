#include "runtime.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/*******************************************
 * File seek and position functions
 *******************************************
 */

void __seekwrite(File* f, uint64_t n)
{
    if (files[f->handle].inUse && files[f->handle].file != NULL)
    {
	fseek(files[f->handle].file, SEEK_SET, n * f->recordSize);
	return;
    }
    FileError("seekwrite");
}

void __seekread(File* f, uint64_t n)
{
    if (files[f->handle].inUse && files[f->handle].file != NULL)
    {
	fseek(files[f->handle].file, SEEK_SET, n * f->recordSize);
	return;
    }
    FileError("seekread");
}

void __seekupdate(File* f, uint64_t n)
{
    if (files[f->handle].inUse && files[f->handle].file != NULL)
    {
	fseek(files[f->handle].file, SEEK_SET, n * f->recordSize);
	return;
    }
    FileError("seekupdate");
}

/* Note this is not thread-safe, but we don't have threads in Lacsap at present! */
bool __empty(File* f)
{
    if (files[f->handle].inUse && files[f->handle].file != NULL)
    {
	FILE* file = files[f->handle].file;
	long  current = ftell(file);
	fseek(file, SEEK_END, 0);
	long len = ftell(file);
	fseek(file, SEEK_SET, current);
	return (len == 0);
    }
    FileError("empty");
    return false;
}

long __position(File* f)
{
    if (files[f->handle].inUse && files[f->handle].file != NULL)
    {
	long current = ftell(files[f->handle].file);
	return current / f->recordSize;
    }
    FileError("position");
    return 0;
}

long __lastposition(File* f)
{
    if (files[f->handle].inUse && files[f->handle].file != NULL)
    {
	FILE* file = files[f->handle].file;
	long  current = ftell(file);
	fseek(file, SEEK_END, 0);
	long len = ftell(file);
	fseek(file, SEEK_SET, current);
	return len / f->recordSize;
    }
    FileError("lastposition");
    return 0;
}
