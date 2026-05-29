#include "dllinjector.h"
#include "version.h"
#include <cstdio>
#include <TlHelp32.h>

static bool ExtractToTemp(HINSTANCE hInst, WCHAR* outPath, size_t outLen) {
    HRSRC hRes = FindResourceW(hInst, MAKEINTRESOURCEW(IDR_DLL_BIN), (LPCWSTR)RT_RCDATA);
    if (!hRes) return false;
    HGLOBAL hGlobal = LoadResource(hInst, hRes);
    if (!hGlobal) return false;
    void* pData = LockResource(hGlobal);
    DWORD size = SizeofResource(hInst, hRes);
    if (!pData || size == 0) return false;

    WCHAR tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    swprintf(outPath, outLen, L"%sDeskPinsHook.dll", tempPath);

    HANDLE hFile = CreateFileW(outPath, GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written;
    WriteFile(hFile, pData, size, &written, nullptr);
    CloseHandle(hFile);
    return written == size;
}

static LPTHREAD_START_ROUTINE GetLoadLibraryWAddr() {
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    return (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryW");
}

static HMODULE FindRemoteModule(HANDLE hProcess, const WCHAR* name) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetProcessId(hProcess));
    if (hSnap == INVALID_HANDLE_VALUE) return nullptr;
    MODULEENTRY32W me = { sizeof(me) };
    HMODULE hFound = nullptr;
    if (Module32FirstW(hSnap, &me)) {
        do {
            if (_wcsicmp(me.szModule, name) == 0) {
                hFound = me.hModule;
                break;
            }
        } while (Module32NextW(hSnap, &me));
    }
    CloseHandle(hSnap);
    return hFound;
}

bool InjectDLL(HINSTANCE hInst, HWND hTarget, DWORD processId) {
    WCHAR dllPath[MAX_PATH];
    if (!ExtractToTemp(hInst, dllPath, MAX_PATH)) return false;

    WCHAR pidStr[32];
    swprintf(pidStr, 32, L"%lu", GetCurrentProcessId());
    SetEnvironmentVariableW(L"DESKPINS_MAIN_PID", pidStr);

    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
        PROCESS_QUERY_INFORMATION,
        FALSE, processId);
    if (!hProcess) return false;

    size_t pathSize = (wcslen(dllPath) + 1) * sizeof(WCHAR);
    void* pRemoteMem = VirtualAllocEx(hProcess, nullptr, pathSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMem) { CloseHandle(hProcess); return false; }

    WriteProcessMemory(hProcess, pRemoteMem, dllPath, pathSize, nullptr);

    LPTHREAD_START_ROUTINE pLoadLibrary = GetLoadLibraryWAddr();
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
        pLoadLibrary, pRemoteMem, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);

    HMODULE hRemoteDll = FindRemoteModule(hProcess, L"DeskPinsHook.dll");
    if (hRemoteDll) {
        HMODULE hLocalDll = GetModuleHandleW(L"DeskPinsHook.dll");
        if (!hLocalDll) hLocalDll = LoadLibraryW(dllPath);
        if (hLocalDll) {
            FARPROC pLocal = GetProcAddress(hLocalDll, "AttachToWindow");
            if (pLocal) {
                uintptr_t offset = (uintptr_t)pLocal - (uintptr_t)hLocalDll;
                LPTHREAD_START_ROUTINE pRemote = (LPTHREAD_START_ROUTINE)((uintptr_t)hRemoteDll + offset);

                void* pArg = VirtualAllocEx(hProcess, nullptr, sizeof(HWND) + sizeof(HMODULE),
                    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                if (pArg) {
                    struct { HWND hwnd; HMODULE hmod; } args = { hTarget, hRemoteDll };
                    WriteProcessMemory(hProcess, pArg, &args, sizeof(args), nullptr);
                    HANDLE hCall = CreateRemoteThread(hProcess, nullptr, 0,
                        pRemote, pArg, 0, nullptr);
                    if (hCall) {
                        WaitForSingleObject(hCall, 5000);
                        CloseHandle(hCall);
                    }
                    VirtualFreeEx(hProcess, pArg, 0, MEM_RELEASE);
                }
            }
        }
    }

    VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return true;
}
