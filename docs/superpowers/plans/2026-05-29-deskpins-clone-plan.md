# DeskPins Clone — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a portable window-pinning utility — system-tray app, pin-mode cursor, click-to-pin windows, DLL-injected title bar pin icons.

**Architecture:** Single EXE with embedded DLL resource. EXE manages tray, mouse hook, and injection. DLL handles per-window subclassing for title bar icon drawing and click-to-unpin.

**Tech Stack:** C++20 / MinGW-w64 (MSYS2 UCRT64) / CMake / Win32 API + comctl32

---

## File Map

| File | Purpose | Task |
|------|---------|------|
| `CMakeLists.txt` (root) | Top-level project, two targets | 1 |
| `src/CMakeLists.txt` | EXE target definition | 1 |
| `src/resource.h` | Resource IDs for EXE | 1, 3 |
| `src/resource.rc` | Embed .ico, .cur, DLL binary | 1, 3 |
| `src/version.h` | App-wide constants | 1 |
| `src/main.cpp` | WinMain, message pump, wiring | 6 |
| `src/traymanager.h` | Tray manager interface | 2 |
| `src/traymanager.cpp` | Shell_NotifyIcon, menu, double-click | 2 |
| `src/proclist.h` | Pinned window tracker interface | 3 |
| `src/proclist.cpp` | Track HWNDs, dead-window cleanup | 3 |
| `src/dllinjector.h` | DLL injector interface | 4 |
| `src/dllinjector.cpp` | Resource extraction, CreateRemoteThread | 4 |
| `src/pincontroller.h` | Pin mode controller interface | 5 |
| `src/pincontroller.cpp` | WH_MOUSE_LL hook, cursor, ESC, highlight | 5 |
| `dll/CMakeLists.txt` | DLL target definition | 7 |
| `dll/resource.h` | Resource IDs for DLL | 7 |
| `dll/resource.rc` | Pin icon bitmap | 7 |
| `dll/dllmain.cpp` | DllMain, SetWindowSubclass, click detection | 7, 8 |
| `dll/titlebarpainter.h` | Title bar icon drawing interface | 8 |
| `dll/titlebarpainter.cpp` | WM_NCPAINT handler, DPI-aware GDI drawing | 8 |
| `assets/app.ico` | Tray icon (16x16 + 32x32 + 48x48) | 1 |
| `assets/pin.cur` | Pin cursor (.cur file) | 1 |
| `.vscode/tasks.json` | Build task for CMake | 1 |
| `.vscode/launch.json` | Debug config for CMake | 1 |

---

### Task 1: Project Scaffolding

**Files:**
- Create: `CMakeLists.txt`, `src/CMakeLists.txt`, `dll/CMakeLists.txt`, `src/version.h`, `src/resource.h`, `src/resource.rc`, `assets/app.ico`, `assets/pin.cur`
- Modify: `.vscode/tasks.json`, `.vscode/launch.json`

- [ ] **Step 1: Create top-level CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.20)
project(DeskPins VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(dll)
add_subdirectory(src)
```

- [ ] **Step 2: Create src/CMakeLists.txt**

```cmake
set(EXE_SOURCES
    main.cpp
    traymanager.cpp
    proclist.cpp
    dllinjector.cpp
    pincontroller.cpp
)

set(EXE_HEADERS
    version.h
    resource.h
    traymanager.h
    proclist.h
    dllinjector.h
    pincontroller.h
)

add_executable(DeskPins ${EXE_SOURCES} ${EXE_HEADERS} resource.rc)

# Embed the DLL binary as a post-build dependency
add_dependencies(DeskPins DeskPinsHook)

# Copy DLL next to EXE for resource compilation to find it
add_custom_command(TARGET DeskPins POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:DeskPinsHook>
        $<TARGET_FILE_DIR:DeskPins>/DeskPinsHook.dll
)

target_include_directories(DeskPins PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DeskPins PRIVATE comctl32)
```

- [ ] **Step 3: Create dll/CMakeLists.txt**

```cmake
add_library(DeskPinsHook SHARED
    dllmain.cpp
    titlebarpainter.cpp
    resource.rc
)

target_include_directories(DeskPinsHook PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DeskPinsHook PRIVATE comctl32 gdi32 user32)

set_target_properties(DeskPinsHook PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)
```

- [ ] **Step 4: Create src/version.h**

```cpp
#pragma once

#define APP_NAME        L"DeskPins"
#define APP_VERSION     L"1.0.0"
#define WM_TRAYICON     (WM_APP + 1)
#define HOTKEY_ID_ESC   1
#define HOTKEY_ID_PIN   2
#define TIMER_ID_POLL   1
#define POLL_INTERVAL_MS 1000
#define ID_TRAY_ICON    1

// Resource IDs
#define IDI_APP_ICON    101
#define IDC_PIN_CURSOR  102
#define IDR_DLL_BIN     103
```

- [ ] **Step 5: Create src/resource.h**

```cpp
#pragma once
#include "version.h"
// IDI_APP_ICON, IDC_PIN_CURSOR, IDR_DLL_BIN defined in version.h
```

- [ ] **Step 6: Create src/resource.rc**

```cpp
#include "resource.h"

IDI_APP_ICON    ICON    "../assets/app.ico"
IDC_PIN_CURSOR  CURSOR  "../assets/pin.cur"
IDR_DLL_BIN     RCDATA  "DeskPinsHook.dll"
```

- [ ] **Step 7: Create placeholder asset files**

Create `assets/app.ico` and `assets/pin.cur` as minimal valid files. For now, generate a simple 16x16 solid-color .ico and a basic .cur using a hex dump approach.

For `assets/app.ico` — a minimal 16x16 32bpp ICO (just a red dot for now, 318 bytes standard minimal ICO header + 1 BMP image).

For `assets/pin.cur` — a minimal 32x32 monochrome cursor (standard .cur format with hotspot at 0,0).

(These will be replaced with proper designed icons later, but must be valid now for compilation.)

- [ ] **Step 8: Create src/main.cpp (skeleton)**

```cpp
#include <windows.h>
#include "version.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // Single-instance check
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"DeskPins_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    // TODO: Rest wired in later tasks
    MessageBoxW(nullptr, L"DeskPins started OK", APP_NAME, MB_OK);

    CloseHandle(hMutex);
    return 0;
}
```

- [ ] **Step 9: Update .vscode/tasks.json for CMake**

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "type": "cmake",
            "label": "CMake: Configure",
            "command": "configure",
            "group": "build",
            "options": {
                "cwd": "${workspaceFolder}/build"
            }
        },
        {
            "type": "cmake",
            "label": "CMake: Build",
            "command": "build",
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "options": {
                "cwd": "${workspaceFolder}/build"
            }
        }
    ]
}
```

