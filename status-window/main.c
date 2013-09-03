#include "main.h"
#include "hinst.h"
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <time.h>

static HWND mainWin = NULL;

static LRESULT CALLBACK
my_wndproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		break;
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
		wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
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
		NULL,
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

int CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;

	hInst = hInstance;

	/* create a window for displaying the message */
	makeMainWin();

	/* message loop */
	while (GetMessage(&msg, NULL, 0, 0))
	{
		DispatchMessage(&msg);
	}
}
