#include "runtime.h"
#include <stdbool.h>
#include <time.h>

void __gettimestamp(struct TimeStamp* ts)
{
    assert(ts && "expect non-null ts argument");
    time_t    t = time(NULL);
    struct tm tt = *localtime(&t);
    ts->DateValid = true;
    ts->TimeValid = true;
    ts->Year = tt.tm_year + 1900;
    ts->Month = tt.tm_mon + 1;
    ts->Day = tt.tm_mday;
    ts->Hour = tt.tm_hour;
    ts->Minute = tt.tm_min;
    ts->Second = tt.tm_sec;
}

// Length: 8+nul = 9
void __Time(struct TimeStamp* ts, char* dest)
{
    sprintf(dest, "%02d:%02d:%02d", ts->Hour, ts->Minute, ts->Second);
}

// Lenght: 10+nul = 11
void __Date(struct TimeStamp* ts, char* dest)
{
    sprintf(dest, "%04d-%02d-%02d", ts->Year, ts->Month, ts->Day);
}