- [ ] **Step 10: Update .vscode/launch.json**

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "(gdb) Launch DeskPins",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/src/DeskPins.exe",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}/build/src",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "miDebuggerPath": "D:\\msys64\\ucrt64\\bin\\gdb.exe",
            "setupCommands": [
                { "text": "-enable-pretty-printing", "ignoreFailures": true },
                { "text": "-gdb-set disassembly-flavor intel", "ignoreFailures": true }
            ],
            "preLaunchTask": "CMake: Build"
        }
    ]
}
```

- [ ] **Step 11: Configure and build skeleton**

Run:
```bash
cd d:/githubprojects/pin && mkdir -p build && cd build && cmake -G "MinGW Makefiles" -DCMAKE_MAKE_PROGRAM=D:/msys64/ucrt64/bin/mingw32-make.exe -DCMAKE_CXX_COMPILER=D:/msys64/ucrt64/bin/g++.exe ..
```

Expected: CMake configures without errors. DLL compilation may fail (no dllmain.cpp yet) — that's fine, fix in Task 7.

- [ ] **Step 12: Commit**

```bash
git add -A
git commit -m "feat: project scaffolding with CMake, resources, and skeleton main"
```

---

### Task 2: Tray Manager

**Files:**
- Create: `src/traymanager.h`, `src/traymanager.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Create traymanager.h**

```cpp
#pragma once
#include <windows.h>

// Callbacks registered by main.cpp
struct TrayCallbacks {
    void (*onStartPin)(void);
    void (*onClearAll)(void);
    void (*onExit)(void);
};

bool TrayInit(HINSTANCE hInst, HWND hWnd, const TrayCallbacks* cb);
void TrayShutdown(HWND hWnd);
void TrayShowMenu(HWND hWnd);
void TrayOnDoubleClick(void);
```

- [ ] **Step 2: Create traymanager.cpp**

```cpp
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
    SetForegroundWindow(hWnd); // required for popup to dismiss properly

    int cmd = (int)TrackPopupMenu(hMenu,
        TPM_RETURNCMD | TPM_NONOTIFY,
        pt.x, pt.y, 0, hWnd, nullptr);

    PostMessageW(hWnd, WM_NULL, 0, 0); // required after TrackPopupMenu

    DestroyMenu(hMenu);

    switch (cmd) {
    case 1:
        if (g_cb.onStartPin) g_cb.onStartPin();
        break;
    case 2:
        if (g_cb.onClearAll) g_cb.onClearAll();
        break;
    case 3:
        if (g_cb.onExit) g_cb.onExit();
        break;
    }
}

void TrayOnDoubleClick(void) {
    if (g_cb.onStartPin) g_cb.onStartPin();
}
```

- [ ] **Step 3: Wire TrayManager into main.cpp**

