
#include "InOutSlider.h"
#include "main.h"

const WORD BTN_OK = 2;
bool 		m_lbuttonDown = false;
WORD		m_lbuttonDownX = 0;
WORD		m_lbuttonDownY = 0;

LRESULT CALLBACK InOutSliderProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	if (message == WM_CREATE)
	{
	}
	if (message == WM_PAINT)
	{
		HDC hdc = GetDC( hWnd );
		PAINTSTRUCT ps;
		BeginPaint( hWnd, &ps );

		RECT rect;
		GetClientRect( hWnd, &rect );

//		SelectObject( hdc, GetStockObject(GRAY_BRUSH) );

		EndPaint( hWnd, &ps );
	}
	else if (message == WM_COMMAND)
	{
		if (LOWORD( wParam ) == BTN_OK)
		{
			if (HIWORD( wParam ) == BN_CLICKED)
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
	else if (message == WM_LBUTTONDOWN)
	{
		m_lbuttonDown = true;
		m_lbuttonDownX = LOWORD( lParam );
		m_lbuttonDownY = HIWORD( lParam );
	}
	else if (message == WM_MOUSEMOVE)
	{
		if (m_lbuttonDown)
		{
			short xDrag = LOWORD( lParam ) - m_lbuttonDownX;
			SendMessage( GetParent( hWnd ), WM_INOUT_MOUSEMOVE, (WPARAM)hWnd, (LPARAM)xDrag );
		}
	}
	else if (message == WM_LBUTTONUP)
	{
		m_lbuttonDown = false;
	}
	else
	{
		return DefWindowProc( hWnd, message, wParam, lParam );
	}

	return 0;
}

void initInOutSlider()
{
	WNDCLASS wc;

	ZeroMemory( &wc, sizeof wc );
	wc.lpfnWndProc = InOutSliderProc;
	wc.hInstance = g_hInstance;
	wc.lpszClassName = INOUTSLIDERCLASS;
	wc.lpszMenuName  = 0;
	wc.hbrBackground = (HBRUSH)GetStockObject( GRAY_BRUSH );
	wc.hCursor       = 0;
	wc.hIcon         = 0;
	RegisterClass(&wc);
}

