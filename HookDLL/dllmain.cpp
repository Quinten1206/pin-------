#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#pragma comment(linker, "/EXPORT:InstallHook=InstallHook")

// ── Global state (one DLL instance per pinned window) ──

static HWND g_hWnd = nullptr;
static UINT_PTR g_timerId = 0;
static HICON g_hPinIcon = nullptr;
static int g_iconSize = 16;
static DWORD g_mainProcessId = 0;

// ── Shared memory: EXE writes PID+HWND before injecting ──

static const WCHAR SHARED_MEM_NAME[] = L"DeskPins_SharedMem";

struct SharedData
{
    DWORD exePid;
    HWND hwnd;
};

// ── Create a simple pin-shaped icon (solid red rectangle as placeholder) ──

static void CreatePinIcon(HWND hwnd)
{
    int dpi = GetDpiForWindow(hwnd);
    g_iconSize = MulDiv(16, dpi, 96);

    HDC hdc = GetDC(hwnd);
    HBITMAP hbmColor = CreateCompatibleBitmap(hdc, g_iconSize, g_iconSize);
    HBITMAP hbmMask = CreateBitmap(g_iconSize, g_iconSize, 1, 1, nullptr);
    ReleaseDC(hwnd, hdc);

    HDC hdcMem = CreateCompatibleDC(nullptr);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hbmColor);

    RECT rc = {0, 0, g_iconSize, g_iconSize};
    HBRUSH hBr = CreateSolidBrush(RGB(200, 30, 30));
    FillRect(hdcMem, &rc, hBr);
    DeleteObject(hBr);

    SelectObject(hdcMem, hOld);
    DeleteDC(hdcMem);

    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmColor = hbmColor;
    ii.hbmMask = hbmMask;
    ii.xHotspot = 0;
    ii.yHotspot = 0;

    if (g_hPinIcon) DestroyIcon(g_hPinIcon);
    g_hPinIcon = CreateIconIndirect(&ii);

    DeleteObject(hbmColor);
    DeleteObject(hbmMask);
}

// ── Subclass procedure ──

static LRESULT CALLBACK SubclassProc(
    HWND hWnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR)
{
    switch (uMsg)
    {
    case WM_NCPAINT:
    {
        LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (!g_hPinIcon) CreatePinIcon(hWnd);

        HDC hdc = GetWindowDC(hWnd);
        if (hdc)
        {
            int titleBarH = GetSystemMetrics(SM_CYCAPTION)
                          + GetSystemMetrics(SM_CYFRAME)
                          + GetSystemMetrics(SM_CXPADDEDBORDER);
            int frameX = GetSystemMetrics(SM_CXFRAME)
                       + GetSystemMetrics(SM_CXPADDEDBORDER);

            RECT rc;
            GetWindowRect(hWnd, &rc);
            int iconX = (rc.right - rc.left) - frameX - g_iconSize * 4;
            int iconY = GetSystemMetrics(SM_CXPADDEDBORDER)
                      + (titleBarH - frameX - g_iconSize) / 2;

            DrawIconEx(hdc, iconX, iconY, g_hPinIcon,
                g_iconSize, g_iconSize, 0, nullptr, DI_NORMAL);
            ReleaseDC(hWnd, hdc);
        }
        return result;
    }

    case WM_NCLBUTTONDOWN:
    {
        if (wParam == HTCAPTION)
        {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT rc;
            GetWindowRect(hWnd, &rc);
            int frameX = GetSystemMetrics(SM_CXFRAME)
                       + GetSystemMetrics(SM_CXPADDEDBORDER);
            int titleBarH = GetSystemMetrics(SM_CYCAPTION)
                          + GetSystemMetrics(SM_CYFRAME)
                          + GetSystemMetrics(SM_CXPADDEDBORDER);
            int iconX = (rc.right - rc.left) - frameX - g_iconSize * 4;
            int iconY = GetSystemMetrics(SM_CXPADDEDBORDER)
                      + (titleBarH - frameX - g_iconSize) / 2;

            ScreenToClient(hWnd, &pt);
            if (pt.x >= iconX && pt.x <= iconX + g_iconSize &&
                pt.y >= iconY && pt.y <= iconY + g_iconSize)
            {
                SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                RemoveWindowSubclass(hWnd, SubclassProc, uIdSubclass);
                if (g_hPinIcon) { DestroyIcon(g_hPinIcon); g_hPinIcon = nullptr; }
                if (g_timerId) { KillTimer(nullptr, g_timerId); g_timerId = 0; }
                FreeLibraryAndExitThread(
                    GetModuleHandleW(L"DeskPinsHook.dll"), 0);
                return 0;
            }
        }
        break;
    }

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, SubclassProc, uIdSubclass);
        if (g_hPinIcon) { DestroyIcon(g_hPinIcon); g_hPinIcon = nullptr; }
        if (g_timerId) { KillTimer(nullptr, g_timerId); g_timerId = 0; }
        FreeLibraryAndExitThread(GetModuleHandleW(L"DeskPinsHook.dll"), 0);
        return 0;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// ── Heartbeat: checks if EXE died or window got unpinned from EXE side ──

static VOID CALLBACK HeartbeatTimer(HWND, UINT, UINT_PTR, DWORD)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, g_mainProcessId);
    bool exeDead = (hProcess == nullptr);
    if (!exeDead) CloseHandle(hProcess);

    bool unpinned = !(GetWindowLongW(g_hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST);

    if (exeDead || unpinned)
    {
        if (g_hPinIcon) { DestroyIcon(g_hPinIcon); g_hPinIcon = nullptr; }
        if (g_timerId) { KillTimer(nullptr, g_timerId); g_timerId = 0; }
        RemoveWindowSubclass(g_hWnd, SubclassProc, 1);
        FreeLibraryAndExitThread(GetModuleHandleW(L"DeskPinsHook.dll"), 0);
    }
}

// ── Worker thread: subclass the window and start heartbeat ──

static DWORD WINAPI WorkerThread(LPVOID)
{
    if (!IsWindow(g_hWnd)) return 1;

    SetWindowSubclass(g_hWnd, SubclassProc, 1, 0);
    g_timerId = SetTimer(nullptr, 0, 2000, HeartbeatTimer);

    SetWindowPos(g_hWnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    // Message loop for the timer (timers need a message pump)
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

// ── Exported: alternative path — EXE can call this via second CreateRemoteThread ──

extern "C" __declspec(dllexport) DWORD WINAPI InstallHook(LPVOID lpParam)
{
    g_hWnd = (HWND)lpParam;
    WorkerThread(nullptr);
    return 0;
}

// ── DllMain: read shared memory, spawn worker thread ──

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        HANDLE hMapping = OpenFileMappingW(FILE_MAP_READ, FALSE, SHARED_MEM_NAME);
        if (hMapping)
        {
            SharedData* pData = (SharedData*)MapViewOfFile(
                hMapping, FILE_MAP_READ, 0, 0, sizeof(SharedData));
            if (pData)
            {
                g_mainProcessId = pData->exePid;
                g_hWnd = pData->hwnd;
                UnmapViewOfFile(pData);

                // Spawn worker thread to subclass (can't do it from DllMain)
                HANDLE hThread = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
                if (hThread) CloseHandle(hThread);
            }
            CloseHandle(hMapping);
        }
    }
    return TRUE;
}
