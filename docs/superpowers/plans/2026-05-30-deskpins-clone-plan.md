# DeskPins Clone Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a portable single-file EXE that mimics DeskPins — system tray app with pin mode, window topmost toggle, and title bar pin icon via DLL injection.

**Architecture:** C# .NET 8 WinForms EXE for tray/UI/logic, minimal C++ DLL injected via `CreateRemoteThread` for title bar icon drawing. DLL embedded as byte[] resource in EXE, extracted to temp at runtime. Communication: EXE writes PID+HWND to named shared memory BEFORE calling `CreateRemoteThread(LoadLibraryW)`; DLL's `DllMain` reads shared memory, spawns worker thread to subclass the window. DLL heartbeat detects both EXE death and unpin.

**Tech Stack:** C# 12 / .NET 8 / WinForms / P/Invoke (user32, kernel32, shell32) / C++20 MinGW-w64 -static

---

### Task 1: Project Scaffolding

**Files:**
- Create: `DeskPins/DeskPins.csproj`
- Create: `DeskPins/Resources/.gitkeep`
- Create: `HookDLL/CMakeLists.txt`
- Create: `publish.bat`
- Delete: `CMakeLists.txt` (old root-level, replaced)

- [ ] **Step 1: Create DeskPins.csproj**

```xml
<Project Sdk="Microsoft.NET.Sdk">

  <PropertyGroup>
    <OutputType>WinExe</OutputType>
    <TargetFramework>net8.0-windows</TargetFramework>
    <UseWindowsForms>true</UseWindowsForms>
    <AssemblyName>DeskPins</AssemblyName>
    <Nullable>enable</Nullable>
  </PropertyGroup>

  <ItemGroup>
    <EmbeddedResource Include="Resources\DeskPinsHook.dll" />
  </ItemGroup>

</Project>
```

