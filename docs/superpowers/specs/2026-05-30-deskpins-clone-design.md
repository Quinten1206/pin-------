# DeskPins Clone — Design Spec

## Context

Build a window-pinning utility for Windows 11 x64 that mimics DeskPins. A system-tray app: enter "pin mode" to change the cursor into a thumbtack, click any window to toggle always-on-top, and show a pin icon in the title bar of pinned windows (via DLL injection + subclassing). Portable single-file EXE.

## Technology

- **Language**: C# 12 (.NET 8) + WinForms for the main EXE
- **Native DLL**: C++20 (MinGW-w64, `-static`) — minimal ~150-line DLL for title bar icon
- **Build**: `dotnet publish` with `PublishSingleFile=true` for the EXE; CMake for the DLL
- **Platform**: Windows 11 x64 only
- **Dependencies**: None beyond Win32 API (all via P/Invoke)

## Architecture

```
DeskPins.exe (C# / .NET 8 / WinForms / Single-file)
│
├── TrayManager           —— Shell_NotifyIcon, right-click context menu
├── PinController         —— WH_MOUSE_LL global hook, pin cursor, ESC hotkey
├── DllInjector           —— Extract embedded DLL to %TEMP%, CreateRemoteThread injection
├── PinnedWindowTracker   —— Dictionary<HWND, pinned-info>, cleanup on window death
├── NativeMethods         —— All P/Invoke declarations in one static class
│
└── [Embedded Resource] DeskPinsHook.dll (C++, MinGW-w64)
     Injected via CreateRemoteThread → LoadLibrary
     ├── SetWindowSubclass on target window
     ├── WM_NCPAINT       → draw pin icon on title bar (DPI-aware)
     ├── WM_NCLBUTTONDOWN → detect icon click → unpin locally
     ├── WM_NCDESTROY     → cleanup and self-unload
     └── Heartbeat timer  → detect EXE death → self-unload
```

## Project Structure

```
pin/
├── DeskPins/                  # C# project
│   ├── DeskPins.csproj
│   ├── Program.cs             # Entry, single-instance mutex, hidden form
│   ├── TrayManager.cs         # NotifyIcon + context menu
│   ├── PinController.cs       # Mouse hook, cursor swap, pin mode state machine
│   ├── DllInjector.cs         # Embedded resource extraction + remote thread injection
│   ├── PinnedWindowTracker.cs # HWND tracking, SetWinEventHook for destruction
│   ├── NativeMethods.cs       # P/Invoke declarations (user32, kernel32)
│   └── Resources/
│       ├── DeskPinsHook.dll   # Pre-compiled native DLL (embedded as byte[])
│       ├── app.ico            # Tray icon
│       └── pin.cur            # Pin mode cursor
├── HookDLL/                   # Native DLL source
│   ├── CMakeLists.txt
│   └── dllmain.cpp            # SubclassProc, WM_NCPAINT drawing, heartbeat
└── README.md
```

## Interaction Flow

### 1. Startup
- `CreateMutex("DeskPins_SingleInstance")` → if exists, exit
- Create hidden WinForms window (message pump for hooks + timer)
- `Shell_NotifyIcon(NIM_ADD)` → tray icon
- Right-click menu: *开始钉选* / *清除所有置顶* / *关于* / *退出*

### 2. Enter Pin Mode (menu or double-click tray)
- Change cursor to pin.cur
- Install `SetWindowsHookEx(WH_MOUSE_LL)`
- Register ESC as cancel hotkey
- Tray icon shows "active" indicator

### 3. In Pin Mode
- Mouse moves → `WindowFromPoint()` → optional XOR highlight rectangle
- Left click on window:
  - Not in tracker → `SetWindowPos(HWND_TOPMOST)` → `InjectDLL(hwnd)` → add to tracker → exit pin mode
  - Already in tracker → `SetWindowPos(HWND_NOTOPMOST)` → remove from tracker → exit pin mode
  - (DLL heartbeat detects WS_EX_TOPMOST flag cleared → self-unload within ~2s)
