#include "launch_in_session.h"
#include <windows.h>
#include <tlhelp32.h>

//  based on http://www.codeproject.com/Articles/18367/Launch-your-application-in-Vista-under-the-local-s

static DWORD
find_process(const char *targetExeName, DWORD targetSession)
{
	PROCESSENTRY32 pe32 = {0};

	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE) {
		return 0;
	}

	pe32.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(hSnap, &pe32)) {
		CloseHandle(hSnap);
		return 0;
	}

	do
	{
		if (_stricmp(pe32.szExeFile, targetExeName) == 0) {
			DWORD sessId = 0;
			if (ProcessIdToSessionId(pe32.th32ProcessID, &sessId)
				&& sessId == targetSession)
			{
				/* found target process */
				CloseHandle(hSnap);
				return pe32.th32ProcessID;
			}
		}
	} while (Process32Next(hSnap, &pe32));

	CloseHandle(hSnap);
	return 0;
}

static HANDLE
make_token_from_pid(DWORD winLogonPid)
{
	HANDLE hProcess;
	HANDLE hTokenOrig;
	LUID debugPriv;
	TOKEN_PRIVILEGES tp = {0};
	HANDLE hTokenDup;

	hProcess = OpenProcess(MAXIMUM_ALLOWED, FALSE, winLogonPid);
	if (hProcess == NULL) {
		// failed
		return NULL;
	}

	if (!OpenProcessToken(hProcess,
		TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY | TOKEN_DUPLICATE |
			TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_SESSIONID |
			TOKEN_READ | TOKEN_WRITE,
			&hTokenOrig))
	{
		// failed
		return NULL;
	}

	if (!DuplicateTokenEx(hTokenOrig,
		MAXIMUM_ALLOWED,
		NULL,
		SecurityIdentification, /* impersonation level */
		TokenPrimary,           /* token type */
		&hTokenDup
		))
	{
		// failed
		return FALSE;
	}

	if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &debugPriv))
	{
		// failed
		return NULL;
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = debugPriv;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	/* adjust token privilege */
	SetTokenInformation(hTokenDup,
		TokenSessionId, (void*) 1, sizeof(DWORD));

	if (!AdjustTokenPrivileges(hTokenDup, FALSE, &tp, sizeof(TOKEN_PRIVILEGES),
		(TOKEN_PRIVILEGES*) NULL,
		NULL))
	{
		// failed
		return NULL;
	}

	CloseHandle(hProcess);
	CloseHandle(hTokenOrig);

	return hTokenDup;
}

BOOL
launch_in_session(TCHAR *processExe)
{
	DWORD winLogonPid = find_process("winlogon.exe", 1);
	HANDLE hTokenDup = make_token_from_pid(winLogonPid);
	STARTUPINFO si = {0};
	PROCESS_INFORMATION pi = {0};

	if (hTokenDup == NULL) {
		return FALSE;
	}

	si.cb = sizeof(STARTUPINFO);
	si.lpDesktop = TEXT("winsta0\\winlogon");

	CreateProcessAsUser(
		hTokenDup,  /* client access token */
		processExe,
		NULL,
		NULL,
		NULL,
		TRUE,       /* handles are inheritable */
		0,
		NULL,
		NULL,
		&si,        /* STARTUPINFO structure */
		&pi         /* receives information about new process */
		);

	CloseHandle(hTokenDup);

	return TRUE;
}
