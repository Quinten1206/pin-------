#pragma once
#include <windows.h>

void DrawPinOnTitleBar(HWND hwnd);
bool IsClickOnPinIcon(HWND hwnd, POINT ptScreen);
RECT GetPinIconRect(HWND hwnd);
