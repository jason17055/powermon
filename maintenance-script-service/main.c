#include "main.h"
#include "config.h"
#include "install.h"
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <time.h>
#include <wtsapi32.h>

static SERVICE_STATUS status = {0};
static SERVICE_STATUS_HANDLE statusHandle = NULL;
static HANDLE stopEvent = NULL;


static void
SvcReportEvent(const TCHAR *message)
{
}

static void
report_status(DWORD currentState, DWORD exitCode, DWORD waitHint)
{
	static DWORD checkPoint = 1;

	status.dwCurrentState = currentState;
	status.dwWin32ExitCode = exitCode;
	status.dwWaitHint = waitHint;

	if (currentState == SERVICE_START_PENDING)
		status.dwControlsAccepted = 0;
	else
		status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	if (currentState == SERVICE_RUNNING || currentState == SERVICE_STOPPED)
	{
		status.dwCheckPoint = 0;
	}
	else
	{
		status.dwCheckPoint++;
	}

	/* actually report the status */
	SetServiceStatus(statusHandle, &status);
}

static BOOL
check_for_active_sessions(void)
{
	WTS_SESSION_INFO *session_info;
	DWORD session_info_count;
	BOOL rv;
	BOOL found_active_session = FALSE;

	rv = WTSEnumerateSessions(
			WTS_CURRENT_SERVER_HANDLE,
			0, /* Reserved */
			1, /* Version */
			&session_info,
			&session_info_count);
	if (!rv)
	{
		logger_printf(TEXT("WTSEnumerateSessions: error"));
		return FALSE;
	}

	if (session_info_count > 0)
	{
		size_t i;
		for (i = 0; i < session_info_count; i++)
		{
			logger_printf(TEXT("session %d \"%s\" is %d"),
				session_info[i].SessionId,
				session_info[i].pWinStationName,
				session_info[i].State);

			{
				TCHAR *pBuffer = NULL;
				DWORD bytesReturned;
				if (WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, session_info[i].SessionId,
						WTSUserName, &pBuffer, &bytesReturned))
				{
					logger_printf(TEXT("  username: %s"), pBuffer);
					if (_tcscmp(TEXT("Student-PAC"), pBuffer) == 0)
					{
						logger_printf(TEXT("  this username is EXEMPT"));
					}
					else if (session_info[i].State == WTSActive
						&& _tcscmp(TEXT("Console"), session_info[i].pWinStationName) == 0
						&& _tcslen(pBuffer) > 0)
					{
						logger_printf(TEXT("  this session is active"));
						found_active_session = TRUE;
					}
					WTSFreeMemory(pBuffer);
					pBuffer = NULL;
				}
			}

		}
	}
	else
	{
		logger_printf(TEXT("no sessions found"));
	}

	WTSFreeMemory(session_info);

	{
		LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
		if (GetLastInputInfo(&lii))
		{
			size_t elapsed = GetTickCount() - lii.dwTime;
			logger_printf(TEXT("Note: %d seconds since last input"), elapsed / 1000);
		//	if (elapsed / 1000 < 300 && lii.dwTime / 1000 > 300)
		//	{
		//		return TRUE;
		//	}
		}
	}

	logger_printf(TEXT("found active session == %s"),
			found_active_session ? TEXT("true") : TEXT("false"));
	return found_active_session;
}

