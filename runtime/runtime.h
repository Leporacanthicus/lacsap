#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

/*******************************************
 * Enum declarations
 *******************************************
 */
/* Max number/size values */
enum
{
    MaxPascalFiles = 1000,
    MaxStringLen = 255,
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
    int   recordSize;
    int   isText;
} File;

struct FileEntry
{
    File* fileData;
    FILE* file;
    char* name;
    int   inUse;
    int   readAhead;
    int   readPos;
    int   bufferSize;
};

typedef struct
{
    unsigned char len;
    unsigned char str[MaxStringLen + 1];
} String;

struct TimeStamp
{
    bool DateValid;
    bool TimeValid;
    int  Year;
    int  Month;
    int  Day;
    int  Hour;
    int  Minute;
    int  Second;
    int  MicroSecond;
};

struct BindingType
{
    bool   Bound;
    String Name;
};

struct Complex
{
    double r;
    double i;
};

// Used for printing enum values.
struct EnumToString
{
    char* strings; // Stores as [len][string1][len][string2] ...
    int   nelem;
    int   offset[1]; // Offset to string1, string2, etc. Note variable length field.
};

/*******************************************
 * Local variables
 *******************************************
 */
extern struct FileEntry files[];

/*******************************************
 * External variables
 *******************************************
 */
extern File input;
extern File output;

/*******************************************
 * Function declarations
 *******************************************
 */
void InitFiles();
void SetupFile(File* f, int recSize, int isText);
void FileError(const char* op);

/*******************************************
 * File Basics, low level I/O.
 *******************************************
 */
static inline FILE* getFile(File* f)
{
    if (f->handle < MaxPascalFiles && files[f->handle].inUse)
    {
	return files[f->handle].file;
    }
    return NULL;
}

int  __get(File* file);
void __put(File* file);
int  __eof(File* file);
int  __eoln(File* file);
void __assign(File* f, char* name);
void __assign_unnamed(File* f);
