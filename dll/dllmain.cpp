#include <windows.h>
#include <commctrl.h>
#include "titlebarpainter.h"

static DWORD g_mainProcessId = 0;

static LRESULT CALLBACK SubclassProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp,
                                     UINT_PTR subclassId, DWORD_PTR refData) {
    switch (msg) {
    case WM_NCPAINT: {
        LRESULT result = DefSubclassProc(hWnd, msg, wp, lp);
        DrawPinOnTitleBar(hWnd);
        return result;
    }

    case WM_NCLBUTTONDOWN: {
        POINT pt = { (int)(short)LOWORD(lp), (int)(short)HIWORD(lp) };
        if (IsClickOnPinIcon(hWnd, pt)) {
            SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE |
                SWP_NOZORDER | SWP_FRAMECHANGED);
            RemoveWindowSubclass(hWnd, SubclassProc, subclassId);
            SetTimer(hWnd, 0xFFFF, 100, nullptr);
            return 0;
        }
        break;
    }

    case WM_TIMER:
        if (wp == 0xFFFF) {
            KillTimer(hWnd, 0xFFFF);
            FreeLibraryAndExitThread((HMODULE)refData, 0);
        }
        break;

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, SubclassProc, subclassId);
        break;
    }

    return DefSubclassProc(hWnd, msg, wp, lp);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        InitCommonControls();
        WCHAR buf[32];
        if (GetEnvironmentVariableW(L"DESKPINS_MAIN_PID", buf, 32)) {
            g_mainProcessId = wcstoul(buf, nullptr, 10);
        }
    }
    return TRUE;
}

extern "C" __declspec(dllexport)
BOOL WINAPI AttachToWindow(HWND hWnd, HMODULE hMod) {
    return SetWindowSubclass(hWnd, SubclassProc, 1, (DWORD_PTR)hMod);
}