- ESC → cancel, no action, exit pin mode

### 4. Unpin via Title Bar Icon (DLL side, fully autonomous)
- DLL's SubclassProc detects click on icon region
- → `SetWindowPos(HWND_NOTOPMOST)` on the local window
- → `RemoveWindowSubclass` → `FreeLibraryAndExitThread`
- EXE learns about it via `SetWinEventHook(EVENT_OBJECT_DESTROY)` on subclass removal message, or next tracker refresh

### 5. Unpin All
- Iterate tracked HWNDs → `SetWindowPos(HWND_NOTOPMOST)` → clear tracker
- Each DLL's heartbeat detects TOPMOST flag cleared → self-unload within ~2s

### 6. Exit
- `UnhookWindowsHookEx` if active
- Clean up all pinned windows (NOTOPMOST)
- `Shell_NotifyIcon(NIM_DELETE)`
- `Application.Exit()`

## Key P/Invoke APIs

| API | DLL | Purpose |
|-----|-----|---------|
| `SetWindowPos` | user32 | Set/clear HWND_TOPMOST |
| `SetWindowsHookEx` | user32 | WH_MOUSE_LL global hook |
| `UnhookWindowsHookEx` | user32 | Remove hook |
| `WindowFromPoint` | user32 | Identify window under cursor |
| `GetWindowThreadProcessId` | user32 | Get PID for injection |
| `Shell_NotifyIcon` | shell32 | Tray icon add/modify/delete |
| `SetWinEventHook` | user32 | Track window destruction |
| `CreateRemoteThread` | kernel32 | DLL injection |
| `VirtualAllocEx` | kernel32 | Allocate memory in target |
| `WriteProcessMemory` | kernel32 | Write DLL path string |
| `OpenProcess` | kernel32 | Open target process handle |
| `GetDpiForWindow` | user32 | DPI-aware icon scaling |

## Edge Cases

| Case | Handling |
|------|----------|
| Protected/system process | `OpenProcess` fails → skip injection, TOPMOST still works |
| UWP / ApplicationFrameHost | Skip injection (DWM-managed chrome), TOPMOST only |
| Pinned window closed by user | DLL's WM_NCDESTROY → self-cleanup; EXE's SetWinEventHook catches it |
| Main EXE killed or window unpinned from EXE | DLL heartbeat (~2s) checks: Is EXE still alive? Is window still TOPMOST? If either false → `FreeLibraryAndExitThread` |
| High DPI / multi-monitor | `SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE)`, scale icon by `GetDpiForWindow` |
| Already running | `CreateMutex` check → exit second instance |
| Missing DLL at runtime (extraction failed) | Log, skip injection, TOPMOST still works |
| `CreateRemoteThread` fails (AV/NX) | Silent skip, TOPMOST still works |

## Build & Publish

```bash
# 1. Build native DLL
cd HookDLL
cmake -B build -G "MinGW Makefiles"
cmake --build build --config Release

# 2. Copy DLL into C# resources
cp build/DeskPinsHook.dll ../DeskPins/Resources/

# 3. Publish single-file EXE
dotnet publish DeskPins/DeskPins.csproj -c Release \
  -p:PublishSingleFile=true \
  -p:SelfContained=false \
  -p:PublishTrimmed=true \
  -o publish/
```

Output: `DeskPins.exe` — true portable single-file EXE.

## Verification

1. Run `DeskPins.exe` → tray icon appears, no window
2. Double-click tray → cursor becomes pin icon
3. Click on Notepad → Notepad stays on top, pin icon visible in title bar
4. Click pin icon in title bar → Notepad returns to normal z-order
5. Pin multiple windows → right-click "清除所有置顶" → all restored
6. Close a pinned window → no crash, tracker updated
7. Kill DeskPins.exe from Task Manager → previously injected DLLs self-unload within ~2s
8. Start a second DeskPins.exe → exits immediately (single instance)
