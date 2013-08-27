#include "main.h"
#include "config.h"
#include "install.h"
#include "hinst.h"
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <time.h>
#include <wtsapi32.h>

static SERVICE_STATUS status = {0};
static SERVICE_STATUS_HANDLE statusHandle = NULL;
static HWND msgWinHandle = NULL;

#define M_STOP_SERVICE  (WM_USER+101)

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

typedef struct MyParamsStruct
{
	int dummy;
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

static void
get_registry_params(const TCHAR *service_name, MyParams *pparams)
{
	TCHAR key_path[1000];
	LONG rv;
	HKEY hkey;

	/* defaults */
	pparams->dummy = 0;

	_sntprintf(key_path, 1000, TEXT("SYSTEM\\CurrentControlSet\\Services\\%s\\Parameters"), service_name);
	rv = RegOpenKeyEx(HKEY_LOCAL_MACHINE, key_path,
			0, /* reserved */
			KEY_QUERY_VALUE, &hkey);
	if (rv != ERROR_SUCCESS)
		return;

	/* TODO- read registry values */

	RegCloseKey(hkey);
}

static LRESULT CALLBACK
my_wndproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case M_STOP_SERVICE:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

static void
SvcInit(DWORD dwArgc, TCHAR *lpszArgv[])
{
	MyParams params = {0};
	MSG msg;

	/* look in registry for guidance */
	get_registry_params(lpszArgv[0], &params);
	logger_message(TEXT("startup: version 2013.08.27"));

	report_status(SERVICE_RUNNING, NO_ERROR, 0);

	/* message loop */
	while (GetMessage(&msg, NULL, 0, 0))
	{
		DispatchMessage(&msg);
	}

	/* if here, then the quit message was received */
	logger_message(TEXT("received stop signal"));
	report_status(SERVICE_STOPPED, NO_ERROR, 0);
	return;
}

static DWORD WINAPI
SvcCtrlHandler(DWORD dwControl, DWORD dwEventType, void *lpEventData, void *lpContext)
{
	switch (dwControl)
	{
	case SERVICE_CONTROL_STOP:
		report_status(SERVICE_STOP_PENDING, NO_ERROR, 0);
		PostMessage(msgWinHandle, M_STOP_SERVICE, 0, 0);
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

#define MY_WNDCLASS_NAME TEXT("McPowerMonMessageWindow")
static const TCHAR *
my_wndclass(void)
{
	static ATOM registered = 0;
	if (registered == 0) {
		WNDCLASS wc = {0};
		wc.lpfnWndProc = my_wndproc;
		wc.cbWndExtra = 0;
		wc.hInstance = hInst;
		wc.lpszClassName = MY_WNDCLASS_NAME;
		registered = RegisterClass(&wc);
	}
	return MY_WNDCLASS_NAME;
}

static void WINAPI
ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
	hInst = GetModuleHandle(NULL);

	/* create a message-only window for message handling */
	msgWinHandle = CreateWindow(
		my_wndclass(),
		NULL,
		0, 0, 0, 0, 0, /* style and dimensions */
		HWND_MESSAGE,  /* parent */
		NULL,
		hInst,
		NULL);

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