static void
launch_status_window(const TCHAR *status_window_cmd)
{
	STARTUPINFO si = { sizeof(STARTUPINFO) };
	PROCESS_INFORMATION pi = {0};
	BOOL rv;

	si.lpDesktop = TEXT("WinSta0\\WinLogon");

	rv = CreateProcess(
		NULL,
		(TCHAR*) status_window_cmd,
		NULL, /* lpProcessAttributes */
		NULL, /* lpThreadAttributes */
		FALSE, /* bInheritHandles */
		0, /* dwCreationFlags */
		NULL, /* lpEnvironment */
		NULL, /* lpCurrentDirectory */
		&si,
		&pi);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

static void
create_process(const TCHAR *script)
{
	STARTUPINFO si = { sizeof(STARTUPINFO) };
	PROCESS_INFORMATION pi = {0};
	BOOL rv;

	TCHAR cmdline[MAX_PATH + 40];
	_sntprintf(cmdline, MAX_PATH + 40, TEXT("c:\\windows\\system32\\cmd.exe /C %s"), script);
	
	rv = CreateProcess(
		NULL,
		cmdline,
		NULL, /* lpProcessAttributes */
		NULL, /* lpThreadAttributes */
		FALSE, /* bInheritHandles */
		0, /* dwCreationFlags */
		NULL, /* lpEnvironment */
		NULL, /* lpCurrentDirectory */
		&si,
		&pi);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

}

typedef struct MyParamsStruct
{
	TCHAR *script;
	unsigned int delay;
	unsigned int hour;
	TCHAR *status_window_cmd;
} MyParams;

static unsigned int
get_uint_parameter(HKEY hkey, const TCHAR *param_name, unsigned int def_value)
{
	DWORD tmp;
	DWORD dwSize = sizeof(tmp);
	LONG rv;

	rv = RegQueryValueEx(hkey, param_name,
		NULL, NULL,
		(BYTE *) &tmp,
		&dwSize);
	if (rv == ERROR_SUCCESS && dwSize == sizeof(tmp))
	{
		return tmp;
	}
	else
	{
		return def_value;
	}
}

static TCHAR *
get_string_parameter(HKEY hkey, const TCHAR *param_name)
{
	TCHAR tmp[MAX_PATH];
	DWORD dwSize = sizeof(tmp);
	LONG rv;

	rv = RegQueryValueEx(hkey, param_name,
		NULL, NULL,
		(BYTE *) tmp,
		&dwSize);
	if (rv == ERROR_SUCCESS)
	{
		size_t nchars = dwSize / sizeof(TCHAR);
		if (nchars >= MAX_PATH) { nchars = MAX_PATH - 1; }
		tmp[nchars] = '\0';
		return _tcsdup(tmp);
	}
	else
	{
		logger_printf(TEXT("RegQueryValueEx for '%s' returned %d"), param_name, rv);
		return NULL;
	}
}

static void
get_registry_params(const TCHAR *service_name, MyParams *pparams)
{
	TCHAR key_path[1000];
	LONG rv;
	HKEY hkey;

	/* defaults */
	pparams->script = NULL;
	pparams->delay = 0;

	_sntprintf(key_path, 1000, TEXT("SYSTEM\\CurrentControlSet\\Services\\%s\\Parameters"), service_name);
	rv = RegOpenKeyEx(HKEY_LOCAL_MACHINE, key_path,
			0, /* reserved */
			KEY_QUERY_VALUE, &hkey);
	if (rv != ERROR_SUCCESS)
		return;

	pparams->script = get_string_parameter(hkey, TEXT("Script"));
	pparams->status_window_cmd = get_string_parameter(hkey, TEXT("StatusWindowCommand"));

	pparams->delay = get_uint_parameter(hkey, TEXT("Delay"), 0);
	pparams->hour = get_uint_parameter(hkey, TEXT("Hour"), 101);
	RegCloseKey(hkey);
}

static void
SvcInit(DWORD dwArgc, TCHAR *lpszArgv[])
{
	MyParams params = {0};

	/* create an event; this event will be raised when we're told
	 * to shut down */
	stopEvent = CreateEvent(
			NULL, /* default security attributes */
			TRUE, /* manual reset event */
			FALSE, /* not signaled */
			NULL); /* no name */
	if (stopEvent == NULL)
	{
		report_status(SERVICE_STOPPED, NO_ERROR, 0);
		return;
	}

	/* look in registry for guidance */
	get_registry_params(lpszArgv[0], &params);
	logger_message(TEXT("startup: version 2011.03.21"));

	report_status(SERVICE_RUNNING, NO_ERROR, 0);

	while(1)
	{

	if (params.delay != 0)
	{
		HANDLE hTimer;
		LARGE_INTEGER li = {0};
		const __int64 nTimerUnitsPerSecond = 10000000;
		int nsecs_delay;

		if (params.hour != 101)
		{
			struct tm *tm;
			time_t t;
			int nhours_delay;

			time(&t);
			tm = localtime(&t);

			nhours_delay = (params.hour + 24 - tm->tm_hour) % 24;
			nsecs_delay = params.delay + 3600 * nhours_delay;
		}
		else
		{
			nsecs_delay = params.delay;
		}
		logger_printf(TEXT("waiting for %d seconds"), nsecs_delay);
		li.QuadPart = -(nsecs_delay * nTimerUnitsPerSecond);

		hTimer = CreateWaitableTimer(NULL, FALSE, NULL);
		SetWaitableTimer(hTimer, &li, 0, NULL, NULL, TRUE);

		{
			HANDLE handles[2];
			DWORD dwEvent;

			handles[0] = stopEvent;
			handles[1] = hTimer;

			dwEvent = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

			if (dwEvent == WAIT_OBJECT_0 + 0)
			{
				/* stop the service */
				logger_message(TEXT("received stop signal"));
				report_status(SERVICE_STOPPED, NO_ERROR, 0);
				CloseHandle(hTimer);
				return;
			}
			else if (dwEvent != WAIT_OBJECT_0 + 1)
			{
				/* FIXME- error returned by WaitForMultipleObjects */
			}
		}
		CloseHandle(hTimer);
	}

	if (check_for_active_sessions())
	{
		logger_printf(TEXT("station appears to be in use"));
		continue;
	}

		break;
	}

	if (params.script != NULL)
	{
		logger_printf(TEXT("resetting the idle timer"));
		SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_CONTINUOUS);

		if (params.status_window_cmd != NULL)
		{
			logger_printf(TEXT("launching status window process"));
			launch_status_window(params.status_window_cmd);
		}

		logger_printf(TEXT("launching script %s"), params.script);
		create_process(params.script);
	}
	else
	{
		logger_message(TEXT("nothing to run"));
	}

	{
		WaitForSingleObject(stopEvent, INFINITE);
		logger_message(TEXT("received stop signal"));
		report_status(SERVICE_STOPPED, NO_ERROR, 0);
		return;
	}
}

