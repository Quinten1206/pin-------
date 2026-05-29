#include "traymanager.h"
#include "version.h"
#include <shellapi.h>

static HINSTANCE g_hInst;
static TrayCallbacks g_cb;

bool TrayInit(HINSTANCE hInst, HWND hWnd, const TrayCallbacks* cb) {
    g_hInst = hInst;
    g_cb = *cb;

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
    wcscpy_s(nid.szTip, L"DeskPins - 窗口置顶工具");

    return Shell_NotifyIconW(NIM_ADD, &nid);
}

void TrayShutdown(HWND hWnd) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_ICON;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void TrayShowMenu(HWND hWnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 1, L"开始钉选");
    AppendMenuW(hMenu, MF_STRING, 2, L"清除所有置顶");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 3, L"退出");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hWnd);

    int cmd = (int)TrackPopupMenu(hMenu,
        TPM_RETURNCMD | TPM_NONOTIFY,
        pt.x, pt.y, 0, hWnd, nullptr);

    PostMessageW(hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);

    switch (cmd) {
    case 1: if (g_cb.onStartPin) g_cb.onStartPin(); break;
    case 2: if (g_cb.onClearAll) g_cb.onClearAll(); break;
    case 3: if (g_cb.onExit) g_cb.onExit(); break;
    }
}

void TrayOnDoubleClick(void) {
    if (g_cb.onStartPin) g_cb.onStartPin();
}
