#pragma once
#include <windows.h>

bool PinControllerStart(HINSTANCE hInst, HWND hWnd);
void PinControllerStop();
void PinControllerOnClick(HWND hTarget);
bool PinControllerIsActive();