static DWORD WINAPI
SvcCtrlHandler(DWORD dwControl, DWORD dwEventType, void *lpEventData, void *lpContext)
{
	switch (dwControl)
	{
	case SERVICE_CONTROL_STOP:
		report_status(SERVICE_STOP_PENDING, NO_ERROR, 0);
		SetEvent(stopEvent);
		return NO_ERROR;

	case SERVICE_CONTROL_INTERROGATE:
		return NO_ERROR;

	case SERVICE_CONTROL_POWEREVENT:
		logger_printf(TEXT("power event %d"), dwEventType);
		break;

	case SERVICE_CONTROL_SESSIONCHANGE:
		logger_printf(TEXT("session change %d"), dwEventType);
		break;
	}

	return ERROR_CALL_NOT_IMPLEMENTED;
}

static void WINAPI
ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
	/* register the handler function for the service */
	statusHandle = RegisterServiceCtrlHandlerEx(SVCNAME, SvcCtrlHandler, NULL);
	if (!statusHandle)
	{
		SvcReportEvent(TEXT("RegisterServiceCtrlHandler"));
		return;
	}

	status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	status.dwServiceSpecificExitCode = 0;

	/* report initial status to the SCM */
	report_status(SERVICE_START_PENDING, NO_ERROR, 3000);

	/* report service-specific initialization and work */
	SvcInit(dwArgc, lpszArgv);
}

void __cdecl
_tmain(int argc, _TCHAR *argv[])
{
	SERVICE_TABLE_ENTRY DispatchTable[] = 
	{
		{ TEXT(""), ServiceMain },
		{ NULL, NULL },
	};

	if (argc == 2 && lstrcmpi(argv[1], TEXT("install")) == 0)
	{
		do_install();
		return;
	}

	if (argc == 2 && lstrcmpi(argv[1], TEXT("uninstall")) == 0)
	{
		do_uninstall();
		return;
	}

	if (argc == 2 && lstrcmpi(argv[1], TEXT("test")) == 0)
	{
		logger_message("hello world!");
		return;
	}

	if (!StartServiceCtrlDispatcher(DispatchTable))
	{
		if (GetLastError() != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
		{
			fatal_win32_error(TEXT("StartServiceCtrlDispatcher"));
		}
	
		fprintf(stderr, "Error: no option specified\n");
		fprintf(stderr, "Usage: %s install\n", argv[0]);
		exit(1);
	}
}
