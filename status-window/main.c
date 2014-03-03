#include <winsock2.h>
#include <ws2tcpip.h>
#include "main.h"
#include "hinst.h"
#include "printf.h"
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <time.h>

#define IDC_PRIM 101
#define IDC_SEC  102
#define IDC_WATCHDOG_TIMER 103
#define M_PACKET (WM_USER+1)
#define WATCHDOG_TIMER 10

static HWND mainWin = NULL;
static SOCKET mySocket = {0};
static BOOL watchdog_enabled = FALSE;
static DWORD watchdog_warn_time = 0;
static DWORD watchdog_expire_time = 0;

static void
createServerSocket(void)
{
	struct addrinfo hints;
	struct addrinfo *result;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo("127.0.0.1", "56599", &hints, &result);
	mySocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	bind(mySocket, result->ai_addr, (int)result->ai_addrlen);
	freeaddrinfo(result);
}

static void
on_create(HWND hWnd)
{
	LOGFONT lf = {0};
	HFONT hfont;

	CreateWindow(
		TEXT("STATIC"),
		NULL,
		WS_CHILD | WS_VISIBLE | SS_CENTER,
		0, 0,
		500, 100,
		hWnd,
		(HMENU)(IDC_PRIM),
		hInst,
		NULL);

	lf.lfHeight = 36;
	lf.lfPitchAndFamily = FF_SWISS;
	hfont = CreateFontIndirect(&lf);
	SendDlgItemMessage(hWnd, IDC_PRIM,
			WM_SETFONT, (WPARAM)hfont, TRUE);

	CreateWindow(
		TEXT("STATIC"),
		NULL,
		WS_CHILD | WS_VISIBLE | SS_CENTER,
		0, 100,
		500, 100,
		hWnd,
		(HMENU)(IDC_SEC),
		hInst,
		NULL);

	lf.lfHeight = 24;
	lf.lfPitchAndFamily = FF_SWISS;
	hfont = CreateFontIndirect(&lf);
	SendDlgItemMessage(hWnd, IDC_SEC,
			WM_SETFONT, (WPARAM)hfont, TRUE);

	CreateWindow(
		TEXT("STATIC"),
		NULL,
		WS_CHILD | SS_CENTER,
		0, 100,
		500, 100,
		hWnd,
		(HMENU)(IDC_WATCHDOG_TIMER),
		hInst,
		NULL);

	createServerSocket();
	WSAAsyncSelect(mySocket, hWnd,
		M_PACKET,
		FD_READ);
}

static INT_PTR
on_ctlcolorstatic(HWND hWnd, HDC hDC, HWND hwndCtrl)
{
	SetTextColor(hDC, RGB(0,0,0));
	SetBkMode(hDC, TRANSPARENT);
	return (INT_PTR) GetStockObject(WHITE_BRUSH);
}

static void
on_size(HWND hWnd, WPARAM sizeType, int newWidth, int newHeight)
{
	/* calculate height of standard text */
	int timHeight;
	{
		TEXTMETRIC tm = {0};
		HDC hdc = GetDC(GetDlgItem(hWnd, IDC_WATCHDOG_TIMER));
		GetTextMetrics(hdc, &tm);
		ReleaseDC(GetDlgItem(hWnd, IDC_WATCHDOG_TIMER), hdc);

		timHeight = tm.tmHeight;
	}

	MoveWindow(
		GetDlgItem(hWnd, IDC_PRIM),
		0, newHeight/2-40,
		newWidth, 40,
		TRUE
		);
	MoveWindow(
		GetDlgItem(hWnd, IDC_SEC),
		0, newHeight/2,
		newWidth, 40,
		TRUE
		);
	MoveWindow(
		GetDlgItem(hWnd, IDC_WATCHDOG_TIMER),
		0, newHeight-timHeight,
		newWidth, timHeight,
		TRUE
		);
}

static void
log_watchdog_warning(void)
{
	//TODO
}

static void
on_watchdog_timer(void)
{
	DWORD curTime = GetTickCount();
	INT32 timeWarn = (INT32)(watchdog_warn_time - curTime);
	INT32 timeExpire = (INT32)(watchdog_expire_time - curTime);

	if (timeExpire <= 0) {

		/* initiate reboot */
		/*system("C:\\maintenance\\leave-maintenance-mode.bat");*/
		system("shutdown /r /c \"watchdog timeout\" /t 0");
		SetDlgItemText(mainWin, IDC_WATCHDOG_TIMER, TEXT("Forcing a reboot."));
		KillTimer(mainWin, WATCHDOG_TIMER);
		return;
	}

	if (timeWarn <= 0) {

		log_watchdog_warning();

	{
	WCHAR *tmp = tcsdup_printf(TEXT("The operation is taking longer than expected. A reboot will be forced in %ds"), (timeExpire/1000)+1);
	SetDlgItemText(mainWin, IDC_WATCHDOG_TIMER, tmp);
	free(tmp);

	ShowWindow(GetDlgItem(mainWin, IDC_WATCHDOG_TIMER), SW_SHOW);
	}

	}

	SetTimer(mainWin, WATCHDOG_TIMER, (timeExpire % 1000)+10, NULL);
}

