#include "printf.h"
#include <stdlib.h>

TCHAR *
tcsdup_vprintf(const TCHAR *format, va_list args)
{
	/* initial guess at buffer length */
	int n, size = 100;
	TCHAR *p, *np;

	if ((p = malloc(size * sizeof(TCHAR))) == NULL)
		return NULL;

	for (;;)
	{
		/* try to print in the allocated space */
		n = _vsntprintf(p, size, format, args);
		/* if that worked, return the string. */
		if (n > -1 && n < size)
			return p;
		/* else try again with more space */
		size *= 2;
		if ((np = realloc(p, size * sizeof(TCHAR))) == NULL) {
			free(p);
			return NULL;
		} else {
			p = np;
		}
	}
}
