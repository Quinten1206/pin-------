#pragma once
#include <windows.h>

bool InjectDLL(HINSTANCE hInst, HWND hTarget, DWORD processId);
