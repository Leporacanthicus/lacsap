#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PASCAL_FILES 1000

/* Note: This should match the definition in the compiler, or weirdness happens! */
typedef struct File
{
    int   handle;
    void* buffer;
} File;

struct FileEntry
{
    File  fileData;
    FILE* file;
    char* name;
    int   isText;
    int   inUse;
    int   recordSize;
};

static struct FileEntry files[MAX_PASCAL_FILES];

extern void __PascalMain(void);


void __assign(File* f, char* name, int recordSize, int isText)
{
    int i;
    for(i = 0; i < MAX_PASCAL_FILES && files[i].inUse; i++)
	;
    if (i == MAX_PASCAL_FILES)
    {
	fprintf(stderr, "No free files... Exiting\n");
	exit(1);
    }
    f->handle = i;
    f->buffer = malloc(recordSize);
    files[i].recordSize = recordSize;
    files[i].inUse = 1;
    files[i].name = malloc(strlen(name)+1);
    files[i].fileData = *f;
    files[i].isText = isText;
    strcpy(files[i].name, name);
}

void __reset(File* f)
{
    if (files[f->handle].inUse && files[f->handle].file == NULL)
    {
	files[f->handle].file = fopen(files[f->handle].name, "r");
	if (files[f->handle].file)
	    return;
    }
    fprintf(stderr, "Attempt to open file failed\n");
}

void __rewrite(File* f)
{
    if (files[f->handle].inUse && files[f->handle].file == NULL)
    {
	files[f->handle].file = fopen(files[f->handle].name, "w");
	if (files[f->handle].file)
	    return;
    }
    fprintf(stderr, "Attempt to open file failed\n");
}

void __append(File* f)
{
    if (files[f->handle].inUse && files[f->handle].file == NULL)
    {
	files[f->handle].file = fopen(files[f->handle].name, "a");
	if (files[f->handle].file)
	    return;
    }
    fprintf(stderr, "Attempt to open file failed\n");
}

void __close(File* f)
{
    if (files[f->handle].inUse && files[f->handle].file != NULL)
    {
	fclose(files[f->handle].file);
	files[f->handle].file = NULL;
	return;
    }
    fprintf(stderr, "Attempt to open file failed\n");
}

static FILE* getFile(File* f, FILE* deflt)
{
    if (f)
    {
	if (f->handle < MAX_PASCAL_FILES && files[f->handle].inUse)
	{
	    return files[f->handle].file;
	}
	return NULL;
    }
    return deflt;
} 

void __write_int(File* file, int v, int width)
{
    FILE* f = getFile(file, stdout);
    fprintf(f, "%*d", width, v);
}

void __write_real(File* file, double v, int width, int precision)
{
    FILE* f = getFile(file, stdout);
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
    FILE* f = getFile(file, stdout);
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
    FILE* f = getFile(file, stdout);
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
 
void __write_str(File* file, const char* v, int width)
{
    FILE* f = getFile(file, stdout);
    if (width > 0)
    {
	fprintf(f, "%*s", width, v);
    }
    else
    {
	fprintf(f, "%s", v);
    }
}

void __write_nl(File* file)
{
    FILE* f = getFile(file, stdout);
    fputc('\n', f);
}

void __read_int(File* file, int* v)
{
    FILE* f = getFile(file, stdin);
    fscanf(f, "%d", v);
}

void __read_chr(File* file, char* v)
{
    FILE* f = getFile(file, stdin);
    *v = getc(f);
}

void __read_real(File* file, double* v)
{ 
    FILE* f = getFile(file, stdin);
    fscanf(f, "%lf", v);
}

void __read_nl(File* file)
{
    FILE* f = getFile(file, stdin);
    int ch;
    while((ch = fgetc(f)) != '\n' && ch != EOF)
	;
}

int __eof(File* file)
{
    FILE* f = getFile(file, stdin);
    return !!feof(f);
}

int __eoln(File* file)
{
    FILE* f = getFile(file, stdin);
    int ch = getc(f);
    ungetc(ch, f);
    return !!(ch == '\n' || ch == EOF);
}

void* __new(int size)
{
    return malloc(size);
}

void __dispose(void* ptr)
{
    free(ptr);
}

static void InitFiles()
{
    for(int i = 0; i < MAX_PASCAL_FILES; i++)
    {
	files[i].inUse = 0;
    }
}

int main()
{
    InitFiles();
    __PascalMain();
}
