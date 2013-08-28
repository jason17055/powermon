#include "logger.h"
#include "config.h"
#include "printf.h"
#include <stdio.h>
#include <time.h>


void
logger_message(const TCHAR *text)
{
	TCHAR datestr[100];
	time_t t;
	struct tm *tm;
	FILE *fp;

	t = time(NULL);
	tm = localtime(&t);
	_tcsftime(datestr, 100, _TEXT("%Y-%m-%d %H:%M:%S"), tm);

	fp = _tfopen(LOGFILE_PATH, _TEXT("at"));
	if (fp != NULL)
	{
		_fputts(datestr, fp);
		_fputts(_TEXT(" "), fp);
		_fputts(text, fp);
		_fputts(_TEXT("\n"), fp);
		fclose(fp);
	}
}

void
logger_printf(const TCHAR *format, ...)
{
	va_list ap;
	TCHAR *tmp;

	va_start(ap, format);
	tmp = tcsdup_vprintf(format, ap);
	va_end(ap);

	logger_message(tmp);
	free(tmp);
}
