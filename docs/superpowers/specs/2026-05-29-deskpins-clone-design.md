# DeskPins Clone — Design Spec

## Context

Build a window-pinning utility for Windows 11 that mimics DeskPins. The user wants a system-tray app: enter "pin mode" to change the cursor into a thumbtack, click a window to keep it always-on-top, and show a pin icon in the title bar of pinned windows (via DLL injection + subclassing). No installer — single portable EXE.

## Technology

- **Language**: C++20 (MinGW-w64 via MSYS2, already set up)
- **Build**: CMake, two targets — `DeskPins.exe` + `DeskPinsHook.dll`
- **Platform**: Windows 11 x64 only (no 32-bit support in v1)
- **Dependencies**: Win32 API + comctl32 (`SetWindowSubclass`)

## Architecture

```
DeskPins.exe (tray + pin mode controller)
    │
    ├─ SetWindowsHookEx(WH_MOUSE_LL) — global mouse hook during pin mode
    ├─ SetWindowPos(HWND_TOPMOST) — pin/unpin windows
    ├─ CreateRemoteThread(LoadLibraryW) — inject DLL into target process
    └─ Embed DeskPinsHook.dll as RCDATA resource, extract to %TEMP% at runtime
```

### DeskPinsHook.dll (injected)

- `SetWindowSubclass` the target window
- Handle `WM_NCPAINT` → draw pin icon on title bar
- Handle `WM_NCLBUTTONDOWN` → detect icon click → unpin (`HWND_NOTOPMOST`) → `RemoveWindowSubclass` → `FreeLibraryAndExitThread`
- Handle `WM_NCDESTROY` → cleanup
- Periodic heartbeat: check if main EXE died → self-unload

## Components

### Main EXE (src/)

| Module | Responsibility |
|--------|---------------|
| `main.cpp` | WinMain, message pump, single-instance mutex |
| `traymanager.cpp/h` | `Shell_NotifyIcon`, right-click menu, double-click handler |
| `pincontroller.cpp/h` | `WH_MOUSE_LL` hook, cursor swap, window highlight, ESC hotkey |
| `dllinjector.cpp/h` | Extract DLL from resource, `CreateRemoteThread` injection |
| `proclist.cpp/h` | Track pinned HWNDs, dead-window cleanup timer |
| `resource.rc/h` | Embed app icon, pin cursor, and DLL binary |

### DLL (dll/)

| Module | Responsibility |
|--------|---------------|
| `dllmain.cpp` | DllMain → SetWindowSubclass on target |
| `titlebarpainter.cpp/h` | WM_NCPAINT handler, DPI-aware icon drawing |
| `resource.rc/h` | Pin icon bitmap |

## Interaction Flow

1. **Startup**: Register tray icon (`NIM_ADD`), create hidden message-only window
2. **Enter pin mode** (menu "开始钉选" or double-click tray icon):
   - Set pin cursor, `SetCapture`, install `WH_MOUSE_LL` hook, register ESC hotkey
3. **While in pin mode**:
   - Mouse move → `WindowFromPoint` → optional XOR highlight border
   - Left click on window → `SetWindowPos(HWND_TOPMOST)` → `InjectDLL(hwnd)` → exit pin mode
   - ESC → exit pin mode without action
4. **Unpin a specific window**: click the pin icon drawn on its title bar → DLL handles locally
5. **Unpin all**: tray menu "清除所有置顶" → iterate tracked HWNDs → `SetWindowPos(HWND_NOTOPMOST)`
6. **Exit**: tray menu "退出" → `NIM_DELETE` → cleanup all → `PostQuitMessage`

## Edge Cases

| Case | Handling |
|------|----------|
| DLL injection denied (protected process) | Silent skip; `SetWindowPos` still works, just no title bar icon |
| Pinned window closed by user | WM_NCDESTROY in DLL + 1s polling in EXE to remove from list |
| Main EXE crashes | DLL heartbeat detects dead process → self-unload |
| UWP / ApplicationFrameHost windows | Skip injection; still allow pinning via TOPMOST |
| High DPI / multi-monitor | PMv2 DPI awareness, scale icon by `GetDpiForWindow` |
| Already running instance | CreateMutex check, exit second instance |

## Project Structure

```
pin/
├── CMakeLists.txt
├── src/            → DeskPins.exe
├── dll/            → DeskPinsHook.dll (embedded as RCDATA in exe)
├── assets/         → app.ico, pin.cur
└── .vscode/        → tasks.json, launch.json
```

## Verification

1. Build both targets: `cmake --build build --config Release`
2. Run `DeskPins.exe` — tray icon appears, no main window
3. Double-click tray icon → cursor becomes pin
4. Click on Notepad → Notepad stays on top, pin icon visible in its title bar
5. Click pin icon in Notepad title bar → Notepad back to normal
6. Pin multiple windows → "清除所有置顶" → all restored
7. Kill DeskPins.exe from Task Manager → previously injected DLLs self-unload