static void
cancel_watchdog(void)
{
	if (watchdog_enabled) {
		KillTimer(mainWin, WATCHDOG_TIMER);
		ShowWindow(GetDlgItem(mainWin, IDC_WATCHDOG_TIMER), SW_HIDE);
		watchdog_enabled = FALSE;
	}
}

static void
reset_watchdog(int time_sec)
{
	DWORD curTime = GetTickCount();
	watchdog_warn_time = curTime + (time_sec * 1000) / 2;
	watchdog_expire_time = curTime + (time_sec * 1000);

	if (watchdog_enabled) {
		KillTimer(mainWin, WATCHDOG_TIMER);
	}

	/* schedule watchdog timer */
	watchdog_enabled = TRUE;
	on_watchdog_timer();
}

static void
on_udp_packet(void)
{
	char buf[1500];

	int nbytes = recvfrom(mySocket,
			buf, sizeof(buf)-1,
			0, /*flags */
			NULL, NULL /* optional src addr struct and len */
			);

	if (nbytes > 0) {
		buf[nbytes] = '\0';

		if (buf[0] == '1') {
			WCHAR *tmp = tcsdup_printf(L"%S", &buf[1]);
			SetDlgItemText(mainWin, IDC_PRIM, tmp);
			free(tmp);
			SetDlgItemText(mainWin, IDC_SEC, TEXT(""));
			ShowWindow(mainWin, SW_SHOW);
		}
		else if (buf[0] == '2') {
			WCHAR *tmp = tcsdup_printf(L"%S", &buf[1]);
			SetDlgItemText(mainWin, IDC_SEC, tmp);
			free(tmp);
		}
		else if (buf[0] == '0') {
			SetDlgItemText(mainWin, IDC_PRIM, TEXT(""));
			SetDlgItemText(mainWin, IDC_SEC, TEXT(""));
		}
		else if (strcmp(buf, "-hide") == 0) {
			ShowWindow(mainWin, SW_HIDE);
			return;
		}
		else if (strcmp(buf, "-watchdog_cancel") == 0) {
			cancel_watchdog();
			return;
		}
		else if (strncmp(buf, "-watchdog ", 9) == 0) {
			reset_watchdog(atoi(&buf[9]));
			return;
		}
		InvalidateRect(mainWin, NULL, TRUE);
	}
}

static LRESULT CALLBACK
my_wndproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		on_create(hWnd);
		return 0;

	case WM_CLOSE:
		PostQuitMessage(0);
		break;

	case WM_CTLCOLORSTATIC:
		return on_ctlcolorstatic(hWnd, (HDC)wParam, (HWND)lParam);

	case WM_SIZE:
		on_size(hWnd, wParam, LOWORD(lParam), HIWORD(lParam));
		return 0;

	case WM_TIMER:
		if (wParam == WATCHDOG_TIMER) {
			on_watchdog_timer();
			return 0;
		}
		break;

	case M_PACKET:
		on_udp_packet();
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

#define MY_WNDCLASS_NAME TEXT("McShepherdStatusWindow")
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
		wc.hbrBackground = GetStockObject(WHITE_BRUSH);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		registered = RegisterClass(&wc);
	}
	return MY_WNDCLASS_NAME;
}

static void
makeMainWin(void)
{
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	int winWidth = screenWidth / 2;
	int winHeight = screenHeight / 4;

	mainWin = CreateWindow(
		my_wndclass(),
		TEXT("Please Wait. Updates are being applied."),
		WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER | WS_VISIBLE, /* style */
		screenWidth/2-winWidth/2,
		screenHeight/2-winHeight/2,
		winWidth,
		winHeight,
		NULL,  /* no parent -> top-level window */
		NULL,
		hInst,
		NULL);
	if (mainWin == NULL) {
		exit(1);
	}
}

static void
initApp(HINSTANCE hInstance)
{
	WSADATA wsaData = {0};

	hInst = hInstance;

	WSAStartup(MAKEWORD(2,2), &wsaData);
}

int CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;

	initApp(hInstance);

	/* create a window for displaying the message */
	makeMainWin();

	/* message loop */
	while (GetMessage(&msg, NULL, 0, 0))
	{
		DispatchMessage(&msg);
	}
}
