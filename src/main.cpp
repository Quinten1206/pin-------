#include <windows.h>
#include "version.h"
#include "traymanager.h"
#include "proclist.h"
#include "pincontroller.h"

static HWND g_hWnd;
static HINSTANCE g_hInstance;

static void OnStartPin() {
    PinControllerStart(g_hInstance, g_hWnd);
}

static void OnClearAll() {
    ProcListClearAll();
}

static void OnExit() {
    PinControllerStop();
    TrayShutdown(g_hWnd);
    PostQuitMessage(0);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TRAYICON:
        if (LOWORD(lp) == WM_RBUTTONUP) {
            TrayShowMenu(hWnd);
        } else if (LOWORD(lp) == WM_LBUTTONDBLCLK) {
            TrayOnDoubleClick();
        }
        break;

    case WM_APP + 10:
        PinControllerOnClick((HWND)wp);
        break;

    case WM_HOTKEY:
        if (wp == HOTKEY_ID_ESC) PinControllerStop();
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"DeskPins_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    g_hInstance = hInstance;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DeskPins_Hidden";
    RegisterClassExW(&wc);

    g_hWnd = CreateWindowExW(0, L"DeskPins_Hidden", L"DeskPins",
        WS_OVERLAPPED, 0, 0, 100, 100, nullptr, nullptr, hInstance, nullptr);

    TrayCallbacks cb = { OnStartPin, OnClearAll, OnExit };
    if (!TrayInit(hInstance, g_hWnd, &cb)) {
        CloseHandle(hMutex);
        return 1;
    }

    ProcListStartPolling(g_hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseHandle(hMutex);
    return 0;
}
