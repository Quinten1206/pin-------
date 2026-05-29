#include "titlebarpainter.h"
#include <cmath>

static RECT DrawPinShape(HDC hdc, RECT rc) {
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;

    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    int sz = MulDiv(12, dpi, 96);

    HPEN hPen = CreatePen(PS_SOLID, MulDiv(1, dpi, 96), RGB(200, 30, 30));
    HBRUSH hBrush = CreateSolidBrush(RGB(220, 60, 60));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

    int r = sz / 3;
    int headY = cy - r / 2;
    Ellipse(hdc, cx - r, headY - r, cx + r, headY + r);

    POINT pts[3] = {
        { cx, cy + sz / 2 },
        { cx - r / 2, headY + r },
        { cx + r / 2, headY + r }
    };
    Polygon(hdc, pts, 3);

    HPEN hThinPen = CreatePen(PS_SOLID, MulDiv(1, dpi, 96), RGB(180, 20, 20));
    SelectObject(hdc, hThinPen);
    MoveToEx(hdc, cx - r - 2, headY, nullptr);
    LineTo(hdc, cx + r + 2, headY);
    DeleteObject(hThinPen);

    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);

    RECT result;
    result.left = cx - sz;
    result.top = cy - sz;
    result.right = cx + sz;
    result.bottom = cy + sz;
    return result;
}

static int GetTitleBarHeight(HWND hwnd) {
    UINT dpi = GetDpiForWindow(hwnd);
    int h = GetSystemMetrics(SM_CYCAPTION);
    h = MulDiv(h, (int)dpi, 96);
    int frameY = GetSystemMetrics(SM_CYFRAME);
    int paddedBorder = 0;
    if (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_THICKFRAME) {
        paddedBorder = GetSystemMetrics(SM_CXPADDEDBORDER);
    }
    h += frameY + paddedBorder;
    return h;
}

void DrawPinOnTitleBar(HWND hwnd) {
    if (!IsWindowVisible(hwnd)) return;
    LONG exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (!(exStyle & WS_EX_TOPMOST)) return;

    RECT rcWindow;
    GetWindowRect(hwnd, &rcWindow);
    UINT dpi = GetDpiForWindow(hwnd);
    int titleBarHeight = GetTitleBarHeight(hwnd);

    int sysBtnWidth = GetSystemMetrics(SM_CXSIZE);
    int iconSize = MulDiv(16, (int)dpi, 96);
    int marginX = sysBtnWidth * 3 + MulDiv(8, (int)dpi, 96);
    int marginY = (titleBarHeight - iconSize) / 2;

    int iconLeft = (rcWindow.right - rcWindow.left) - marginX - iconSize;
    int iconTop = marginY;

    RECT rcIcon = { iconLeft, iconTop, iconLeft + iconSize, iconTop + iconSize };

    HDC hdc = GetWindowDC(hwnd);
    if (!hdc) return;
    DrawPinShape(hdc, rcIcon);
    ReleaseDC(hwnd, hdc);
}

RECT GetPinIconRect(HWND hwnd) {
    RECT rcWindow;
    GetWindowRect(hwnd, &rcWindow);
    UINT dpi = GetDpiForWindow(hwnd);
    int titleBarHeight = GetTitleBarHeight(hwnd);

    int sysBtnWidth = GetSystemMetrics(SM_CXSIZE);
    int iconSize = MulDiv(16, (int)dpi, 96);
    int marginX = sysBtnWidth * 3 + MulDiv(8, (int)dpi, 96);
    int marginY = (titleBarHeight - iconSize) / 2;

    RECT rc = {
        rcWindow.left + (rcWindow.right - rcWindow.left) - marginX - iconSize,
        rcWindow.top + marginY,
        rcWindow.left + (rcWindow.right - rcWindow.left) - marginX,
        rcWindow.top + marginY + iconSize
    };
    return rc;
}

bool IsClickOnPinIcon(HWND hwnd, POINT ptScreen) {
    RECT rc = GetPinIconRect(hwnd);
    return PtInRect(&rc, ptScreen);
}
