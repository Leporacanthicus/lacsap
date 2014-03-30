#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    MaxPascalFiles =  1000,
    MaxSetWords    =  16,
};

/* Note: This should match the definition in the compiler, or weirdness happens! */
typedef struct File
{
    int   handle;
    char* buffer;
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


typedef struct 
{
    unsigned int v[MaxSetWords];
} Set;

static struct FileEntry files[MaxPascalFiles];

extern void __PascalMain(void);

static FILE* getFile(File* f, FILE* deflt)
{
    if (f)
    {
	if (f->handle < MaxPascalFiles && files[f->handle].inUse)
	{
	    return files[f->handle].file;
	}
	return NULL;
    }
    return deflt;
} 

int __eof(File* file)
{
    FILE* f = getFile(file, stdin);
    return !!feof(f);
}

int __eoln(File* file)
{
    return !!(*file->buffer == '\n' || __eof(file));
}

void __get(File *file)
{
    struct FileEntry *f = 0;
    if (file->handle < MaxPascalFiles && files[file->handle].inUse)
    {
	f = &files[file->handle];
    }
    fread(file->buffer, f->recordSize, 1, f->file);
}

void __put(File *file)
{
    struct FileEntry *f = 0;
    if (file->handle < MaxPascalFiles && files[file->handle].inUse)
    {
	f = &files[file->handle];
    }
    fwrite(file->buffer, f->recordSize, 1, f->file);
}


void __assign(File* f, char* name, int recordSize, int isText)
{
    int i;
    for(i = 0; i < MaxPascalFiles && files[i].inUse; i++)
	;
    if (i == MaxPascalFiles)
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
	{
	    __get(f);
	    return;
	}
    }
    fprintf(stderr, "Attempt to open file failed\n");
}

void __rewrite(File* f)
{
    if (files[f->handle].inUse && files[f->handle].file == NULL)
    {
	files[f->handle].file = fopen(files[f->handle].name, "w");
	if (files[f->handle].file)
	{
	    return;
	}
    }
    fprintf(stderr, "Attempt to open file failed\n");
}

void __append(File* f)
{
    if (files[f->handle].inUse && files[f->handle].file == NULL)
    {
	files[f->handle].file = fopen(files[f->handle].name, "a");
	if (files[f->handle].file)
	{
	    return;
	}
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

void __write_bin(File* file, void *val)
{
    struct FileEntry *f = 0;
    if (file->handle < MaxPascalFiles && files[file->handle].inUse)
    {
	f = &files[file->handle];
    }
    if (!f)
    {
	fprintf(stderr, "Invalid file used for write binary file\n");
	return;
    }
    memcpy(file->buffer, val, f->recordSize); 
    __put(file);
}

void __read_int(File* file, int* v)
{
    FILE* f = getFile(file, stdin);
    ungetc(*file->buffer, f);
    fscanf(f, "%d", v);
    __get(file);
}

void __read_chr(File* file, char* v)
{
    *v = *file->buffer;
    __get(file);
}

void __read_real(File* file, double* v)
{ 
    FILE* f = getFile(file, stdin);
    
    ungetc(*file->buffer, f);
    fscanf(f, "%lf", v);
    __get(file);
}

void __read_nl(File* file)
{
    while(*file->buffer != '\n' && !__eof(file))
	__get(file);
}

void __read_bin(File* file, void *val)
{
    struct FileEntry *f = 0;
    if (file->handle < MaxPascalFiles && files[file->handle].inUse)
    {
	f = &files[file->handle];
    }
    if (!f)
    {
	fprintf(stderr, "Invalid file used for read binary\n");
	return;
    }
    memcpy(val, file->buffer, f->recordSize);
    __get(file);
}

static void InitFiles()
{
    for(int i = 0; i < MaxPascalFiles; i++)
    {
	files[i].inUse = 0;
    }
}

/* Memory allocation functions */
void* __new(int size)
{
    return malloc(size);
}

void __dispose(void* ptr)
{
    free(ptr);
}

double __random(void)
{
    return rand() / (double)RAND_MAX;
}

/* Set functions */
int __SetEqual(Set *a, Set *b)
{
    return !memcmp(a->v, b->v, sizeof(*a));
}

Set __SetUnion(Set *a, Set *b)
{
    Set res;
    for(int i = 0; i < MaxSetWords; i++)
    {
	res.v[i] = a->v[i] | b->v[i]; 
    }
    return res;
}

Set __SetDiff(Set *a, Set *b)
{
    Set res;
    for(int i = 0; i < MaxSetWords; i++)
    {
	res.v[i] = a->v[i] & ~b->v[i]; 
    }
    return res;
}

Set __SetIntersect(Set *a, Set *b)
{
    Set res;
    for(int i = 0; i < MaxSetWords; i++)
    {
	res.v[i] = a->v[i] & b->v[i]; 
    }
    return res;
}

/* Check if all values in a are in set b. */
int __SetContains(Set *a, Set *b)
{
    for(int i = 0; i < MaxSetWords; i++)
    {
	if ((a->v[i] & b->v[i]) != a->v[i])
	    return 0;
    }
    return 1;
}

int main()
{
    InitFiles();
    __PascalMain();
}
