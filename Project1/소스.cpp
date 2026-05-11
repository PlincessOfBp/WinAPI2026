#include <Windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <queue>
#include <math.h>

// 이미지소스\\Cursors.ko-KR << 필요한 부분만 잘라서 가공 필요!! 
// 이미지소스\\낚시\\570x540-Beach_Overview

HINSTANCE g_hInst;
LPCTSTR lpszClass = L"Window Class";
LPCTSTR lpszWindowName = L"windows program 1";

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	HDC hDC, hMemDC;
	PAINTSTRUCT ps;
	HBRUSH hBrush = (HBRUSH)GetStockObject(BLACK_BRUSH), oldBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
	TCHAR str[100];

	static HBITMAP hBitmap;
	static RECT rc;
	

	switch (iMessage)
	{
	case WM_CREATE:
		hBitmap = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\570x540-Beach_Overview.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		
		break;

	case WM_PAINT:
		hDC = BeginPaint(hWnd, &ps);
		hMemDC = CreateCompatibleDC(hDC);
		SelectObject(hMemDC, hBitmap);


		DeleteDC(hMemDC);
		EndPaint(hWnd, &ps);
		break;

	case WM_LBUTTONDOWN:
	{
		
			
		InvalidateRect(hWnd, NULL, TRUE);
		break;
	}

	case WM_MOUSEMOVE:
	{
		
		break;
	}

	case WM_TIMER:
	{
		switch (wParam) {

		}
		InvalidateRect(hWnd, NULL, TRUE);
		break;
	}

	case WM_RBUTTONDOWN:
	{
		
		InvalidateRect(hWnd, NULL, TRUE);
		break;
	}

	case WM_KEYDOWN:
		switch (wParam) {
		}
		InvalidateRect(hWnd, NULL, TRUE);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, iMessage, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow)
{
	HWND hWnd;
	MSG Message;
	WNDCLASSEX WndClass;
	g_hInst = hInstance;

	srand(time(NULL));

	WndClass.cbSize = sizeof(WndClass);
	WndClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	WndClass.lpfnWndProc = (WNDPROC)WndProc;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hInstance;
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = lpszClass;
	WndClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClassEx(&WndClass);

	hWnd = CreateWindow(lpszClass, lpszWindowName, WS_OVERLAPPEDWINDOW,
		0, 0, 800, 800, NULL, (HMENU)NULL, hInstance, NULL);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	while (GetMessage(&Message, 0, 0, 0)) {
		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}
	return Message.wParam;
}