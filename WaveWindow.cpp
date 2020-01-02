
#include "WaveWindow.h"
#include "InOutSlider.h"
#include "main.h"

const WORD INOUT_SLIDER = 2;
HWND		m_hwndInOutSlider = 0;

LRESULT CALLBACK ChildWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_CREATE)
	{
		m_hwndInOutSlider = CreateWindow(	INOUTSLIDERCLASS,
														"InOut",
														WS_VISIBLE | WS_CHILD,
														10, 10,
														100, 100,
														hWnd,
														(HMENU)INOUT_SLIDER,
														g_hInstance,
														NULL	);
		
		CreateWindow(	INOUTSLIDERCLASS,
														"InOut",
														WS_VISIBLE | WS_CHILD,
														120, 10,
														100, 100,
														hWnd,
														(HMENU)INOUT_SLIDER,
														g_hInstance,
														NULL	);
	}
	if (message == WM_PAINT)
	{
		HDC hdc = GetDC( hWnd );
		PAINTSTRUCT ps;
		BeginPaint( hWnd, &ps );

		RECT rect;
		GetClientRect( hWnd, &rect );

//		SelectObject( hdc, GetStockObject(GRAY_BRUSH) );

		char buf[10000] = "Child";

//		DrawText( hdc, buf, strlen( buf ), &rect, 0 );

		EndPaint( hWnd, &ps );
	}
	else if (message == WM_COMMAND)
	{
		if (LOWORD( wParam ) == INOUT_SLIDER)
		{
			if (HIWORD( wParam ) == WM_INOUT_MOUSEMOVE)
			{
				int x = 0;
			}
			else
			{
				return DefWindowProc( hWnd, message, wParam, lParam );
			}
		}
		else
		{
			return DefWindowProc( hWnd, message, wParam, lParam );
		}
	}
	else if (message == WM_INOUT_MOUSEMOVE)
	{
		// wParam is child ID
		// lParam is child hwnd
		HWND child = (HWND)wParam;
		short xDrag = lParam;

		RECT rect;
		GetWindowRect( child, &rect );
		int width = rect.right - rect.left;
		int height = rect.bottom - rect.top;

		POINT upperLeft = { rect.left, rect.top };
		ScreenToClient( hWnd, &upperLeft );

		MoveWindow( child, upperLeft.x + xDrag, upperLeft.y, width, height, TRUE );
	}
	else
	{
		return DefWindowProc( hWnd, message, wParam, lParam );
	}

	return 0;
}

void initWaveWindow()
{
	WNDCLASS wc;

	ZeroMemory( &wc, sizeof wc );
	wc.lpfnWndProc = ChildWndProc;
	wc.hInstance = g_hInstance;
	wc.lpszClassName = CHILDCLASSNAME;
	wc.lpszMenuName  = 0;
	wc.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
	wc.hCursor       = 0;
	wc.hIcon         = 0;
	RegisterClass(&wc);

	initInOutSlider();
}

