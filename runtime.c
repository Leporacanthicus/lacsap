#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>

/*******************************************
 * Enum declarations
 *******************************************
 */
/* Max number/size values */
enum
{
    MaxPascalFiles =  1000,
    MaxSetWords    =  16,	/* Must match compiler definition */
    MaxStringLen   =  255,
};

/*******************************************
 * Structure declarations
 *******************************************
 */
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
    int   readAhead;
};

typedef struct 
{
    unsigned int v[MaxSetWords];
} Set;

typedef struct 
{
    unsigned char len;
    unsigned char str[MaxStringLen];
} String;


/*******************************************
 * Local variables
 *******************************************
 */
static struct FileEntry files[MaxPascalFiles];

/*******************************************
 * External variables
 *******************************************
 */
extern File input;
extern File output;

/*******************************************
 * Pascal Starting point
 *******************************************
 */
extern void __PascalMain(void);

/*******************************************
 * File Basics, low level I/O.
 *******************************************
 */
static FILE* getFile(File* f, File* deflt)
{
    if (!f)
    {
	assert(deflt);
	f = deflt;
    }
    if (f->handle < MaxPascalFiles && files[f->handle].inUse)
    {
	return files[f->handle].file;
    }
    return NULL;
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

void __get(File *file)
{
    struct FileEntry *f = 0;
    if (file->handle < MaxPascalFiles && files[file->handle].inUse)
    {
	f = &files[file->handle];
    }
    fread(file->buffer, f->recordSize, 1, f->file);
    f->readAhead = 1;
}

/*******************************************
 * File End of {line,file}
 *******************************************
 */
int __eof(File* file)
{
    if (!file)
    {
	file = &input;
    }
    FILE* f = getFile(file, NULL);
    if (!files[file->handle].readAhead)
    {
	__get(file);
    }
    return !!feof(f);
}

int __eoln(File* file)
{
    if (!file)
    {
	file = &input;
    }
    if (!files[file->handle].readAhead)
    {
	__get(file);
    }
    return !!(*file->buffer == '\n' || __eof(file));
}

/*******************************************
 * File Create/Open/Close functions
 *******************************************
 */
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
    files[i].readAhead = 0;
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
    fprintf(stderr, "Attempt to append file failed\n");
}

void __close(File* f)
{
    if (files[f->handle].inUse && files[f->handle].file != NULL)
    {
	fclose(files[f->handle].file);
	files[f->handle].file = NULL;
	return;
    }
    fprintf(stderr, "Attempt to close file failed\n");
}

/*******************************************
 * Read Functionality
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



/*******************************************
 * Read Functionality
 *******************************************
 */
static void skip_spaces(File* file)
{
    while(isspace(*file->buffer) && !__eof(file))
    {
	__get(file);
    }
}

static int get_sign(File* file)
{
    
    if (*file->buffer == '-')
    {
	__get(file);
	return -1;
    }
    else if (*file->buffer == '+')
    {
	__get(file);
    }
    return 1;
}

// Turn an exponent into a multiplier value. 
static double exponent_to_multi(int exponent)
{
    double m = 1.0;
    int oneover = 0;
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
	return 1.0/m;
    }
    return m;
}

void __read_int(File* file, int* v)
{
    int n = 0;
    int sign;

    if (!file)
    {
	file = &input;
    }
    if (file->handle >= MaxPascalFiles)
    {
	return;
    }
    if (!files[file->handle].readAhead)
    {
	__get(file);
    }
    skip_spaces(file);
    sign = get_sign(file);
    while(isdigit(*file->buffer) && !__eoln(file))
    {
	n *= 10;
	n += (*file->buffer) - '0';
	__get(file);
    }
    *v = n * sign;
}

void __read_chr(File* file, char* v)
{
    if (!file)
    {
	file = &input;
    }

    if (file->handle >= MaxPascalFiles)
    {
	return;
    }

    if (!files[file->handle].readAhead)
    {
	__get(file);
    }

    *v = *file->buffer;
    __get(file);
}

