#include <stdio.h>
#include <string.h>
#include "runtime.h"

/*******************************************
 * Write Functionality
 *******************************************
 */
void __write_int(File* file, int v, int width)
{
    FILE* f = getFile(file, &output);
    fprintf(f, "%*d", width, v);
}

void __write_int64(File* file, long v, int width)
{
    FILE* f = getFile(file, &output);
    fprintf(f, "%*ld", width, v);
}

void __write_real(File* file, double v, int width, int precision)
{
    FILE* f = getFile(file, &output);
    if (precision > 0)
    {
	fprintf(f, "%*.*f", width, precision, v);
    }
    else
    {
	fprintf(f, "%*E", width, v);
    }
}

void __write_char(File* file, char v, int width)
{
    FILE* f = getFile(file, &output);
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
    FILE* f = getFile(file, &output);
    const char* vstr = (v)?"TRUE":"FALSE";
    if (width > 0)
    {
	fprintf(f, "%*s", width, vstr);
    }
    else
    {
	fprintf(f, "%s", vstr);
    }
}   
 
void __write_chars(File* file, const char* v, int width)
{
    FILE* f = getFile(file, &output);
    if (width > 0)
    {
	fprintf(f, "%*s", width, v);
    }
    else
    {
	fprintf(f, "%s", v);
    }
}

void __write_str(File* file, const String* v, int width)
{
    FILE* f = getFile(file, &output);
    char s[256];
    memcpy(s, v->str, v->len);
    s[v->len] = 0;
    if (width < v->len)
    {
	fprintf(f, "%*s", width, s);
    }
    else
    {
	fprintf(f, "%s",s);
    }
}


void __write_nl(File* file)
{
    FILE* f = getFile(file, &output);
    fputc('\n', f);
}

