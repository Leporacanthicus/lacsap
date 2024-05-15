#include "runtime.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	File* file;
	struct
	{
	    String* str;
	    int     strpos;
	};
    };
};

static int file_current(struct interface* intf)
{
    return *intf->file->buffer;
}

static int file_get_text(struct interface* intf)
{
    return __get_text(intf->file);
}

static int file_eof(struct interface* intf)
{
    return __eof(intf->file);
}

static int file_eoln(struct interface* intf)
{
    return __eoln(intf->file);
}

static void file_preread(struct interface* intf)
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

static int str_current(struct interface* intf)
{
    return intf->str->str[intf->strpos];
}

static int str_get_text(struct interface* intf)
{
    if (intf->strpos == intf->str->len)
    {
	return EOF;
    }
    intf->strpos++;
    return str_current(intf);
}

static int str_eof(struct interface* intf)
{
    return (intf->strpos == intf->str->len);
}

// Nothing to do here.
static void str_preread(struct interface* intf)
{
    (void)intf;
}

static void initStr(String* str, struct interface* intf)
{
    intf->str = str;
    intf->strpos = 0;
    intf->fngetnext = str_get_text;
    // EOF and EOLN are the same, we don't support multi-line input...
    intf->fneof = str_eof;
    intf->fneoln = str_eof;
    intf->fncurrent = str_current;
    intf->fnpreread = str_preread;
}

struct interfaceNode
{
    struct interfaceNode* next;
    struct interface*     intf;
};

static struct interfaceNode* intfList;

struct interface* __read_S_init(String* str)
{
    struct interface* intf = malloc(sizeof(struct interface));
    initStr(str, intf);
    // TODO: Threading will need a lock here.
    struct interfaceNode* intfNode = malloc(sizeof(struct interfaceNode));
    intfNode->intf = intf;
    intfNode->next = intfList;
    intfList = intfNode;
    return intf;
}

void __read_S_end(struct interface* intf)
{
    struct interfaceNode* prev = NULL;
    for (struct interfaceNode* in = intfList; in; prev = in, in = in->next)
    {
	if (in->intf == intf)
	{
	    if (prev)
	    {
		prev->next = in->next;
	    }
	    else
	    {
		intfList = in->next;
	    }
	    free(in->intf);
	    free(in);
	    return;
	}
    }
    assert(0 && "Huh? We lost the interface somewhere...");
}

static struct interface* findInterface(String* str)
{
    for (struct interfaceNode* in = intfList; in; in = in->next)
    {
	if (in->intf->str == str)
	{
	    return in->intf;
	}
    }
    return NULL;
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
    char ch = intf->fncurrent(intf);
    if (ch == '-')
    {
	intf->fngetnext(intf);
	return -1;
    }
    else if (ch == '+')
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
    char ch;
    while ((ch = intf->fncurrent(intf)) && isdigit(ch))
    {
	n *= 10;
	n += ch - '0';
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

void __read_S_int64(String* str, int64_t* v)
{
    struct interface* intf = findInterface(str);
    assert(intf && "Expected to find an interface");
    readint64(intf, v);
}

void __read_int32(File* file, int* v)
{
    int64_t n;
    __read_int64(file, &n);
    // We probably should check range?
    *v = (int)n;
}

void __read_S_int32(String* str, int* v)
{
    int64_t n;
    __read_S_int64(str, &n);
    *v = (int)n;
}

static void readchar(struct interface* intf, char* v)
{
    intf->fnpreread(intf);

    *v = intf->fncurrent(intf);
    intf->fngetnext(intf);
}

void __read_chr(File* file, char* v)
{
    if (file->handle >= MaxPascalFiles)
    {
	return;
    }

    struct interface intf;
    initFile(file, &intf);

    readchar(&intf, v);
}

void __read_S_chr(String* str, char* v)
{
    struct interface* intf = findInterface(str);
    assert(intf && "Expected to find an interface");

    readchar(intf, v);
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

void __read_S_real(String* str, double* v)
{
    struct interface* intf = findInterface(str);
    assert(intf && "Expected to find an interface");

    readreal(intf, v);
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

void __read_S_str(String* str, String* v)
{
    struct interface* intf = findInterface(str);
    assert(intf && "Expected to find an interface");

    readstr(intf, v);
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

void __read_S_chars(String* str, char* v)
{
    struct interface* intf = findInterface(str);
    assert(intf && "Expected to find an interface");

    readchars(intf, v);
}
