#pragma once

#include <windows.h>

#define CLASS_NAME  "AisSimUiWnd"

LRESULT uiWndFunction (HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result;

    switch (msg)
    {
        case WM_CREATE:
            result = 1; break;

        default:
            result = DefWindowProc (wnd, msg, wParam, lParam);
    }

    return result;
}

void registerClass (WNDCLASS& classInfo)
{
    memset (& classInfo, 0, sizeof (classInfo));

    classInfo.hbrBackground = (HBRUSH) GetStockObject (WHITE_BRUSH);
    classInfo.hCursor       = (HCURSOR) LoadCursor (0, IDC_ARROW);
    classInfo.hIcon         = (HICON) LoadIcon (0, IDI_APPLICATION);
    classInfo.lpfnWndProc   = uiWndFunction;
    classInfo.lpszClassName = CLASS_NAME;
    classInfo.style         = CS_HREDRAW | CS_VREDRAW;
}

void runUI ()
{
    WNDCLASS classInfo;

    registerClass (classInfo);
}