- [ ] **Step 2: Create HookDLL/CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
project(DeskPinsHook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -s -static")

add_definitions(-DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00)

add_library(DeskPinsHook SHARED dllmain.cpp)
target_link_libraries(DeskPinsHook PRIVATE gdi32 user32 comctl32)
```

- [ ] **Step 3: Create Resources directory placeholder**

```bash
mkdir -p DeskPins/Resources && touch DeskPins/Resources/.gitkeep
```

- [ ] **Step 4: Create publish.bat**

```batch
@echo off
:: Build native DLL
cd HookDLL
cmake -B build -G "MinGW Makefiles"
cmake --build build --config Release
copy build\DeskPinsHook.dll ..\DeskPins\Resources\
cd ..

:: Publish single-file EXE
dotnet publish DeskPins\DeskPins.csproj -c Release ^
  -p:PublishSingleFile=true ^
  -p:SelfContained=false ^
  -p:PublishTrimmed=true ^
  -o publish

echo Done: publish\DeskPins.exe
```

- [ ] **Step 5: Delete old root CMakeLists.txt**

```bash
rm CMakeLists.txt
```

- [ ] **Step 6: Commit**

```bash
git add DeskPins/DeskPins.csproj DeskPins/Resources/.gitkeep HookDLL/CMakeLists.txt publish.bat
git rm CMakeLists.txt
git commit -m "feat: scaffold DeskPins C# project and HookDLL CMake config"
```

---

### Task 2: NativeMethods.cs — All P/Invoke Declarations

**Files:**
- Create: `DeskPins/NativeMethods.cs`

- [ ] **Step 1: Write NativeMethods.cs**

```csharp
using System.Runtime.InteropServices;

namespace DeskPins;

internal static partial class NativeMethods
{
    // ── user32.dll ──────────────────────────────────────

    [LibraryImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool SetWindowPos(
        IntPtr hWnd, IntPtr hWndInsertAfter,
        int X, int Y, int cx, int cy,
        uint uFlags);

    [LibraryImport("user32.dll", SetLastError = true)]
    internal static partial IntPtr SetWindowsHookExW(
        int idHook, HookProc lpfn, IntPtr hMod, uint dwThreadId);

    [LibraryImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool UnhookWindowsHookEx(IntPtr hhk);

    [LibraryImport("user32.dll", SetLastError = true)]
    internal static partial IntPtr CallNextHookEx(
        IntPtr hhk, int nCode, IntPtr wParam, IntPtr lParam);

    [LibraryImport("user32.dll")]
    internal static partial IntPtr WindowFromPoint(POINT Point);

    [LibraryImport("user32.dll")]
    internal static partial uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [LibraryImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool GetCursorPos(out POINT lpPoint);

    [LibraryImport("user32.dll")]
    internal static partial IntPtr LoadCursorFromFileW([MarshalAs(UnmanagedType.LPWStr)] string lpFileName);

    [LibraryImport("user32.dll")]
    internal static partial IntPtr SetCursor(IntPtr hCursor);

    [LibraryImport("user32.dll")]
    internal static partial IntPtr GetForegroundWindow();

    [LibraryImport("user32.dll")]
    internal static partial uint GetWindowLongW(IntPtr hWnd, int nIndex);

    [LibraryImport("user32.dll")]
    internal static partial uint GetDpiForWindow(IntPtr hwnd);

    [LibraryImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool SetProcessDpiAwarenessContext(int value);

    [LibraryImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool DestroyCursor(IntPtr hCursor);

    [LibraryImport("user32.dll")]
    internal static partial uint RegisterWindowMessageW(
        [MarshalAs(UnmanagedType.LPWStr)] string lpString);

    [LibraryImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool PostMessageW(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [LibraryImport("user32.dll")]
    internal static partial IntPtr SetWinEventHook(
        uint eventMin, uint eventMax, IntPtr hmodWinEventProc,
        WinEventProc lpfnWinEventProc, uint idProcess, uint idThread, uint dwFlags);

    [LibraryImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool UnhookWinEvent(IntPtr hWinEventHook);

    // ── shell32.dll ─────────────────────────────────────

    [LibraryImport("shell32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool Shell_NotifyIconW(uint dwMessage, ref NOTIFYICONDATAW lpData);

    // ── kernel32.dll ────────────────────────────────────

    [LibraryImport("kernel32.dll", SetLastError = true)]
    internal static partial IntPtr OpenProcess(
        uint dwDesiredAccess,
        [MarshalAs(UnmanagedType.Bool)] bool bInheritHandle,
        uint dwProcessId);

    [LibraryImport("kernel32.dll", SetLastError = true)]
    internal static partial IntPtr VirtualAllocEx(
        IntPtr hProcess, IntPtr lpAddress, uint dwSize,
        uint flAllocationType, uint flProtect);

    [LibraryImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool VirtualFreeEx(
        IntPtr hProcess, IntPtr lpAddress, uint dwSize, uint dwFreeType);

    [LibraryImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool WriteProcessMemory(
        IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer,
        uint nSize, out uint lpNumberOfBytesWritten);

    [LibraryImport("kernel32.dll", SetLastError = true)]
    internal static partial IntPtr CreateRemoteThread(
        IntPtr hProcess, IntPtr lpThreadAttributes, uint dwStackSize,
        IntPtr lpStartAddress, IntPtr lpParameter, uint dwCreationFlags,
        out uint lpThreadId);

    [LibraryImport("kernel32.dll", SetLastError = true)]
    internal static partial uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

    [LibraryImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool CloseHandle(IntPtr hObject);

    [LibraryImport("kernel32.dll", SetLastError = true, StringMarshalling = StringMarshalling.Utf16)]
    internal static partial IntPtr GetModuleHandleW(string? lpModuleName);

    [LibraryImport("kernel32.dll", SetLastError = true)]
    internal static partial IntPtr GetProcAddress(IntPtr hModule, IntPtr lpProcName);

    [LibraryImport("kernel32.dll", SetLastError = true)]
    internal static partial IntPtr CreateMutexW(IntPtr lpMutexAttributes,
        [MarshalAs(UnmanagedType.Bool)] bool bInitialOwner,
        [MarshalAs(UnmanagedType.LPWStr)] string lpName);

    [LibraryImport("kernel32.dll", SetLastError = true)]
    internal static partial uint GetLastError();

    [LibraryImport("kernel32.dll", SetLastError = true)]
    internal static partial uint ResumeThread(IntPtr hThread);

    // ── Structs ─────────────────────────────────────────

    [StructLayout(LayoutKind.Sequential)]
    internal struct POINT
    {
        public int x;
        public int y;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct NOTIFYICONDATAW
    {
        public uint cbSize;
        public IntPtr hWnd;
        public uint uID;
        public uint uFlags;
        public uint uCallbackMessage;
        public IntPtr hIcon;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string szTip;
        public uint dwState;
        public uint dwStateMask;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string szInfo;
        public uint uVersionOrTimeout;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
        public string szInfoTitle;
        public uint dwInfoFlags;
    }

    // ── Delegates ───────────────────────────────────────

    internal delegate IntPtr HookProc(int nCode, IntPtr wParam, IntPtr lParam);

    internal delegate void WinEventProc(
        IntPtr hWinEventHook, uint eventType, IntPtr hwnd,
        int idObject, int idChild, uint dwEventThread, uint dwmsEventTime);

    // ── Constants ───────────────────────────────────────

    internal static IntPtr HWND_TOPMOST = new(-1);
    internal static IntPtr HWND_NOTOPMOST = new(-2);

    internal const uint SWP_NOSIZE = 0x0001;
    internal const uint SWP_NOMOVE = 0x0002;
    internal const uint SWP_NOACTIVATE = 0x0010;
    internal const uint SWP_SHOWWINDOW = 0x0040;

    internal const int WH_MOUSE_LL = 14;
    internal const int WH_KEYBOARD_LL = 13;

    internal const uint WM_LBUTTONDOWN = 0x0201;
    internal const uint WM_RBUTTONDOWN = 0x0204;
    internal const uint WM_MOUSEMOVE = 0x0200;

    internal const int GWL_EXSTYLE = -20;
    internal const uint WS_EX_TOPMOST = 0x00000008;

    internal const uint PROCESS_ALL_ACCESS = 0x001F0FFF;
    internal const uint PROCESS_CREATE_THREAD = 0x0002;
    internal const uint PROCESS_QUERY_INFORMATION = 0x0400;
    internal const uint PROCESS_VM_OPERATION = 0x0008;
    internal const uint PROCESS_VM_WRITE = 0x0020;
    internal const uint PROCESS_VM_READ = 0x0010;

    internal const uint MEM_COMMIT = 0x1000;
    internal const uint MEM_RESERVE = 0x2000;
    internal const uint MEM_RELEASE = 0x8000;
    internal const uint PAGE_READWRITE = 0x04;

    internal const uint INFINITE = 0xFFFFFFFF;

    internal const uint NIM_ADD = 0x00000000;
    internal const uint NIM_MODIFY = 0x00000001;
    internal const uint NIM_DELETE = 0x00000002;
    internal const uint NIF_MESSAGE = 0x00000001;
    internal const uint NIF_ICON = 0x00000002;
    internal const uint NIF_TIP = 0x00000004;
    internal const uint NIF_INFO = 0x00000010;
    internal const uint NIIF_INFO = 0x00000001;

    internal const uint WM_TRAYICON = 0x8001;
    internal const uint WM_APP_UNPIN_ALL = 0x8002;

    internal const uint EVENT_OBJECT_DESTROY = 0x8001;
    internal const uint WINEVENT_OUTOFCONTEXT = 0x0000;

    internal const int DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = -4;

    internal const int ERROR_ALREADY_EXISTS = 183;
}
```

- [ ] **Step 2: Commit**

```bash
git add DeskPins/NativeMethods.cs
git commit -m "feat: add all P/Invoke declarations in NativeMethods.cs"
```

---

### Task 3: Native Hook DLL

**Files:**
- Create: `HookDLL/dllmain.cpp`

- [ ] **Step 1: Write dllmain.cpp**

```cpp
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#pragma comment(linker, "/EXPORT:InstallHook=InstallHook")

// ── Global state (one DLL instance per pinned window) ──

static HWND g_hWnd = nullptr;
static UINT_PTR g_timerId = 0;
static HICON g_hPinIcon = nullptr;
static int g_iconSize = 16;
static DWORD g_mainProcessId = 0;

// ── Shared memory: EXE writes PID+HWND before injecting ──

static const WCHAR SHARED_MEM_NAME[] = L"DeskPins_SharedMem";

struct SharedData
{
    DWORD exePid;
    HWND hwnd;
};

// ── Create a simple pin-shaped icon (solid red rectangle as placeholder) ──

static void CreatePinIcon(HWND hwnd)
{
    int dpi = GetDpiForWindow(hwnd);
    g_iconSize = MulDiv(16, dpi, 96);

    HDC hdc = GetDC(hwnd);
    HBITMAP hbmColor = CreateCompatibleBitmap(hdc, g_iconSize, g_iconSize);
    HBITMAP hbmMask = CreateBitmap(g_iconSize, g_iconSize, 1, 1, nullptr);
    ReleaseDC(hwnd, hdc);

    HDC hdcMem = CreateCompatibleDC(nullptr);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hbmColor);

    RECT rc = {0, 0, g_iconSize, g_iconSize};
    HBRUSH hBr = CreateSolidBrush(RGB(200, 30, 30));
    FillRect(hdcMem, &rc, hBr);
    DeleteObject(hBr);

    SelectObject(hdcMem, hOld);
    DeleteDC(hdcMem);

    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmColor = hbmColor;
    ii.hbmMask = hbmMask;
    ii.xHotspot = 0;
    ii.yHotspot = 0;

    if (g_hPinIcon) DestroyIcon(g_hPinIcon);
    g_hPinIcon = CreateIconIndirect(&ii);

    DeleteObject(hbmColor);
    DeleteObject(hbmMask);
}

// ── Subclass procedure ──

static LRESULT CALLBACK SubclassProc(
    HWND hWnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR)
{
    switch (uMsg)
    {
    case WM_NCPAINT:
    {
        LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (!g_hPinIcon) CreatePinIcon(hWnd);

        HDC hdc = GetWindowDC(hWnd);
        if (hdc)
        {
            int titleBarH = GetSystemMetrics(SM_CYCAPTION)
                          + GetSystemMetrics(SM_CYFRAME)
                          + GetSystemMetrics(SM_CXPADDEDBORDER);
            int frameX = GetSystemMetrics(SM_CXFRAME)
                       + GetSystemMetrics(SM_CXPADDEDBORDER);

            RECT rc;
            GetWindowRect(hWnd, &rc);
            int iconX = (rc.right - rc.left) - frameX - g_iconSize * 4;
            int iconY = GetSystemMetrics(SM_CXPADDEDBORDER)
                      + (titleBarH - frameX - g_iconSize) / 2;

            DrawIconEx(hdc, iconX, iconY, g_hPinIcon,
                g_iconSize, g_iconSize, 0, nullptr, DI_NORMAL);
            ReleaseDC(hWnd, hdc);
        }
        return result;
    }

    case WM_NCLBUTTONDOWN:
    {
        if (wParam == HTCAPTION)
        {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT rc;
            GetWindowRect(hWnd, &rc);
            int frameX = GetSystemMetrics(SM_CXFRAME)
                       + GetSystemMetrics(SM_CXPADDEDBORDER);
            int titleBarH = GetSystemMetrics(SM_CYCAPTION)
                          + GetSystemMetrics(SM_CYFRAME)
                          + GetSystemMetrics(SM_CXPADDEDBORDER);
            int iconX = (rc.right - rc.left) - frameX - g_iconSize * 4;
            int iconY = GetSystemMetrics(SM_CXPADDEDBORDER)
                      + (titleBarH - frameX - g_iconSize) / 2;

            ScreenToClient(hWnd, &pt);
            if (pt.x >= iconX && pt.x <= iconX + g_iconSize &&
                pt.y >= iconY && pt.y <= iconY + g_iconSize)
            {
                SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                RemoveWindowSubclass(hWnd, SubclassProc, uIdSubclass);
                if (g_hPinIcon) { DestroyIcon(g_hPinIcon); g_hPinIcon = nullptr; }
                if (g_timerId) { KillTimer(nullptr, g_timerId); g_timerId = 0; }
                FreeLibraryAndExitThread(
                    GetModuleHandleW(L"DeskPinsHook.dll"), 0);
                return 0;
            }
        }
        break;
    }

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, SubclassProc, uIdSubclass);
        if (g_hPinIcon) { DestroyIcon(g_hPinIcon); g_hPinIcon = nullptr; }
        if (g_timerId) { KillTimer(nullptr, g_timerId); g_timerId = 0; }
        FreeLibraryAndExitThread(GetModuleHandleW(L"DeskPinsHook.dll"), 0);
        return 0;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// ── Heartbeat: checks if EXE died or window got unpinned from EXE side ──

static VOID CALLBACK HeartbeatTimer(HWND, UINT, UINT_PTR, DWORD)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, g_mainProcessId);
    bool exeDead = (hProcess == nullptr);
    if (!exeDead) CloseHandle(hProcess);

    bool unpinned = !(GetWindowLongW(g_hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST);

    if (exeDead || unpinned)
    {
        if (g_hPinIcon) { DestroyIcon(g_hPinIcon); g_hPinIcon = nullptr; }
        if (g_timerId) { KillTimer(nullptr, g_timerId); g_timerId = 0; }
        RemoveWindowSubclass(g_hWnd, SubclassProc, 1);
        FreeLibraryAndExitThread(GetModuleHandleW(L"DeskPinsHook.dll"), 0);
    }
}

// ── Worker thread: subclass the window and start heartbeat ──

static DWORD WINAPI WorkerThread(LPVOID)
{
    if (!IsWindow(g_hWnd)) return 1;

    SetWindowSubclass(g_hWnd, SubclassProc, 1, 0);
    g_timerId = SetTimer(nullptr, 0, 2000, HeartbeatTimer);

    SetWindowPos(g_hWnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    // Message loop for the timer (timers need a message pump)
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

// ── Exported: alternative path — EXE can call this via second CreateRemoteThread ──

extern "C" __declspec(dllexport) DWORD WINAPI InstallHook(LPVOID lpParam)
{
    g_hWnd = (HWND)lpParam;
    WorkerThread(nullptr);
    return 0;
}

// ── DllMain: read shared memory, spawn worker thread ──

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        HANDLE hMapping = OpenFileMappingW(FILE_MAP_READ, FALSE, SHARED_MEM_NAME);
        if (hMapping)
        {
            SharedData* pData = (SharedData*)MapViewOfFile(
                hMapping, FILE_MAP_READ, 0, 0, sizeof(SharedData));
            if (pData)
            {
                g_mainProcessId = pData->exePid;
                g_hWnd = pData->hwnd;
                UnmapViewOfFile(pData);

                // Spawn worker thread to subclass (can't do it from DllMain)
                HANDLE hThread = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
                if (hThread) CloseHandle(hThread);
            }
            CloseHandle(hMapping);
        }
    }
    return TRUE;
}
```

- [ ] **Step 2: Build native DLL**

```bash
cd HookDLL
cmake -B build -G "MinGW Makefiles"
cmake --build build --config Release
ls -la build/DeskPinsHook.dll
```

Expected: `DeskPinsHook.dll` exists (~20-30KB with -static)

- [ ] **Step 3: Copy DLL to C# resources and commit**

```bash
cp HookDLL/build/DeskPinsHook.dll DeskPins/Resources/
git add HookDLL/dllmain.cpp HookDLL/build/DeskPinsHook.dll DeskPins/Resources/DeskPinsHook.dll
git commit -m "feat: implement native DLL with subclassing and heartbeat"
```

---

### Task 4: TrayManager.cs

**Files:**
- Create: `DeskPins/TrayManager.cs`

- [ ] **Step 1: Write TrayManager.cs**

```csharp
namespace DeskPins;

internal class TrayManager : IDisposable
{
    private readonly NotifyIcon _notifyIcon;
    private readonly ContextMenuStrip _menu;
    private readonly ToolStripMenuItem _pinModeItem;
    private readonly ToolStripMenuItem _unpinAllItem;

    public event Action? PinModeRequested;
    public event Action? UnpinAllRequested;
    public event Action? ExitRequested;

    public TrayManager()
    {
        _menu = new ContextMenuStrip();

        _pinModeItem = new ToolStripMenuItem("开始钉选");
        _pinModeItem.Click += (_, _) => PinModeRequested?.Invoke();

        _unpinAllItem = new ToolStripMenuItem("清除所有置顶");
        _unpinAllItem.Click += (_, _) => UnpinAllRequested?.Invoke();

        var aboutItem = new ToolStripMenuItem("关于");
        aboutItem.Click += (_, _) =>
            MessageBox.Show("DeskPins — 窗口置顶工具\n仿 DeskPins 练手项目",
                "关于", MessageBoxButtons.OK, MessageBoxIcon.Information);

        var exitItem = new ToolStripMenuItem("退出");
        exitItem.Click += (_, _) => ExitRequested?.Invoke();

        _menu.Items.Add(_pinModeItem);
        _menu.Items.Add(_unpinAllItem);
        _menu.Items.Add(new ToolStripSeparator());
        _menu.Items.Add(aboutItem);
        _menu.Items.Add(new ToolStripSeparator());
        _menu.Items.Add(exitItem);

        _notifyIcon = new NotifyIcon
        {
            Icon = LoadTrayIcon(),
            Text = "DeskPins",
            ContextMenuStrip = _menu,
            Visible = true
        };
        _notifyIcon.DoubleClick += (_, _) => PinModeRequested?.Invoke();
    }

    public void SetPinModeActive(bool active)
    {
        _pinModeItem.Enabled = !active;
        _notifyIcon.Text = active ? "DeskPins — 钉选模式 (ESC 取消)" : "DeskPins";
    }

    public void UpdatePinnedCount(int count)
    {
        _unpinAllItem.Enabled = count > 0;
    }

    private static Icon LoadTrayIcon()
    {
        // Try embedded resource first, then fall back to default icon
        try
        {
            var asm = typeof(TrayManager).Assembly;
            using var stream = asm.GetManifestResourceStream("DeskPins.Resources.app.ico");
            if (stream != null)
                return new Icon(stream);
        }
        catch { /* fall through */ }
        return SystemIcons.Application;
    }

    public void Dispose()
    {
        _notifyIcon.Visible = false;
        _notifyIcon.Dispose();
        _menu.Dispose();
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add DeskPins/TrayManager.cs
git commit -m "feat: implement tray icon with context menu"
```

---

### Task 5: PinController.cs

**Files:**
- Create: `DeskPins/PinController.cs`

- [ ] **Step 1: Write PinController.cs**

```csharp
using System.Runtime.InteropServices;
using static DeskPins.NativeMethods;

namespace DeskPins;

internal class PinController : IDisposable
{
    private IntPtr _mouseHook;
    private IntPtr _pinCursor;
    private IntPtr _normalCursor;
    private HookProc? _mouseProc;
    private bool _isActive;

    public event Action<IntPtr>? WindowClicked;

    public bool IsActive => _isActive;

    public PinController()
    {
        _normalCursor = Cursors.Arrow.Handle;
    }

    public void EnterPinMode(string pinCursorPath)
    {
        if (_isActive) return;
        _isActive = true;

        // Load pin cursor
        try
        {
            _pinCursor = LoadCursorFromFileW(pinCursorPath);
            if (_pinCursor == IntPtr.Zero)
                _pinCursor = Cursors.Cross.Handle; // fallback
        }
        catch
        {
            _pinCursor = Cursors.Cross.Handle;
        }

        // Install low-level mouse hook
        _mouseProc = MouseHookProc;
        _mouseHook = SetWindowsHookExW(WH_MOUSE_LL, _mouseProc,
            GetModuleHandleW(null!), 0);

        if (_mouseHook == IntPtr.Zero)
        {
            _isActive = false;
            throw new InvalidOperationException(
                $"Failed to install mouse hook. Error: {GetLastError()}");
        }
    }

    public void ExitPinMode()
    {
        if (!_isActive) return;
        _isActive = false;

        if (_mouseHook != IntPtr.Zero)
        {
            UnhookWindowsHookEx(_mouseHook);
            _mouseHook = IntPtr.Zero;
        }

        if (_pinCursor != IntPtr.Zero && _pinCursor != Cursors.Cross.Handle)
        {
            DestroyCursor(_pinCursor);
            _pinCursor = IntPtr.Zero;
        }

        Cursor.Current = Cursors.Arrow;
    }

    private IntPtr MouseHookProc(int nCode, IntPtr wParam, IntPtr lParam)
    {
        if (nCode >= 0)
        {
            uint msg = (uint)wParam;

            // Set pin cursor while in pin mode
            if (_pinCursor != IntPtr.Zero)
                SetCursor(_pinCursor);

            if (msg == WM_LBUTTONDOWN)
            {
                GetCursorPos(out POINT pt);
                IntPtr hWnd = WindowFromPoint(pt);

                // Walk up to find root window for the process
                hWnd = GetAncestor(hWnd, 2); // GA_ROOT

                if (hWnd != IntPtr.Zero && IsWindowValid(hWnd))
                {
                    WindowClicked?.Invoke(hWnd);
                    return (IntPtr)1; // Swallow this click
                }
            }
            else if (msg == WM_RBUTTONDOWN)
            {
                // Right-click cancels pin mode
                ExitPinMode();
                return (IntPtr)1;
            }
        }

        return CallNextHookEx(_mouseHook, nCode, wParam, lParam);
    }

    private static IntPtr GetAncestor(IntPtr hwnd, uint gaFlags)
    {
        // GetAncestor not always available via LibraryImport easily,
        // use GetDesktopWindow as root check
        IntPtr parent = GetParent(hwnd);
        while (parent != IntPtr.Zero)
        {
            hwnd = parent;
            parent = GetParent(hwnd);
        }
        return hwnd;
    }

    [DllImport("user32.dll")]
    private static extern IntPtr GetParent(IntPtr hWnd);

    [DllImport("user32.dll")]
    private static extern IntPtr GetDesktopWindow();

    private static bool IsWindowValid(IntPtr hwnd)
    {
        // Skip desktop, taskbar, our own window
        if (hwnd == GetDesktopWindow()) return false;
        if (hwnd == GetShellWindow()) return false;

        // Skip invisible windows
        uint style = GetWindowLongW(hwnd, -16); // GWL_STYLE
        if ((style & 0x10000000) == 0) return false; // WS_VISIBLE

        return true;
    }

    [DllImport("user32.dll")]
    private static extern IntPtr GetShellWindow();

    public void Dispose()
    {
        ExitPinMode();
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add DeskPins/PinController.cs
git commit -m "feat: implement pin mode with global mouse hook"
```

---

### Task 6: DllInjector.cs

**Files:**
- Create: `DeskPins/DllInjector.cs`

- [ ] **Step 1: Write DllInjector.cs**

```csharp
using System.Reflection;
using System.Runtime.InteropServices;
using static DeskPins.NativeMethods;

namespace DeskPins;

internal class DllInjector
{
    private string _dllPath = "";

    public DllInjector()
    {
        ExtractDll();
    }

    private void ExtractDll()
    {
        var asm = Assembly.GetExecutingAssembly();
        string resourceName = "DeskPins.Resources.DeskPinsHook.dll";

        using var stream = asm.GetManifestResourceStream(resourceName);
        if (stream == null)
        {
            System.Diagnostics.Debug.WriteLine("DLL resource not found, injection disabled");
            return;
        }

        _dllPath = Path.Combine(Path.GetTempPath(), "DeskPinsHook.dll");
        using var fs = new FileStream(_dllPath, FileMode.Create, FileAccess.Write);
        stream.CopyTo(fs);
    }

    public bool Inject(IntPtr hwnd, uint exePid)
    {
        if (string.IsNullOrEmpty(_dllPath) || !File.Exists(_dllPath))
            return false;

        uint targetPid;
        GetWindowThreadProcessId(hwnd, out targetPid);
        if (targetPid == 0) return false;

        // ── Step 1: Write EXE PID + HWND to shared memory BEFORE injecting
        //    DLL will read this in DllMain and spawn worker thread ──

        IntPtr hMapping = CreateFileMappingW(
            new IntPtr(-1), IntPtr.Zero, 4 /* PAGE_READWRITE */,
            0, 16, "DeskPins_SharedMem");

        if (hMapping != IntPtr.Zero)
        {
            IntPtr pView = MapViewOfFile(hMapping, 2 /* FILE_MAP_WRITE */, 0, 0, 16);
            if (pView != IntPtr.Zero)
            {
                Marshal.WriteInt32(pView, 0, (int)exePid);
                Marshal.WriteInt32(pView, 8, (int)hwnd); // HWND on x64 fits in 8 bytes
                UnmapViewOfFile(pView);
            }
        }

        // ── Step 2: Open target process ──

        IntPtr hProcess = OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
            PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
            false, targetPid);

        if (hProcess == IntPtr.Zero)
        {
            if (hMapping != IntPtr.Zero) CloseHandle(hMapping);
            return false; // Protected process, skip
        }

        try
        {
            // ── Step 3: Allocate memory and write DLL path in target process ──

            byte[] dllPathBytes = System.Text.Encoding.Unicode.GetBytes(_dllPath + '\0');
            IntPtr pRemoteMem = VirtualAllocEx(hProcess, IntPtr.Zero,
                (uint)dllPathBytes.Length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

            if (pRemoteMem == IntPtr.Zero) return false;

            WriteProcessMemory(hProcess, pRemoteMem, dllPathBytes,
                (uint)dllPathBytes.Length, out _);

            // ── Step 4: Create remote thread calling LoadLibraryW ──

            IntPtr pLoadLibrary = GetProcAddress(
                GetModuleHandleW("kernel32.dll"),
                Marshal.StringToHGlobalAnsi("LoadLibraryW"));

            IntPtr hRemoteThread = CreateRemoteThread(hProcess, IntPtr.Zero, 0,
                pLoadLibrary, pRemoteMem, 0, out _);

            if (hRemoteThread == IntPtr.Zero)
            {
                VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
                return false;
            }

            // Wait for DLL to load — DllMain will read shared memory and spawn worker
            WaitForSingleObject(hRemoteThread, 5000);
            CloseHandle(hRemoteThread);
            VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);

            return true;
        }
        catch
        {
            return false;
        }
        finally
        {
            CloseHandle(hProcess);
            // Keep shared memory open briefly for DLL to read during DllMain
            // It'll be cleaned up by OS when all handles close
            if (hMapping != IntPtr.Zero) CloseHandle(hMapping);
        }
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr CreateFileMappingW(
        IntPtr hFile, IntPtr lpAttributes, uint flProtect,
        uint dwMaximumSizeHigh, uint dwMaximumSizeLow,
        [MarshalAs(UnmanagedType.LPWStr)] string lpName);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr MapViewOfFile(
        IntPtr hFileMappingObject, uint dwDesiredAccess,
        uint dwFileOffsetHigh, uint dwFileOffsetLow, uint dwNumberOfBytesToMap);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool UnmapViewOfFile(IntPtr lpBaseAddress);
}
```

- [ ] **Step 2: Commit**

```bash
git add DeskPins/DllInjector.cs
git commit -m "feat: implement DLL extraction and CreateRemoteThread injection"
```

---

### Task 7: PinnedWindowTracker.cs

**Files:**
- Create: `DeskPins/PinnedWindowTracker.cs`

- [ ] **Step 1: Write PinnedWindowTracker.cs**

```csharp
using static DeskPins.NativeMethods;

namespace DeskPins;

internal class PinnedWindowTracker : IDisposable
{
    private readonly Dictionary<IntPtr, bool> _pinned = new();
    private IntPtr _winEventHook;
    private WinEventProc? _winEventProc;
    private readonly System.Windows.Forms.Timer _refreshTimer;

    public int Count => _pinned.Count;
    public event Action? Changed;

    public PinnedWindowTracker()
    {
        // Hook window destruction events
        _winEventProc = WinEventCallback;
        _winEventHook = SetWinEventHook(
            EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY,
            IntPtr.Zero, _winEventProc,
            0, 0, WINEVENT_OUTOFCONTEXT);

        // Fallback: periodic cleanup of dead windows
        _refreshTimer = new System.Windows.Forms.Timer
        {
            Interval = 5000
        };
        _refreshTimer.Tick += CleanupDeadWindows;
        _refreshTimer.Start();
    }

    public bool IsPinned(IntPtr hwnd) => _pinned.ContainsKey(hwnd);

    public void Add(IntPtr hwnd)
    {
        _pinned[hwnd] = true;
        Changed?.Invoke();
    }

    public void Remove(IntPtr hwnd)
    {
        _pinned.Remove(hwnd);
        Changed?.Invoke();
    }

    public void Clear()
    {
        foreach (var hwnd in _pinned.Keys.ToList())
        {
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        _pinned.Clear();
        Changed?.Invoke();
    }

    public IntPtr[] GetPinnedWindows() => _pinned.Keys.ToArray();

    private void WinEventCallback(
        IntPtr hWinEventHook, uint eventType, IntPtr hwnd,
        int idObject, int idChild, uint dwEventThread, uint dwmsEventTime)
    {
        if (_pinned.Remove(hwnd))
        {
            Changed?.Invoke();
        }
    }

    private void CleanupDeadWindows(object? sender, EventArgs e)
    {
        var dead = new List<IntPtr>();
        foreach (var hwnd in _pinned.Keys)
        {
            if (!IsWindow(hwnd))
                dead.Add(hwnd);
        }
        if (dead.Count > 0)
        {
            foreach (var hwnd in dead) _pinned.Remove(hwnd);
            Changed?.Invoke();
        }
    }

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool IsWindow(IntPtr hWnd);

    public void Dispose()
    {
        _refreshTimer.Stop();
        _refreshTimer.Dispose();
        if (_winEventHook != IntPtr.Zero)
        {
            UnhookWinEvent(_winEventHook);
            _winEventHook = IntPtr.Zero;
        }
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add DeskPins/PinnedWindowTracker.cs
git commit -m "feat: implement pinned window tracker with cleanup"
```

---

### Task 8: Program.cs — Wire Everything

**Files:**
- Create: `DeskPins/Program.cs`

- [ ] **Step 1: Write Program.cs**

```csharp
using System.Runtime.InteropServices;
using static DeskPins.NativeMethods;

namespace DeskPins;

internal static class Program
{
    private static TrayManager? _tray;
    private static PinController? _pinController;
    private static DllInjector? _injector;
    private static PinnedWindowTracker? _tracker;
    private static string _pinCursorPath = "";

    [STAThread]
    static void Main()
    {
        // Single instance check
        IntPtr hMutex = CreateMutexW(IntPtr.Zero, false, "DeskPins_SingleInstance");
        if (hMutex != IntPtr.Zero && GetLastError() == ERROR_ALREADY_EXISTS)
        {
            CloseHandle(hMutex);
            return;
        }

        // DPI awareness (Win10 1703+)
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        // Extract embedded resources
        ExtractResources();

        // Initialize components
        _tray = new TrayManager();
        _pinController = new PinController();
        _injector = new DllInjector();
        _tracker = new PinnedWindowTracker();

        // Wire events
        _tray.PinModeRequested += OnPinModeRequested;
        _tray.UnpinAllRequested += OnUnpinAllRequested;
        _tray.ExitRequested += OnExitRequested;
        _pinController.WindowClicked += OnWindowClicked;
        _tracker.Changed += () =>
        {
            _tray?.UpdatePinnedCount(_tracker.Count);
        };

        // Create hidden form for message pump
        var hiddenForm = new Form
        {
            WindowState = FormWindowState.Minimized,
            ShowInTaskbar = false,
            Visible = false
        };

        // Register ESC as cancel key during pin mode
        hiddenForm.KeyPreview = true;
        hiddenForm.KeyDown += (s, e) =>
        {
            if (e.KeyCode == Keys.Escape && _pinController.IsActive)
            {
                _pinController.ExitPinMode();
                _tray.SetPinModeActive(false);
            }
        };

        Application.Run(hiddenForm);
    }

    private static void ExtractResources()
    {
        var asm = typeof(Program).Assembly;

        // Extract pin cursor
        try
        {
            using var cursorStream = asm.GetManifestResourceStream("DeskPins.Resources.pin.cur");
            if (cursorStream != null)
            {
                _pinCursorPath = Path.Combine(Path.GetTempPath(), "DeskPins_pin.cur");
                using var fs = new FileStream(_pinCursorPath, FileMode.Create, FileAccess.Write);
                cursorStream.CopyTo(fs);
            }
        }
        catch { /* non-critical */ }
    }

    private static void OnPinModeRequested()
    {
        if (_pinController!.IsActive)
        {
            _pinController.ExitPinMode();
            _tray!.SetPinModeActive(false);
            return;
        }

        try
        {
            _pinController.EnterPinMode(_pinCursorPath);
            _tray!.SetPinModeActive(true);
        }
        catch (Exception ex)
        {
            MessageBox.Show($"无法进入钉选模式: {ex.Message}",
                "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private static void OnWindowClicked(IntPtr hwnd)
    {
        uint pid = (uint)Environment.ProcessId;
        uint exePid = pid; // capture for DLL heartbeat

        if (_tracker!.IsPinned(hwnd))
        {
            // Unpin
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            _tracker.Remove(hwnd);
            // DLL heartbeat will detect TOPMOST cleared and self-unload
        }
        else
        {
            // Pin
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            _tracker.Add(hwnd);

            // Try to inject DLL for title bar icon (best-effort)
            _injector!.Inject(hwnd, exePid);
        }

        _pinController!.ExitPinMode();
        _tray!.SetPinModeActive(false);
    }

    private static void OnUnpinAllRequested()
    {
        _tracker!.Clear();
    }

    private static void OnExitRequested()
    {
        // Exit pin mode if active
        _pinController?.ExitPinMode();

        // Unpin all windows
        _tracker?.Clear();

        // Cleanup
        _pinController?.Dispose();
        _tracker?.Dispose();
        _tray?.Dispose();

        // Clean up temp files
        try
        {
            string dllPath = Path.Combine(Path.GetTempPath(), "DeskPinsHook.dll");
            if (File.Exists(dllPath)) File.Delete(dllPath);
            if (File.Exists(_pinCursorPath)) File.Delete(_pinCursorPath);
        }
        catch { }

        Application.Exit();
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add DeskPins/Program.cs
git commit -m "feat: wire all components in Program.cs with full interaction flow"
```

---

### Task 9: Tray Icon & Pin Cursor (Programmatic)

Tray icon and pin cursor are created programmatically — no .ico/.cur files needed. The TrayManager already falls back to `SystemIcons.Application`; the PinController falls back to `Cursors.Cross`.

- [ ] **Step 1: Update TrayManager to always use programmatic icon**

Replace the `LoadTrayIcon()` method in `DeskPins/TrayManager.cs`:

```csharp
private static Icon LoadTrayIcon()
{
    // Create a simple 16x16 icon programmatically
    var bmp = new Bitmap(16, 16);
    using var g = Graphics.FromImage(bmp);
    g.Clear(Color.Transparent);
    using var brush = new SolidBrush(Color.FromArgb(220, 40, 40));
    g.FillEllipse(brush, 4, 1, 8, 8);   // pin head
    g.FillRectangle(brush, 7, 8, 2, 8);  // pin shaft
    return Icon.FromHandle(bmp.GetHicon());
}
```

- [ ] **Step 2: Update PinController to create pin cursor programmatically**

Replace the `EnterPinMode` cursor loading in `DeskPins/PinController.cs`:

```csharp
public void EnterPinMode()
{
    if (_isActive) return;
    _isActive = true;

    // Create pin cursor programmatically
    var bmp = new Bitmap(32, 32);
    using var g = Graphics.FromImage(bmp);
    g.Clear(Color.Transparent);
    using var brush = new SolidBrush(Color.FromArgb(220, 40, 40));
    g.FillEllipse(brush, 10, 2, 12, 12);  // pin head
    g.FillRectangle(brush, 14, 12, 4, 18); // pin shaft
    g.FillRectangle(brush, 13, 13, 6, 2);  // cross bar

    _pinCursor = bmp.GetHicon();
    // Note: Icon handle can be used as cursor via SetCursor
}
```

- [ ] **Step 3: Clean up Program.cs — remove ExtractResources, remove pin cursor path**

Remove `ExtractResources()` and `_pinCursorPath` from `DeskPins/Program.cs`. Update `OnPinModeRequested` to call `_pinController.EnterPinMode()` with no arguments.

- [ ] **Step 4: Commit**

```bash
git add DeskPins/TrayManager.cs DeskPins/PinController.cs DeskPins/Program.cs
git commit -m "feat: use programmatic icons instead of embedded .ico/.cur files"
```

---

### Task 10: Full Build & Integration Test

- [ ] **Step 1: Build native DLL**

```bash
cd HookDLL && cmake -B build -G "MinGW Makefiles" && cmake --build build --config Release
```

Expected: `build/DeskPinsHook.dll` created, no errors.

- [ ] **Step 2: Copy DLL and build C# project**

```bash
cp HookDLL/build/DeskPinsHook.dll DeskPins/Resources/
dotnet build DeskPins/DeskPins.csproj -c Release
```

Expected: Build succeeds with no errors.

- [ ] **Step 3: Run and smoke test**

```bash
dotnet run --project DeskPins/DeskPins.csproj -c Release
```

Manual verification checklist:
1. Tray icon appears in system tray area
2. Right-click shows menu: 开始钉选 / 清除所有置顶 / 关于 / 退出
3. Click "开始钉选" → cursor changes (to crosshair at minimum, to pin.cur if resource works)
4. Click on Notepad → Notepad stays on top of other windows
5. Right-click in pin mode → exits pin mode
6. Click "清除所有置顶" → Notepad back to normal
7. "退出" → tray icon disappears, app exits
8. Second instance → exits immediately

- [ ] **Step 4: Publish single-file EXE**

```bash
rm -rf publish
dotnet publish DeskPins/DeskPins.csproj -c Release \
  -p:PublishSingleFile=true \
  -p:SelfContained=false \
  -p:PublishTrimmed=true \
  -o publish
ls -lh publish/DeskPins.exe
```

Expected: Single `DeskPins.exe` file, ~5-10 MB.

- [ ] **Step 5: Verify portable EXE**

```bash
# Run the published EXE directly
./publish/DeskPins.exe
```

Verify tray icon appears (the EXE runs standalone, no .NET SDK needed since SelfContained=false uses system .NET, but on a machine with .NET 8 runtime it works).

For true portability, optionally add `--self-contained` (produces ~60MB but zero dependencies):

```bash
dotnet publish DeskPins/DeskPins.csproj -c Release \
  -p:PublishSingleFile=true \
  -p:SelfContained=true \
  -p:PublishTrimmed=true \
  -o publish-selfcontained
```

- [ ] **Step 6: Commit final state**

```bash
git add -A
git commit -m "chore: finalize build and verify end-to-end"
```
