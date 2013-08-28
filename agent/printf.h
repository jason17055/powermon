#ifndef PRINTF_H
#define PRINTF_H

#include <tchar.h>
#include <stdarg.h>

TCHAR *tcsdup_vprintf(const TCHAR *format, va_list args);

#endif /* PRINTF_H */
