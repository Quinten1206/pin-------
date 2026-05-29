#include "pincontroller.h"
#include "dllinjector.h"
#include "proclist.h"
#include "version.h"

static HINSTANCE g_hInst;
static HWND g_hWnd;
static HHOOK g_hMouseHook;
static HCURSOR g_hPinCursor;
static HCURSOR g_hPrevCursor;
static bool g_active = false;

static HWND GetTargetWindow(POINT pt) {
    HWND hWnd = WindowFromPoint(pt);
    if (!hWnd) return nullptr;
    HWND hOwner = GetAncestor(hWnd, GA_ROOTOWNER);
    return hOwner ? hOwner : hWnd;
}

static LRESULT CALLBACK MouseLLProc(int nCode, WPARAM wp, LPARAM lp) {
    if (nCode < 0) return CallNextHookEx(nullptr, nCode, wp, lp);
    MSLLHOOKSTRUCT* p = (MSLLHOOKSTRUCT*)lp;

    if (wp == WM_LBUTTONDOWN) {
        HWND hTarget = GetTargetWindow(p->pt);
        if (hTarget) {
            PostMessageW(g_hWnd, WM_APP + 10, (WPARAM)hTarget, 0);
        }
    }

    return CallNextHookEx(nullptr, nCode, wp, lp);
}

static void EnterPinMode() {
    g_active = true;
    g_hPinCursor = LoadCursorW(g_hInst, MAKEINTRESOURCEW(IDC_PIN_CURSOR));
    g_hPrevCursor = SetCursor(g_hPinCursor);
    SetCapture(g_hWnd);
    RegisterHotKey(g_hWnd, HOTKEY_ID_ESC, 0, VK_ESCAPE);
    g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseLLProc, g_hInst, 0);
}

static void ExitPinMode() {
    g_active = false;
    if (g_hMouseHook) { UnhookWindowsHookEx(g_hMouseHook); g_hMouseHook = nullptr; }
    UnregisterHotKey(g_hWnd, HOTKEY_ID_ESC);
    ReleaseCapture();
    SetCursor(g_hPrevCursor);
}

void PinControllerOnClick(HWND hTarget) {
    if (!g_active) return;

    WCHAR className[256];
    GetClassNameW(hTarget, className, 256);
    bool isUwp = (wcscmp(className, L"ApplicationFrameWindow") == 0);

    SetWindowPos(hTarget, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    if (!isUwp) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hTarget, &pid);
        InjectDLL(g_hInst, hTarget, pid);
    }

    ProcListAdd(hTarget);
    ExitPinMode();
}

bool PinControllerStart(HINSTANCE hInst, HWND hWnd) {
    if (g_active) return false;
    g_hInst = hInst;
    g_hWnd = hWnd;
    EnterPinMode();
    return true;
}

void PinControllerStop() {
    if (g_active) ExitPinMode();
}

bool PinControllerIsActive() {
    return g_active;
}