```cpp
#include <windows.h>
#include "version.h"
#include "traymanager.h"

// Forward declarations
static HWND g_hWnd;

static void OnStartPin() {
    // Wired in Task 5
}

static void OnClearAll() {
    // Wired in Task 3
}

static void OnExit() {
    TrayShutdown(g_hWnd);
    PostQuitMessage(0);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TRAYICON:
        if (LOWORD(lp) == WM_RBUTTONUP) {
            TrayShowMenu(hWnd);
        } else if (LOWORD(lp) == WM_LBUTTONDBLCLK) {
            TrayOnDoubleClick();
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"DeskPins_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    // Register hidden window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DeskPins_Hidden";
    RegisterClassExW(&wc);

    g_hWnd = CreateWindowExW(0, L"DeskPins_Hidden", L"DeskPins",
        WS_OVERLAPPED, 0, 0, 100, 100, nullptr, nullptr, hInstance, nullptr);

    TrayCallbacks cb = { OnStartPin, OnClearAll, OnExit };
    if (!TrayInit(hInstance, g_hWnd, &cb)) {
        MessageBoxW(nullptr, L"托盘图标创建失败", APP_NAME, MB_ICONERROR);
        CloseHandle(hMutex);
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseHandle(hMutex);
    return 0;
}
```

- [ ] **Step 4: Build and verify tray icon**

Run: `cmake --build build`
Run: `build/src/DeskPins.exe`

Expected: Tray icon appears. Right-click → menu with three items. "退出" dismisses the icon and exits. Double-click does nothing visible yet (no pin controller).

- [ ] **Step 5: Commit**

```bash
git add src/traymanager.h src/traymanager.cpp src/main.cpp
git commit -m "feat: add system tray with right-click menu and double-click"
```

---

### Task 3: ProcList (Pinned Window Tracker)

**Files:**
- Create: `src/proclist.h`, `src/proclist.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Create proclist.h**

```cpp
#pragma once
#include <windows.h>
#include <vector>

struct PinEntry {
    HWND hwnd;
    DWORD threadId; // thread that owns the window (for injection cleanup)
    DWORD processId;
};

void ProcListAdd(HWND hwnd);
void ProcListRemove(HWND hwnd);
bool ProcListContains(HWND hwnd);
void ProcListClearAll(HWND notifyWnd);
const std::vector<PinEntry>& ProcListAll();
void ProcListStartPolling(HWND hWnd);
void ProcListStopPolling();
```

- [ ] **Step 2: Create proclist.cpp**

```cpp
#include "proclist.h"
#include "version.h"
#include <algorithm>

static std::vector<PinEntry> s_list;

void ProcListAdd(HWND hwnd) {
    // Avoid duplicates
    auto it = std::find_if(s_list.begin(), s_list.end(),
        [hwnd](const PinEntry& e) { return e.hwnd == hwnd; });
    if (it != s_list.end()) return;

    PinEntry e;
    e.hwnd = hwnd;
    e.threadId = GetWindowThreadProcessId(hwnd, &e.processId);
    s_list.push_back(e);
}

void ProcListRemove(HWND hwnd) {
    s_list.erase(
        std::remove_if(s_list.begin(), s_list.end(),
            [hwnd](const PinEntry& e) { return e.hwnd == hwnd; }),
        s_list.end());
}

bool ProcListContains(HWND hwnd) {
    return std::any_of(s_list.begin(), s_list.end(),
        [hwnd](const PinEntry& e) { return e.hwnd == hwnd; });
}

