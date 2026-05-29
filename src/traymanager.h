#pragma once
#include <windows.h>

struct TrayCallbacks {
    void (*onStartPin)(void);
    void (*onClearAll)(void);
    void (*onExit)(void);
};

bool TrayInit(HINSTANCE hInst, HWND hWnd, const TrayCallbacks* cb);
void TrayShutdown(HWND hWnd);
void TrayShowMenu(HWND hWnd);
void TrayOnDoubleClick(void);
