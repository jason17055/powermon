#ifndef LOGGER_H
#define LOGGER_H

#include <tchar.h>
#include <stdarg.h>

void logger_message(const TCHAR *text);
void logger_printf(const TCHAR *format, ...);

#endif /* LOGGER_H */
