#include <string.h>
#define __USE_POSIX 1
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "runtime.h"

struct FileEntry files[MaxPascalFiles];

/*******************************************
 * InitFiles
 *******************************************
 */
void InitFiles()
{
    for(int i = 0; i < MaxPascalFiles; i++)
    {
	files[i].inUse = 0;
    }

    __assign(&input, "INPUT", 1, 1);
    __assign(&output, "OUTPUT", 1, 1);

    files[input.handle].file = stdin;
    files[input.handle].isText |= 2 * (!!isatty(fileno(stdin)));
    files[output.handle].file = stdout;
}

/*******************************************
 * File assign
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
    if (isText)
    {
	recordSize = 1024;
    }
    f->handle = i;
    f->buffer = malloc(recordSize);
    files[i].recordSize = recordSize;
    files[i].inUse = 1;
    files[i].name = malloc(strlen(name)+1);
    files[i].fileData = f;
    files[i].isText = isText;
    files[i].readAhead = 0;
    files[i].readPos = 0;
    files[i].bufferSize = 0;
    strcpy(files[i].name, name);
}

