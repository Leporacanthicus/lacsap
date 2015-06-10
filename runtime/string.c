#include <string.h>
#include "runtime.h"

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
    
    if (start > str->len) 
    {
	len = 0;
    }
    if (start + len > str->len)
    {
	len = str->len - start;
	if (len < 0)
	{
	    len = 0;
	}
    }

    String result = { len, "" };
    memcpy(result.str, &str->str[start], len);
    return result;
}
