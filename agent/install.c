#include "install.h"
#include "config.h"
#include "perror.h"
#include <windows.h>
#include <stdio.h>

void
do_install(void)
{
	TCHAR path[MAX_PATH];
	SC_HANDLE scm;
	SC_HANDLE serviceHandle;

	if (!GetModuleFileName(NULL, path, MAX_PATH))
	{
		fatal_win32_error(TEXT("GetModuleFileName"));
		return;
	}

	scm = OpenSCManager(
		NULL, /* local computer */
		NULL, /* ServicesActive database */
		SC_MANAGER_ALL_ACCESS);
	if (scm == NULL)
	{
		fatal_win32_error(TEXT("OpenSCManager"));
		return;
	}

	/* create the service */
	serviceHandle = CreateService(
		scm,
		SVCNAME,
		SVCNAME,
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START,
		SERVICE_ERROR_NORMAL,
		path,
		NULL, /* no load ordering group */
		NULL, /* no tag identifier */
		NULL, /* no dependencies */
		NULL, /* LocalSystem account */
		NULL); /* no password */
	if (serviceHandle == NULL)
	{
		fatal_win32_error(TEXT("CreateService"));
		CloseServiceHandle(scm);
		return;
	}

	printf("Service installed successfully\n");
	CloseServiceHandle(serviceHandle);
	CloseServiceHandle(scm);
}

void
do_uninstall(void)
{
	SC_HANDLE scm;
	SC_HANDLE serviceHandle;

	scm = OpenSCManager(
		NULL, /* local computer */
		NULL, /* ServicesActive database */
		SC_MANAGER_ALL_ACCESS);
	if (scm == NULL)
	{
		fatal_win32_error(TEXT("OpenSCManager"));
		return;
	}

	serviceHandle = OpenService(
		scm,
		SVCNAME,
		SERVICE_ALL_ACCESS);
	if (serviceHandle == NULL)
	{
		fatal_win32_error(TEXT("OpenService"));
		return;
	}

	if (!DeleteService(serviceHandle))
	{
		fatal_win32_error(TEXT("DeleteService"));
		return;
	}

	printf("Service deleted successfully\n");
	CloseServiceHandle(serviceHandle);
	CloseServiceHandle(scm);
}
