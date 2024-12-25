#include "runtime.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*******************************************
 * Write Functionality
 *******************************************
 */

void __write_S_init(String* str)
{
    str->len = 0;
}

static void SetStrResult(String* str, int n, const char* buffer)
{
    memcpy(&str->str[str->len], buffer, n);
    str->len += n;
}

void __write_S_int32(String* str, int v, int width)
{
    char buffer[MaxStringLen + 1];
    int  n = snprintf(buffer, sizeof(buffer), "%*d", width, v);
    SetStrResult(str, n, buffer);
}

void __write_int32(File* file, int v, int width)
{
    FILE* f = getFile(file);
    fprintf(f, "%*d", width, v);
}

void __write_S_int64(String* str, int64_t v, int width)
{
    char buffer[MaxStringLen + 1];
    int  n = snprintf(buffer, sizeof(buffer), "%*" PRId64, width, v);
    SetStrResult(str, n, buffer);
}

void __write_int64(File* file, int64_t v, int width)
{
    FILE* f = getFile(file);
    fprintf(f, "%*" PRId64, width, v);
}

void __write_S_real(String* str, double v, int width, int precision)
{
    char buffer[MaxStringLen + 1];
    bool scientific = precision == -1;

    if (scientific)
    {
	if (width == 0)
	{
	    width = 13;
	}
	if (precision == 0)
	{
	    precision = (width > 8) ? width - 7 : 1;
	}
    }

    int n = snprintf(buffer, sizeof(buffer), (scientific) ? "% *.*E" : "%*.*f", width, precision, v);
    SetStrResult(str, n, buffer);
}

void __write_real(File* file, double v, int width, int precision)
{
    FILE* f = getFile(file);
    bool  scientific = precision == -1;

    if (scientific)
    {
	if (width == 0)
	{
	    width = 13;
	}
	precision = (width > 8) ? width - 7 : 1;
    }
    fprintf(f, (scientific) ? "% *.*E" : "%*.*f", width, precision, v);
}

void __write_S_char(String* str, char v, int width)
{
    char buffer[MaxStringLen + 1];
    if (width <= 0)
    {
	width = 1;
    }
    int n = snprintf(buffer, sizeof(buffer), "%*c", width, v);
    SetStrResult(str, n, buffer);
}

void __write_char(File* file, char v, int width)
{
    FILE* f = getFile(file);
    if (width <= 0)
    {
	width = 1;
    }
    fprintf(f, "%*c", width, v);
}

void __write_S_bool(String* str, int v, int width)
{
    char        buffer[MaxStringLen + 1];
    const char* vstr = (v & 1) ? "TRUE" : "FALSE";
    if (width <= 0)
    {
	width = (v) ? 4 : 5;
    }
    int n = snprintf(buffer, sizeof(buffer), "%*s", width, vstr);
    SetStrResult(str, n, buffer);
}

void __write_bool(File* file, int v, int width)
{
    FILE*       f = getFile(file);
    const char* vstr = (v & 1) ? "TRUE" : "FALSE";
    if (width <= 0)
    {
	width = (v) ? 4 : 5;
    }
    fprintf(f, "%*s", width, vstr);
}

void __write_S_chars(String* str, const char* v, int len, int width)
{
    char buffer[MaxStringLen + 1];
    int  n;
    if (width > 0)
    {
	if (len > width)
	{
	    len = width;
	}
	n = snprintf(buffer, sizeof(buffer), "%*.*s", width, len, v);
    }
    else
    {
	n = snprintf(buffer, sizeof(buffer), "%.*s", len, v);
    }
    SetStrResult(str, n, buffer);
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

void __write_S_str(String* str, const String* v, int width)
{
    char buffer[MaxStringLen + 1];
    char s[MaxStringLen + 1];
    int  n;
    memcpy(s, v->str, v->len);
    s[v->len] = 0;
    if (width < v->len)
    {
	n = snprintf(buffer, sizeof(buffer), "%*s", width, s);
    }
    else
    {
	n = snprintf(buffer, sizeof(buffer), "%s", s);
    }
    SetStrResult(str, n, buffer);
}

void __write_str(File* file, const String* v, int width)
{
    FILE* f = getFile(file);
    char  s[MaxStringLen + 1];
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

void __write_S_enum(String* strout, int en, int width, struct EnumToString* e2s)
{
    if (en < 0 || en > e2s->nelem)
    {
	static const char* msg = "Invalid Enum Value";
	__write_S_chars(strout, msg, strlen(msg), width);
    }
    else
    {
	int   offset = e2s->offset[en];
	int   len = e2s->strings[offset];
	char* str = &e2s->strings[offset + 1];
	__write_S_chars(strout, str, len, width);
    }
}

void __write_enum(File* file, int en, int width, struct EnumToString* e2s)
{
    static const char* msg = "Invalid Enum Value";
    if (en < 0 || en > e2s->nelem)
    {
	__write_chars(file, msg, strlen(msg), width);
    }
    else
    {
	int   offset = e2s->offset[en];
	int   len = e2s->strings[offset];
	char* str = &e2s->strings[offset + 1];
	__write_chars(file, str, len, width);
    }
}

void __write_nl(File* file)
{
    FILE* f = getFile(file);
    fputc('\n', f);
}