void ProcListClearAll(HWND notifyWnd) {
    for (auto& e : s_list) {
        if (IsWindow(e.hwnd)) {
            SetWindowPos(e.hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
    s_list.clear();
}

const std::vector<PinEntry>& ProcListAll() {
    return s_list;
}

static void CALLBACK PollTimerProc(HWND hWnd, UINT, UINT_PTR, DWORD) {
    // Remove dead windows from list
    s_list.erase(
        std::remove_if(s_list.begin(), s_list.end(),
            [](const PinEntry& e) { return !IsWindow(e.hwnd); }),
        s_list.end());
}

void ProcListStartPolling(HWND hWnd) {
    SetTimer(hWnd, TIMER_ID_POLL, POLL_INTERVAL_MS, PollTimerProc);
}

void ProcListStopPolling() {
    // Timer is per-hwnd; killed when window is destroyed
}
```

- [ ] **Step 3: Wire ProcList into main.cpp**

Update the `OnClearAll` function and `WinMain` to call `ProcListStartPolling`:

```cpp
#include "proclist.h"
// ...

static void OnClearAll() {
    ProcListClearAll(g_hWnd);
}

// In WinMain, after TrayInit:
ProcListStartPolling(g_hWnd);
```

- [ ] **Step 4: Build and verify**

Run: `cmake --build build`
Expected: Clean build. No runtime test yet (ProcList used by pin controller in Task 5).

- [ ] **Step 5: Commit**

```bash
git add src/proclist.h src/proclist.cpp src/main.cpp
git commit -m "feat: add pinned window tracker with dead-window cleanup"
```

---

### Task 4: DLL Injector

**Files:**
- Create: `src/dllinjector.h`, `src/dllinjector.cpp`

- [ ] **Step 1: Create dllinjector.h**

```cpp
#pragma once
#include <windows.h>

// Extract embedded DLL to %TEMP% and inject into target process.
// Returns true on success, false if injection failed (e.g., protected process).
bool InjectDLL(HINSTANCE hInst, DWORD processId, DWORD threadId);
```

- [ ] **Step 2: Create dllinjector.cpp**

```cpp
#include "dllinjector.h"
#include "version.h"
#include <cstdio>

static bool ExtractToTemp(HINSTANCE hInst, WCHAR* outPath, size_t outLen) {
    HRSRC hRes = FindResourceW(hInst, MAKEINTRESOURCEW(IDR_DLL_BIN), RT_RCDATA);
    if (!hRes) return false;

    HGLOBAL hGlobal = LoadResource(hInst, hRes);
    if (!hGlobal) return false;

    void* pData = LockResource(hGlobal);
    DWORD size = SizeofResource(hInst, hRes);
    if (!pData || size == 0) return false;

    WCHAR tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    swprintf(outPath, outLen, L"%sDeskPinsHook.dll", tempPath);

    // Write to temp
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

bool InjectDLL(HINSTANCE hInst, DWORD processId, DWORD threadId) {
    WCHAR dllPath[MAX_PATH];
    if (!ExtractToTemp(hInst, dllPath, MAX_PATH)) return false;

    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
        PROCESS_QUERY_INFORMATION,
        FALSE, processId);
    if (!hProcess) {
        // Possibly a protected process — silent failure
        return false;
    }

    size_t pathSize = (wcslen(dllPath) + 1) * sizeof(WCHAR);
    void* pRemoteMem = VirtualAllocEx(hProcess, nullptr, pathSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMem) {
        CloseHandle(hProcess);
        return false;
    }

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

    VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return true;
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: Clean build (linker may warn about missing DLL target — that's fine until Task 7).

- [ ] **Step 4: Commit**

```bash
git add src/dllinjector.h src/dllinjector.cpp
git commit -m "feat: add DLL resource extractor and CreateRemoteThread injector"
```

---

### Task 5: Pin Controller (Mouse Hook + Cursor)

**Files:**
- Create: `src/pincontroller.h`, `src/pincontroller.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Create pincontroller.h**

```cpp
#pragma once
#include <windows.h>

bool PinControllerStart(HINSTANCE hInst, HWND hWnd);
void PinControllerStop();
bool PinControllerIsActive();
```

- [ ] **Step 2: Create pincontroller.cpp**

```cpp
#include "pincontroller.h"
#include "dllinjector.h"
#include "proclist.h"
#include "version.h"
#include <uxtheme.h>

// Per-instance state
static HINSTANCE g_hInst;
static HWND g_hWnd;
static HHOOK g_hMouseHook;
static HCURSOR g_hPinCursor;
static HCURSOR g_hPrevCursor;
static bool g_active = false;

static HWND GetTargetWindow(POINT pt) {
    HWND hWnd = WindowFromPoint(pt);
    if (!hWnd) return nullptr;

    // Map to root owner window (skip child controls)
    HWND hOwner = GetAncestor(hWnd, GA_ROOTOWNER);
    return hOwner ? hOwner : hWnd;
}

static void EnterPinMode() {
    g_active = true;
    g_hPinCursor = LoadCursorW(g_hInst, MAKEINTRESOURCEW(IDC_PIN_CURSOR));
    g_hPrevCursor = SetCursor(g_hPinCursor);
    SetCapture(g_hWnd);

    RegisterHotKey(g_hWnd, HOTKEY_ID_ESC, 0, VK_ESCAPE);

    // System-wide low-level mouse hook
    g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseLLProc, g_hInst, 0);
}

static void ExitPinMode() {
    g_active = false;
    if (g_hMouseHook) {
        UnhookWindowsHookEx(g_hMouseHook);
        g_hMouseHook = nullptr;
    }
    UnregisterHotKey(g_hWnd, HOTKEY_ID_ESC);
    ReleaseCapture();
    SetCursor(g_hPrevCursor);
}

// Low-level mouse hook procedure.
// Called in the hook thread context; we post to our main window to do the work.
static LRESULT CALLBACK MouseLLProc(int nCode, WPARAM wp, LPARAM lp) {
    if (nCode < 0) return CallNextHookEx(nullptr, nCode, wp, lp);

    MSLLHOOKSTRUCT* p = (MSLLHOOKSTRUCT*)lp;

    if (wp == WM_MOUSEMOVE) {
        // Post for optional highlight (can be done here or in main thread)
    }

    if (wp == WM_LBUTTONDOWN) {
        HWND hTarget = GetTargetWindow(p->pt);
        if (hTarget) {
            // We're in the hook thread — post to main window to do the work
            PostMessageW(g_hWnd, WM_APP + 10, (WPARAM)hTarget, 0);
        }
    }

    return CallNextHookEx(nullptr, nCode, wp, lp);
}

// Called from main.cpp's WndProc when WM_APP+10 arrives
void PinControllerOnClick(HWND hTarget) {
    if (!g_active) return;

    // Skip UWP proxy frames
    WCHAR className[256];
    GetClassNameW(hTarget, className, 256);
    if (wcscmp(className, L"ApplicationFrameWindow") == 0) {
        // Still allow pinning but skip injection
        goto do_pin;
    }

    // Set window topmost
    SetWindowPos(hTarget, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    // Inject DLL for title bar icon
    DWORD pid = 0;
    DWORD tid = GetWindowThreadProcessId(hTarget, &pid);
    InjectDLL(g_hInst, pid, tid);

    ProcListAdd(hTarget);
    ExitPinMode();
    return;

do_pin:
    SetWindowPos(hTarget, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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
```

- [ ] **Step 3: Update main.cpp to wire pin controller**

In `main.cpp`, update `OnStartPin` and add handling for `WM_APP + 10`:

```cpp
#include "pincontroller.h"

// In WndProc, add:
case WM_APP + 10:
    PinControllerOnClick((HWND)wp);
    break;

case WM_HOTKEY:
    if (wp == HOTKEY_ID_ESC) {
        PinControllerStop();
    }
    break;

// Update OnStartPin:
static void OnStartPin() {
    PinControllerStart(hInstance, g_hWnd);
}
```

Full `main.cpp` at this point:

```cpp
#include <windows.h>
#include "version.h"
#include "traymanager.h"
#include "proclist.h"
#include "pincontroller.h"

static HWND g_hWnd;
static HINSTANCE g_hInstance;

static void OnStartPin() {
    PinControllerStart(g_hInstance, g_hWnd);
}

static void OnClearAll() {
    ProcListClearAll(g_hWnd);
}

static void OnExit() {
    PinControllerStop();
    TrayShutdown(g_hWnd);
    PostQuitMessage(0);
}

void PinControllerOnClick(HWND hTarget);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TRAYICON:
        if (LOWORD(lp) == WM_RBUTTONUP) {
            TrayShowMenu(hWnd);
        } else if (LOWORD(lp) == WM_LBUTTONDBLCLK) {
            TrayOnDoubleClick();
        }
        break;

    case WM_APP + 10:
        PinControllerOnClick((HWND)wp);
        break;

    case WM_HOTKEY:
        if (wp == HOTKEY_ID_ESC) {
            PinControllerStop();
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"DeskPins_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    g_hInstance = hInstance;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DeskPins_Hidden";
    RegisterClassExW(&wc);

    g_hWnd = CreateWindowExW(0, L"DeskPins_Hidden", L"DeskPins",
        WS_OVERLAPPED, 0, 0, 100, 100, nullptr, nullptr, hInstance, nullptr);

    TrayCallbacks cb = { OnStartPin, OnClearAll, OnExit };
    if (!TrayInit(hInstance, g_hWnd, &cb)) {
        CloseHandle(hMutex);
        return 1;
    }

    ProcListStartPolling(g_hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseHandle(hMutex);
    return 0;
}
```

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: Compiles. May have linker errors for missing DLL target — that's fine, fixed in Task 7.

- [ ] **Step 5: Commit**

```bash
git add src/pincontroller.h src/pincontroller.cpp src/main.cpp
git commit -m "feat: add pin mode controller with WH_MOUSE_LL hook and cursor swap"
```

---

### Task 6: DLL — DllMain + Subclassing

**Files:**
- Create: `dll/dllmain.cpp`, `dll/resource.h`, `dll/resource.rc`
- Create: `dll/titlebarpainter.h`, `dll/titlebarpainter.cpp` (stub)

- [ ] **Step 1: Create dll/resource.h**

```cpp
#pragma once
#define IDB_PIN_ICON 201
```

- [ ] **Step 2: Create dll/resource.rc**

```cpp
#include "resource.h"
// Pin icon will be a 16x16 bitmap drawn programmatically for now
// IDB_PIN_ICON BITMAP "pin.bmp"
// Placeholder: we'll generate icon data in titlebarpainter
```

- [ ] **Step 3: Create dll/titlebarpainter.h**

```cpp
#pragma once
#include <windows.h>

// Draw a pin icon on the title bar of the given window
void DrawPinOnTitleBar(HWND hwnd);

// Check if a non-client click hit the pin icon
// pt is in screen coordinates, returns true if it hit the icon
bool IsClickOnPinIcon(HWND hwnd, POINT ptScreen);

// Get the rect of the pin icon within the window (used for hit testing)
RECT GetPinIconRect(HWND hwnd);
```

- [ ] **Step 4: Create dll/titlebarpainter.cpp (stub)**

```cpp
#include "titlebarpainter.h"

void DrawPinOnTitleBar(HWND hwnd) {
    // TODO: Implement in Task 8
}

bool IsClickOnPinIcon(HWND hwnd, POINT ptScreen) {
    // TODO: Implement in Task 8
    return false;
}

RECT GetPinIconRect(HWND hwnd) {
    // TODO: Implement in Task 8
    return {0, 0, 0, 0};
}
```

- [ ] **Step 5: Create dll/dllmain.cpp**

```cpp
#include <windows.h>
#include <commctrl.h>
#include "titlebarpainter.h"

#pragma comment(linker, "/EXPORT:SubclassProc=_SubclassProc@0")

// Per-window data
struct SubclassData {
    UINT_PTR subclassId;
};

// Subclass window procedure
static LRESULT CALLBACK SubclassProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp,
                                     UINT_PTR subclassId, DWORD_PTR refData) {
    switch (msg) {
    case WM_NCPAINT: {
        // Let original window procedure paint first
        LRESULT result = DefSubclassProc(hWnd, msg, wp, lp);
        DrawPinOnTitleBar(hWnd);
        return result;
    }

    case WM_NCLBUTTONDOWN: {
        // Check if click is on our pin icon
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }; // screen coords for NC messages
        if (IsClickOnPinIcon(hWnd, pt)) {
            // Unpin: remove topmost style
            SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            // Force redraw of title bar (pin icon removed)
            SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE |
                SWP_NOZORDER | SWP_FRAMECHANGED);
            RemoveWindowSubclass(hWnd, SubclassProc, subclassId);
            // Self-unload after short delay to allow message processing
            // Use a timer as trampoline
            SetTimer(hWnd, 0xFFFF, 100, nullptr);
            return 0; // Swallow click
        }
        break;
    }

    case WM_TIMER:
        if (wp == 0xFFFF) {
            KillTimer(hWnd, 0xFFFF);
            FreeLibraryAndExitThread((HMODULE)refData, 0);
        }
        break;

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, SubclassProc, subclassId);
        break;
    }

    return DefSubclassProc(hWnd, msg, wp, lp);
}

// Heartbeat: check if main EXE is still alive, self-unload if not
static DWORD g_mainProcessId = 0;

static void CheckMainProcess() {
    if (g_mainProcessId == 0) return;
    HANDLE hProc = OpenProcess(SYNCHRONIZE, FALSE, g_mainProcessId);
    if (!hProc) return;
    DWORD ret = WaitForSingleObject(hProc, 0);
    CloseHandle(hProc);
    if (ret == WAIT_OBJECT_0) {
        // Main EXE died — we should self-unload.
        // This runs in DllMain context or subclass context.
    }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        // Read main process ID from environment variable set by injector
        WCHAR buf[32];
        if (GetEnvironmentVariableW(L"DESKPINS_MAIN_PID", buf, 32)) {
            g_mainProcessId = wcstoul(buf, nullptr, 10);
        }
        // NOTE: The actual subclassing will be triggered externally
        // by the injector sending a message or by calling an export.
        // For now, subclassing is triggered via WM_COPYDATA.
    }
    return TRUE;
}

// Export: called by injector to subclass a specific window
extern "C" __declspec(dllexport)
BOOL WINAPI AttachToWindow(HWND hWnd, HMODULE hMod) {
    return SetWindowSubclass(hWnd, SubclassProc, 1, (DWORD_PTR)hMod);
}
```

- [ ] **Step 6: Update dllinjector.cpp to call AttachToWindow after injection**

Update `InjectDLL` to:
1. Set `DESKPINS_MAIN_PID` env var before injection
2. After `CreateRemoteThread` + wait, call the exported `AttachToWindow` function

```cpp
// Added to InjectDLL, before CreateRemoteThread:

// Pass main PID to DLL via env var
WCHAR pidStr[32];
swprintf(pidStr, 32, L"%lu", GetCurrentProcessId());
SetEnvironmentVariableW(L"DESKPINS_MAIN_PID", pidStr);

// After WaitForSingleObject(hThread, 5000):
// Call AttachToWindow export in the injected DLL
// We need the target HWND — pass it from the caller. Update InjectDLL signature.
```

Update `dllinjector.h`:

```cpp
bool InjectDLL(HINSTANCE hInst, HWND hTarget, DWORD processId);
```

Update `dllinjector.cpp` — after the thread wait, find the loaded DLL in remote process and call the export:

```cpp
#include <TlHelp32.h>

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
    if (!pRemoteMem) {
        CloseHandle(hProcess);
        return false;
    }

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

    // Find the loaded DLL and call AttachToWindow
    HMODULE hRemoteDll = FindRemoteModule(hProcess, L"DeskPinsHook.dll");
    if (hRemoteDll) {
        // Get AttachToWindow address in our own process (same offset in remote)
        HMODULE hLocalDll = GetModuleHandleW(L"DeskPinsHook.dll");
        if (!hLocalDll) {
            // Load it locally to get export addresses
            hLocalDll = LoadLibraryW(dllPath);
        }
        if (hLocalDll) {
            FARPROC pLocal = GetProcAddress(hLocalDll, "AttachToWindow");
            if (pLocal) {
                // Calculate offset in DLL
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
            // Don't free hLocalDll if it's the one we just loaded
        }
    }

    VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return true;
}
```

- [ ] **Step 7: Update pincontroller.cpp to pass hTarget to InjectDLL**

Change the `InjectDLL` call in `PinControllerOnClick`:

```cpp
InjectDLL(g_hInst, hTarget, pid);
```

- [ ] **Step 8: Build**

Run: `cmake --build build`
Expected: Both DLL and EXE compile and link. The DLL has the `AttachToWindow` export. The EXE links against comctl32 for `SetWindowSubclass` support (actually only DLL uses that).

- [ ] **Step 9: Commit**

```bash
git add dll/ src/dllinjector.cpp src/dllinjector.h src/pincontroller.cpp
git commit -m "feat: add injected DLL with SetWindowSubclass and AttachToWindow export"
```

---

### Task 7: DLL — Title Bar Icon Drawing

**Files:**
- Rewrite: `dll/titlebarpainter.cpp` (replace stub with real implementation)

- [ ] **Step 1: Implement titlebarpainter.cpp**

```cpp
#include "titlebarpainter.h"
#include <cmath>

// Draw a small pin shape using GDI primitives
// Returns the bounding rect of the pin icon in window coordinates
static RECT DrawPinShape(HDC hdc, RECT rc) {
    // Pin icon: small circle (head) + thin triangle (point) + line (shaft)
    // We draw in a 16x16 logical area centered in rc, DPI-scaled

    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;

    // Get DPI to scale
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    int sz = MulDiv(12, dpi, 96); // 12 logical pixels at 96dpi -> scaled

    // Pin shape in a sz x sz bounding box, centered at (cx, cy)
    // Head: filled circle at top half
    HPEN hPen = CreatePen(PS_SOLID, MulDiv(1, dpi, 96), RGB(200, 30, 30));
    HBRUSH hBrush = CreateSolidBrush(RGB(220, 60, 60));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

    int r = sz / 3;
    int headY = cy - r / 2;
    Ellipse(hdc, cx - r, headY - r, cx + r, headY + r);

    // Point: small triangle below
    POINT pts[3] = {
        { cx, cy + sz / 2 },
        { cx - r / 2, headY + r },
        { cx + r / 2, headY + r }
    };
    Polygon(hdc, pts, 3);

    // Shaft: horizontal line through head
    HPEN hThinPen = CreatePen(PS_SOLID, MulDiv(1, dpi, 96), RGB(180, 20, 20));
    SelectObject(hdc, hThinPen);
    MoveToEx(hdc, cx - r - 2, headY, nullptr);
    LineTo(hdc, cx + r + 2, headY);
    DeleteObject(hThinPen);

    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);

    // Return bounding rect
    RECT result;
    result.left = cx - sz;
    result.top = cy - sz;
    result.right = cx + sz;
    result.bottom = cy + sz;
    return result;
}

void DrawPinOnTitleBar(HWND hwnd) {
    // Only draw on visible windows that are topmost
    if (!IsWindowVisible(hwnd)) return;
    LONG exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (!(exStyle & WS_EX_TOPMOST)) return;

    RECT rcWindow;
    GetWindowRect(hwnd, &rcWindow);

    // Get title bar height
    int titleBarHeight = GetSystemMetrics(SM_CYCAPTION);
    // With DPI scaling, title bar height may differ
    UINT dpi = GetDpiForWindow(hwnd);
    titleBarHeight = MulDiv(titleBarHeight, (int)dpi, 96);

    // Adjust for window border/frame
    int frameX = GetSystemMetrics(SM_CXFRAME);
    int frameY = GetSystemMetrics(SM_CYFRAME);
    int paddedBorder = 0;
    if (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_THICKFRAME) {
        paddedBorder = GetSystemMetrics(SM_CXPADDEDBORDER);
    }

    titleBarHeight += frameY + paddedBorder;

    // Pin icon: top-right corner, left of system buttons
    // Reserve space for 3 system buttons (minimize, maximize, close)
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
    int titleBarHeight = GetSystemMetrics(SM_CYCAPTION);
    titleBarHeight = MulDiv(titleBarHeight, (int)dpi, 96);
    int frameY = GetSystemMetrics(SM_CYFRAME);
    int paddedBorder = 0;
    if (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_THICKFRAME) {
        paddedBorder = GetSystemMetrics(SM_CXPADDEDBORDER);
    }
    titleBarHeight += frameY + paddedBorder;

    int sysBtnWidth = GetSystemMetrics(SM_CXSIZE);
    int iconSize = MulDiv(16, (int)dpi, 96);
    int marginX = sysBtnWidth * 3 + MulDiv(8, (int)dpi, 96);
    int marginY = (titleBarHeight - iconSize) / 2;

    // Icon rect in screen coordinates
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
```

- [ ] **Step 2: Update dllmain.cpp DllMain to call InitCommonControls**

Add to `DLL_PROCESS_ATTACH`:

```cpp
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        InitCommonControls(); // Required for some comctl32 features
        WCHAR buf[32];
        if (GetEnvironmentVariableW(L"DESKPINS_MAIN_PID", buf, 32)) {
            g_mainProcessId = wcstoul(buf, nullptr, 10);
        }
    }
```

- [ ] **Step 3: Build and verify compilation**

Run: `cmake --build build`
Expected: Both targets compile and link cleanly.

- [ ] **Step 4: Commit**

```bash
git add dll/titlebarpainter.cpp dll/dllmain.cpp
git commit -m "feat: implement GDI title bar pin icon drawing with DPI awareness"
```

---

### Task 8: Integration — End-to-End Wiring & Verification

**Files:**
- Verify: all files are complete and consistent
- Possibly fix: cross-file interface mismatches

- [ ] **Step 1: Verify all includes and exports are consistent**

Check across all files:
- `version.h` constants match usage in `traymanager.cpp`, `pincontroller.cpp`, `proclist.cpp`, `dllinjector.cpp`
- `pincontroller.h` declares `PinControllerOnClick` for `main.cpp` usage
- `InjectDLL` signature updated in `dllinjector.h` to `(HINSTANCE, HWND, DWORD)`

Add missing declaration to `pincontroller.h`:

```cpp
// Add to pincontroller.h:
void PinControllerOnClick(HWND hTarget);
```

Remove `void PinControllerOnClick(HWND hTarget);` forward declaration from `main.cpp` — it's now in the header.

- [ ] **Step 2: Update files for consistency**

In `src/pincontroller.cpp`, add a guard for the `goto do_pin` label — C++ doesn't allow jumping over variable declarations. Rearrange:

```cpp
void PinControllerOnClick(HWND hTarget) {
    if (!g_active) return;

    // Skip UWP proxy frames
    WCHAR className[256];
    GetClassNameW(hTarget, className, 256);
    bool isUwp = (wcscmp(className, L"ApplicationFrameWindow") == 0);

    // Set window topmost
    SetWindowPos(hTarget, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    // Inject DLL for title bar icon (skip UWP windows)
    if (!isUwp) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hTarget, &pid);
        InjectDLL(g_hInst, hTarget, pid);
    }

    ProcListAdd(hTarget);
    ExitPinMode();
}
```

- [ ] **Step 3: Full build**

Run: `cmake --build build --config Release`
Expected: Zero errors, zero warnings.

- [ ] **Step 4: Manual smoke test**

Run: `build/src/DeskPins.exe`

1. Tray icon appears ✓
2. Right-click shows menu ✓
3. "开始钉选" enters pin mode → cursor changes
4. Click on a Notepad window → Notepad goes topmost
5. Open another window behind Notepad → Notepad stays on top
6. Pin icon drawn on Notepad title bar → click it → Notepad unpinned
7. "清除所有置顶" → all windows restored
8. "退出" → tray icon removed, program exits

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat: integrate all modules, fix consistency issues, end-to-end verification"
```

---

### Task 9: Asset Cleanup

**Files:**
- Replace: `assets/app.ico` (proper icon)
- Replace: `assets/pin.cur` (proper cursor)

- [ ] **Step 1: Create proper app icon**

Generate or design a 32x32 RGBA icon with a pin/thumbtack motif. Use a resource editor or generate programmatically with GDI+ if a designer is not available. As a temporary measure, compile with a simple placeholder drawn in code.

- [ ] **Step 2: Create proper pin cursor**

Generate a .cur file with a pin/thumbtack shape and hotspot at the tip (x=4, y=4). The cursor should be 32x32 with alpha channel.

- [ ] **Step 3: Build and verify icons load correctly**

Run: `cmake --build build --config Release`
Expected: Tray icon visible in tray, cursor visible during pin mode.

- [ ] **Step 4: Commit**

```bash
git add assets/
git commit -m "feat: add proper app icon and pin cursor assets"
```

---

## Verification Checklist

After all tasks complete, verify end-to-end:

1. `cmake --build build --config Release` — zero errors
2. Run `DeskPins.exe` — tray icon appears, no console window
3. Double-click tray icon → pin cursor active
4. ESC → exits pin mode cleanly
5. Click Notepad → Notepad is TOPMOST, pin icon visible on title bar
6. Open another app behind Notepad → Notepad stays on top
7. Click pin icon on Notepad title bar → Notepad no longer TOPMOST
8. Pin 3 windows → "清除所有置顶" → all restored to normal
9. Kill DeskPins.exe from Task Manager → window list cleared, no stale state
10. Run second instance → ignored (single-instance mutex)
