/*
 * This is the file that is being linked when compiling under posix
 */
#include <stdlib.h>

void *ptpd_wrap_malloc(int size)
{
	return malloc(size);
}

void *ptpd_wrap_memset(void *s, int c, int n)
{
	return memset(s, c, n);
}
