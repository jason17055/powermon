#include "perror.h"
#include <windows.h>
#include <stdio.h>
#include <strsafe.h>

void
fatal_win32_error(const TCHAR *function_name)
{
	DWORD error_code = GetLastError();
	TCHAR *buf;
	TCHAR *display_buf;
	size_t display_buf_nchars;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		error_code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(TCHAR *) &buf,
		0, NULL);
	display_buf_nchars = 40 + lstrlen(buf)
		+ lstrlen(function_name);
	display_buf = (TCHAR *) LocalAlloc(LMEM_ZEROINIT,
		display_buf_nchars * sizeof(TCHAR));
	StringCchPrintf(display_buf,
		display_buf_nchars,
		TEXT("%s: %s\n"), function_name, buf);
	
	_fputts(display_buf, stderr);

	LocalFree(display_buf);	
	LocalFree(buf);
	ExitProcess(error_code);
}