void __read_real(File* file, double* v)
{ 
    double n = 0;
    double divisor = 1.0;
    double multiplicand = 1.0;
    int exponent = 0;
    int sign;
    if (!file)
    {
	file = &input;
    }
    if (file->handle >= MaxPascalFiles)
    {
	return;
    }
    if (!files[file->handle].readAhead)
    {
	__get(file);
    }

    skip_spaces(file);
    sign = get_sign(file);
    
    while(isdigit(*file->buffer) && !__eoln(file))
    {
	n *= 10.0;
	n += (*file->buffer) - '0';
	__get(file);
    }
    if (*file->buffer == '.')
    {
	__get(file);
	while(isdigit(*file->buffer) && !__eoln(file))
	{
	    n *= 10.0;
	    n += (*file->buffer) - '0';
	    __get(file);
	    divisor *= 10.0;
	}
    }
    if (*file->buffer == 'e' || *file->buffer == 'E')
    {
	__get(file);
	int expsign = get_sign(file);
	while(isdigit(*file->buffer) && !__eoln(file))
	{
	    exponent *= 10;
	    exponent += (*file->buffer) - '0';
	    __get(file);
	}
	exponent *= expsign;
	multiplicand = exponent_to_multi(exponent);
    }
    n = n * sign / divisor * multiplicand;
    *v = n;
}

void __read_nl(File* file)
{
    if (!file)
    {
	file = &input;
    }
    if (!files[file->handle].readAhead)
    {
	__get(file);
    }
    while(*file->buffer != '\n' && !__eof(file))
    {
	__get(file);
    }
    files[file->handle].readAhead = 0;
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

void __read_str(File* file, String* val)
{
    char buffer[256];
    size_t count = 0;
    if (!file)
    {
	file = &input;
    }
    if (file->handle >= MaxPascalFiles)
    {
	return;
    }
    if (!files[file->handle].readAhead)
    {
	__get(file);
    }
    while(!__eoln(file) && count < sizeof(buffer))
    {
	buffer[count++] = *file->buffer;
	__get(file);
    }
    val->len = count;
    memcpy(val->str, buffer, count);
}

/*******************************************
 * InitFiles
 *******************************************
 */
static void InitFiles()
{
    for(int i = 0; i < MaxPascalFiles; i++)
    {
	files[i].inUse = 0;
    }

    __assign(&input, "INPUT", 1, 1);
    __assign(&output, "OUTPUT", 1, 1);

    files[input.handle].file = stdin;
    files[output.handle].file = stdout;
}

/*******************************************
 * Memory allocation functions
 *******************************************
 */
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

/*******************************************
 * Set functions 
 *******************************************
 */
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

/*******************************************
 * String functions 
 *******************************************
 */
void __StrConcat(String* res, String* a, String* b)
{
    int blen = b->len;
    int total = a->len + blen;
    if (total > MaxStringLen)
    {
	total = MaxStringLen;
	blen = MaxStringLen-a->len;
    }
    res->len = total;

    memcpy(res->str, a->str, a->len);
    memcpy(&res->str[a->len], b->str, blen);
}

/* Assign string b to string a. */
void __StrAssign(String* a, String* b)
{
    a->len = b->len;
    memcpy(a->str, b->str, a->len);
}

/* Return >0 if a is greater than b, 
 * Return <0 if a is less than b. 
 * Return 0 if a == b. 
 */ 
int __StrCompare(String* a, String* b)
{
    int alen = a->len;
    int blen = b->len;
    int shortest = alen;
    if (blen < shortest)
    {
	shortest = blen;
    }
    for(int i = 0; i < shortest; i++)
    {
	if (a->str[i] != b->str[i])
	{
	    return a->str[i] - b->str[i];
	}
    }
    return alen - blen;
}

/* Return substring of input */
String __StrCopy(String* str, int start, int len)
{
    assert(start >= 1);
    assert(len >= 0);
    
    String result;
    if (start > str->len) 
    {
	len = 0;
    }
    if (start + len > str->len)
    {
	len = str->len - start;
    }
    result.len = len;
    memcpy(result.str, &str->str[start], len);
    return result;
}

/* Return >0 if a is greater than b, 
 * Return <0 if a is less than b. 
 * Return 0 if a == b. 
 */ 
int __ArrCompare(char* a, char* b, int size)
{
    for(int i = 0; i < size; i++)
    {
	if (a[i] != b[i])
	{
	    return a[i] - b[i];
	}
    }
    return 0;
}

long __Clock(void)
{
    return clock();
}

long __Panic(const char* msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(11);
}

int main()
{
    InitFiles();
    __PascalMain();
    return 0;
}
