#include "runtime.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*******************************************
 * Write Functionality
 *******************************************
 */
void __write_int32(File* file, int v, int width)
{
    FILE* f = getFile(file);
    fprintf(f, "%*d", width, v);
}

void __write_int64(File* file, int64_t v, int width)
{
    FILE* f = getFile(file);
    fprintf(f, "%*" PRId64, width, v);
}

void __write_real(File* file, double v, int width, int precision)
{
    FILE* f = getFile(file);
    if (precision > 0)
    {
	fprintf(f, "%*.*f", width, precision, v);
    }
    else
    {
	if (width == 0)
	{
	    width = 13;
	}
	precision = (width > 8) ? width - 7 : 1;
	fprintf(f, "% *.*E", width, precision, v);
    }
}

void __write_char(File* file, char v, int width)
{
    FILE* f = getFile(file);
    if (width > 0)
    {
	fprintf(f, "%*c", width, v);
    }
    else
    {
	fprintf(f, "%c", v);
    }
}

void __write_bool(File* file, int v, int width)
{
    FILE*       f = getFile(file);
    const char* vstr = (v & 1) ? "TRUE" : "FALSE";
    if (width > 0)
    {
	fprintf(f, "%*s", width, vstr);
    }
    else
    {
	fprintf(f, "%s", vstr);
    }
}

void __write_chars(File* file, const char* v, int len, int width)
{
    FILE* f = getFile(file);
    if (width > 0)
    {
	if (len > width)
	{
	    len = width;
	}
	fprintf(f, "%*.*s", width, len, v);
    }
    else
    {
	fprintf(f, "%.*s", len, v);
    }
}

void __write_str(File* file, const String* v, int width)
{
    FILE* f = getFile(file);
    char  s[256];
    memcpy(s, v->str, v->len);
    s[v->len] = 0;
    if (width < v->len)
    {
	fprintf(f, "%*s", width, s);
    }
    else
    {
	fprintf(f, "%s", s);
    }
}

void __write_nl(File* file)
{
    FILE* f = getFile(file);
    fputc('\n', f);
}
