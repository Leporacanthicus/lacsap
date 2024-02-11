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

/* Make a local function so it can inline */
static int __get_text(File* file)
{
    struct FileEntry* f = &files[file->handle];
    int               ch = read_chunk_text(f);
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

struct interface
{
    int (*fngetnext)(struct interface* intf);
    int (*fneof)(struct interface* intf);
    int (*fneoln)(struct interface* intf);
    int (*fncurrent)(struct interface* intf);
    void (*fnpreread)(struct interface* intf);

    union
    {
	File*   file;
	String* str;
    };
};

int file_current(struct interface* intf)
{
    return *intf->file->buffer;
}

int file_get_text(struct interface* intf)
{
    return __get_text(intf->file);
}

int file_eof(struct interface* intf)
{
    return __eof(intf->file);
}

int file_eoln(struct interface* intf)
{
    return __eoln(intf->file);
}

void file_preread(struct interface* intf)
{
    if (!files[intf->file->handle].readAhead)
    {
	__get_text(intf->file);
    }
}

static void initFile(File* file, struct interface* intf)
{
    intf->file = file;
    intf->fngetnext = file_get_text;
    intf->fneof = file_eof;
    intf->fneoln = file_eoln;
    intf->fncurrent = file_current;
    intf->fnpreread = file_preread;
}

/*******************************************
 * Read Functionality
 *******************************************
 */
static void skip_spaces(struct interface* intf)
{
    while (isspace(intf->fncurrent(intf)) && !intf->fneof(intf))
    {
	intf->fngetnext(intf);
    }
}

static int get_sign(struct interface* intf)
{
    if (intf->fncurrent(intf) == '-')
    {
	intf->fngetnext(intf);
	return -1;
    }
    else if (intf->fncurrent(intf) == '+')
    {
	intf->fngetnext(intf);
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

static void readint64(struct interface* intf, int64_t* v)
{
    int64_t n = 0;

    intf->fnpreread(intf);

    skip_spaces(intf);
    int sign = get_sign(intf);
    while (isdigit(intf->fncurrent(intf)))
    {
	n *= 10;
	n += (intf->fncurrent(intf)) - '0';
	if (!intf->fngetnext(intf))
	{
	    break;
	}
    }
    *v = n * sign;
}

void __read_int64(File* file, int64_t* v)
{
    if (file->handle >= MaxPascalFiles)
    {
	return;
    }

    struct interface intf;
    initFile(file, &intf);
    readint64(&intf, v);
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

    struct interface intf;
    initFile(file, &intf);

    intf.fnpreread(&intf);

    *v = intf.fncurrent(&intf);
    intf.fngetnext(&intf);
}

static void readreal(struct interface* intf, double* v)
{
    double n = 0;
    double divisor = 1.0;
    double multiplicand = 1.0;
    int    exponent = 0;

    intf->fnpreread(intf);
    skip_spaces(intf);
    int sign = get_sign(intf);

    while (isdigit(intf->fncurrent(intf)))
    {
	n *= 10.0;
	n += (intf->fncurrent(intf)) - '0';
	if (!intf->fngetnext(intf))
	{
	    break;
	}
    }
    if (intf->fncurrent(intf) == '.')
    {
	intf->fngetnext(intf);
	while (isdigit(intf->fncurrent(intf)))
	{
	    n *= 10.0;
	    n += (intf->fncurrent(intf)) - '0';
	    divisor *= 10.0;
	    if (!intf->fngetnext(intf))
	    {
		break;
	    }
	}
    }
    if (intf->fncurrent(intf) == 'e' || intf->fncurrent(intf) == 'E')
    {
	intf->fngetnext(intf);
	int expsign = get_sign(intf);
	while (isdigit(intf->fncurrent(intf)) && !intf->fneoln(intf))
	{
	    exponent *= 10;
	    exponent += (intf->fncurrent(intf)) - '0';
	    if (!intf->fngetnext(intf))
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

void __read_real(File* file, double* v)
{
    if (file->handle >= MaxPascalFiles)
    {
	return;
    }

    struct interface intf;
    initFile(file, &intf);

    readreal(&intf, v);
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

static void readstr(struct interface* intf, String* v)
{
    char   buffer[256];
    size_t count = 0;

    intf->fnpreread(intf);

    while (!intf->fneoln(intf) && count < sizeof(buffer))
    {
	buffer[count++] = intf->fncurrent(intf);
	if (!intf->fngetnext(intf))
	{
	    break;
	}
    }
    v->len = count;
    memcpy(v->str, buffer, count);
}

void __read_str(File* file, String* v)
{
    if (file->handle >= MaxPascalFiles)
    {
	return;
    }

    struct interface intf;
    initFile(file, &intf);

    readstr(&intf, v);
}

static void readchars(struct interface* intf, char* v)
{
    intf->fnpreread(intf);
    while (intf->fncurrent(intf) != '\n')
    {
	*v++ = intf->fncurrent(intf);
	if (!intf->fngetnext(intf))
	{
	    break;
	}
    }
}

void __read_chars(File* file, char* v)
{
    if (file->handle >= MaxPascalFiles)
    {
	return;
    }
    struct interface intf;
    initFile(file, &intf);

    readchars(&intf, v);
}
