#include <stddef.h>	// for size_t

typedef unsigned short	wchar_t;
#define _WCHAR_T_DEFINED


size_t wcslen(const wchar_t* string);
int wcsncmp(const wchar_t* string1, const wchar_t* string2, size_t count);


size_t wcslen(const wchar_t* string)
{
	const wchar_t* cur_chr;
	
	while(*cur_chr != 0x0000)
		cur_chr ++;
	
	return cur_chr - string;
}

int wcsncmp(const wchar_t* string1, const wchar_t* string2, size_t count)
{
	while(count)
	{
		if (*string1 < *string2)
			return -1;
		else if (*string1 > *string2)
			return +1;
		else if (*string1 == 0x0000)
			break;
		string1 ++;
		string2 ++;
		count --;
	}
	
	return 0;
}
