#include <windows.h>
#include "version.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"DeskPins_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    MessageBoxW(nullptr, L"DeskPins started OK", APP_NAME, MB_OK);

    CloseHandle(hMutex);
    return 0;
}
