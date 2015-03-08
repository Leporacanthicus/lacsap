#include <stdio.h>
#include <assert.h>

/*******************************************
 * Enum declarations
 *******************************************
 */
/* Max number/size values */
enum
{
    MaxPascalFiles =  1000,
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
    unsigned char len;
    unsigned char str[MaxStringLen];
} String;

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

int __get(File *file);
void __put(File *file);
int __eof(File* file);
int __eoln(File* file);
void __assign(File* f, char* name, int recordSize, int isText);
