#include <string.h>
#define __USE_POSIX 1
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "runtime.h"

struct FileEntry files[MaxPascalFiles] = { };

/*******************************************
 * InitFiles
 *******************************************
 */
void InitFiles()
{
    __assign(&input, "INPUT");
    __assign(&output, "OUTPUT");

    files[input.handle].file = stdin;
    input.isText |= (bool)(2 * (!!isatty(fileno(stdin))));
    files[output.handle].file = stdout;
}

/*******************************************
 * File assign
 *******************************************
 */
void __assign(File* f, char* name)
{
    int i;
    for(i = 0; i < MaxPascalFiles && files[i].inUse; i++)
	;
    if (i == MaxPascalFiles)
    {
	fprintf(stderr, "No free files... Exiting\n");
	exit(1);
    }
    if (f->isText)
    {
	f->recordSize = 1024;
    }
    f->handle = i;
    f->buffer = malloc(f->recordSize);
    files[i].inUse = 1;
    files[i].name = malloc(strlen(name)+1);
    files[i].fileData = f;
    files[i].readAhead = 0;
    files[i].readPos = 0;
    files[i].bufferSize = 0;
    strcpy(files[i].name, name);
}

/*******************************************
 * File assign for unnamed file
 *******************************************
 */
void __assign_unnamed(File* f)
{
    char name[] = "lacsap_tmp_file_NNNNNN";
    static int n = 0;
    n++;
    n %= MaxPascalFiles;
    size_t pos = strlen(name) - 1;
    for(int i = 0; i < 6; i++)
    {
	name[pos] = '0' + (n % 10);
	n /= 10;
	pos--;
    }
    __assign(f, name);
}
