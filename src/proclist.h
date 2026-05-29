#pragma once
#include <windows.h>
#include <vector>

struct PinEntry {
    HWND hwnd;
    DWORD threadId;
    DWORD processId;
};

void ProcListAdd(HWND hwnd);
void ProcListRemove(HWND hwnd);
bool ProcListContains(HWND hwnd);
void ProcListClearAll();
const std::vector<PinEntry>& ProcListAll();
void ProcListStartPolling(HWND hWnd);
