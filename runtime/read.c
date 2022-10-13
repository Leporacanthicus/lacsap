#include "runtime.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int read_chunk_text(struct FileEntry* f)
{
    File* file = f->fileData;
    if (file->isText & 2)
    {
	int ch = fgetc(f->file);
	f->readAhead = f->bufferSize = (ch != EOF);
	return ch;
    }
    else
    {
	if (f->readPos != f->bufferSize)
	{
	    f->readAhead = 1;
	    return f->fileData->buffer[f->readPos++];
	}

	int n;
	if ((n = fread(file->buffer, 1, file->recordSize, f->file)))
	{
	    if (n > 0)
	    {
		f->bufferSize = n;
		f->readAhead = 1;
		f->readPos = 1;
		return *file->buffer;
	    }
	}
    }
    return EOF;
}

int get_next(struct FileEntry* f)
{
    return read_chunk_text(f);
}

/* Make a local function so it can inline */
static int __get_text(File* file)
{
    struct FileEntry* f = &files[file->handle];
    int               ch = get_next(f);
    *file->buffer = ch;
    if (ch == EOF)
    {
	f->readAhead = 0;
	return 0;
    }
    return 1;
}

/*******************************************
 * File End of {line,file}
 *******************************************
 */
int __eof(File* file)
{
    if (!files[file->handle].readAhead)
    {
	if (!__get_text(file))
	{
	    return 1;
	}
    }
    return 0;
}

int __eoln(File* file)
{
    if (!files[file->handle].readAhead)
    {
	if (!__get_text(file))
	{
	    return 1;
	}
    }
    return *file->buffer == '\n';
}

/*******************************************
 * Read Functionality
 *******************************************
 */
static void skip_spaces(File* file)
{
    while (isspace(*file->buffer) && !__eof(file))
    {
	__get_text(file);
    }
}

static int get_sign(File* file)
{
    if (*file->buffer == '-')
    {
	__get_text(file);
	return -1;
    }
    else if (*file->buffer == '+')
    {
	__get_text(file);
    }
    return 1;
}

// Turn an exponent into a multiplier value.
static double exponent_to_multi(int exponent)
{
    double m = 1.0;
    int    oneover = 0;
    if (exponent < 0)
    {
	oneover = 1;
	exponent = -exponent;
    }
    while (exponent > 0)
    {
	if (exponent & 1)
	{
	    m *= 10.0;
	    exponent--;
	}
	else
	{
	    m *= 100.0;
	    exponent -= 2;
	}
    }
    if (oneover)
    {
	return 1.0 / m;
    }
    return m;
}

void __read_int64(File* file, int64_t* v)
{
    int64_t n = 0;
    int sign;

    if (file->handle >= MaxPascalFiles)
    {
	return;
    }
    if (!files[file->handle].readAhead)
    {
	__get_text(file);
    }
    skip_spaces(file);
    sign = get_sign(file);
    while (isdigit(*file->buffer))
    {
	n *= 10;
	n += (*file->buffer) - '0';
	if (!__get_text(file))
	{
	    break;
	}
    }
    *v = n * sign;
}

void __read_int32(File* file, int* v)
{
    int64_t n;
    __read_int64(file, &n);
    // We probably should check range?
    *v = (int)n;
}



void __read_chr(File* file, char* v)
{
    if (file->handle >= MaxPascalFiles)
    {
	return;
    }

    if (!files[file->handle].readAhead)
    {
	__get_text(file);
    }

    *v = *file->buffer;
    __get_text(file);
}

void __read_real(File* file, double* v)
{
    double n = 0;
    double divisor = 1.0;
    double multiplicand = 1.0;
    int    exponent = 0;
    int    sign;

    if (file->handle >= MaxPascalFiles)
    {
	return;
    }
    if (!files[file->handle].readAhead)
    {
	__get_text(file);
    }

    skip_spaces(file);
    sign = get_sign(file);

    while (isdigit(*file->buffer))
    {
	n *= 10.0;
	n += (*file->buffer) - '0';
	if (!__get_text(file))
	{
	    break;
	}
    }
    if (*file->buffer == '.')
    {
	__get_text(file);
	while (isdigit(*file->buffer))
	{
	    n *= 10.0;
	    n += (*file->buffer) - '0';
	    divisor *= 10.0;
	    if (!__get_text(file))
	    {
		break;
	    }
	}
    }
    if (*file->buffer == 'e' || *file->buffer == 'E')
    {
	__get_text(file);
	int expsign = get_sign(file);
	while (isdigit(*file->buffer) && !__eoln(file))
	{
	    exponent *= 10;
	    exponent += (*file->buffer) - '0';
	    if (!__get_text(file))
	    {
		break;
	    }
	}
	exponent *= expsign;
	multiplicand = exponent_to_multi(exponent);
    }
    n = n * sign / divisor * multiplicand;
    *v = n;
}

void __read_nl(File* file)
{
    if (!files[file->handle].readAhead)
    {
	if (!__get_text(file))
	{
	    return;
	}
    }
    while (*file->buffer != '\n')
    {
	if (!__get_text(file))
	    break;
    }
    files[file->handle].readAhead = 0;
}

void __read_str(File* file, String* val)
{
    char   buffer[256];
    size_t count = 0;

    if (file->handle >= MaxPascalFiles)
    {
	return;
    }
    if (!files[file->handle].readAhead)
    {
	__get_text(file);
    }
    while (*file->buffer != '\n' && count < sizeof(buffer))
    {
	buffer[count++] = *file->buffer;
	if (!__get_text(file))
	{
	    break;
	}
    }
    val->len = count;
    memcpy(val->str, buffer, count);
}

void __read_chars(File* file, char* v)
{
    if (file->handle >= MaxPascalFiles)
    {
	return;
    }
    if (!files[file->handle].readAhead)
    {
	__get_text(file);
    }
    while (*file->buffer != '\n')
    {
	*v++ = *file->buffer;
	if (!__get_text(file))
	{
	    break;
	}
    }
}
