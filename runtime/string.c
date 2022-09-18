#include "runtime.h"
#include <stdbool.h>
#include <string.h>

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
	blen = MaxStringLen - a->len;
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
    for (int i = 0; i < shortest; i++)
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
	len = str->len - (start - 1);
	if (len < 0)
	{
	    len = 0;
	}
    }

    String result = { len, "" };
    memcpy(result.str, &str->str[start - 1], len);
    return result;
}

/* Return "trimmed" string - remove leading and trailing spaces */
String __StrTrim(String* str)
{
    unsigned char* s = str->str;
    unsigned char* end = &str->str[str->len];
    while (s < end && *s <= ' ')
    {
	s++;
    }
    unsigned char* e = end - 1;
    while (e > s && *s <= ' ')
    {
	end--;
    }
    int    len = e - s;
    String result = { len, "" };
    memcpy(result.str, s, len);
    return result;
}

/* Return index of second string in first string */
int __StrIndex(String* str1, String* str2)
{
    bool           found = false;
    unsigned char* s1 = str1->str;
    unsigned char* e1 = &str1->str[str1->len];

    while (s1 < e1)
    {
	unsigned char* s2 = str2->str;
	unsigned char* e2 = &str2->str[str2->len];
	unsigned char* ss1 = s1;

	while (ss1 < e1 && s2 < e2)
	{
	    if (*ss1 == *s2)
	    {
		ss1++;
		s2++;
	    }
	    else
	    {
		break;
	    }
	}
	if (s2 == e2)
	{
	    found = true;
	    break;
	}
	s1++;
    }
    if (found)
    {
	return 1 + (s1 - str1->str);
    }
    return 0;
}
