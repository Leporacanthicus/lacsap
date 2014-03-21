#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PASCAL_FILES 1000

/* Note: This should match the definition in the compiler, or weirdness happens! */
typedef struct File
{
    int   handle;
    void *buffer;
} File;

struct FileEntry
{
    File  fileData;
    FILE *file;
    char *name;
    int   isText;
    int   inUse;
    int   recordSize;
};

static struct FileEntry files[MAX_PASCAL_FILES];

extern void __PascalMain(void);


void __assign(File *f, char *name, int recordSize, int isText)
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

void __reset(File *f)
{
    if (files[f->handle].inUse && files[f->handle].file == NULL)
    {
	fprintf(stderr, "Open file: %s\n", files[f->handle].name);
	files[f->handle].file = fopen(files[f->handle].name, "r");
	if (files[f->handle].file)
	    return;
    }
    fprintf(stderr, "Attempt to open file failed\n");
}

void __close(File *f)
{
    if (files[f->handle].inUse && files[f->handle].file != NULL)
    {
	fclose(files[f->handle].file);
	files[f->handle].file = NULL;
	return;
    }
    fprintf(stderr, "Attempt to open file failed\n");
}

static FILE* getFile(File *f)
{
    if (f)
    {
	if (f->handle < MAX_PASCAL_FILES && files[f->handle].inUse)
	{
	    return files[f->handle].file;
	}
	return NULL;
    }
    return stdin;
} 

void __write_int(int v, int width)
{
    printf("%*d", width, v);
}

void __write_real(double v, int width, int precision)
{
    if (precision > 0)
    {
	printf("%*.*f", width, precision, v);
    }
    else
    {
	printf("%*E", width, v);
    }
}

void __write_char(char v, int width)
{
    if (width > 0)
    {
	printf("%*c", width, v);
    }
    else
    {
	printf("%c", v);
    }
}    

void __write_str(const char* v, int width)
{
    if (width > 0)
    {
	printf("%*s", width, v);
    }
    else
    {
	printf("%s", v);
    }
}

void __read_int(File* file, int* v)
{
    FILE *f = getFile(file);
    fscanf(f, "%d", v);
}

void __read_real(File* file, double* v)
{
    FILE *f = getFile(file);
    fscanf(f, "%lf", v);
}

void __read_nl(File *file)
{
    FILE *f = getFile(file);
    while(fgetc(f) != '\n')
	;
}

void __write_nl(void)
{
    putchar('\n');
}

void* __new(int size)
{
    return malloc(size);
}

void __dispose(void *ptr)
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
