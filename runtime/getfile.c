#include "runtime.h"

/*******************************************
 * File Basics, low level I/O.
 *******************************************
 */
FILE* getFile(File* f, File* deflt)
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

