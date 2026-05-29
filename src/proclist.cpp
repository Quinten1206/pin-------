#include "proclist.h"
#include "version.h"
#include <algorithm>

static std::vector<PinEntry> s_list;

void ProcListAdd(HWND hwnd) {
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

void ProcListClearAll() {
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

static void CALLBACK PollTimerProc(HWND, UINT, UINT_PTR, DWORD) {
    s_list.erase(
        std::remove_if(s_list.begin(), s_list.end(),
            [](const PinEntry& e) { return !IsWindow(e.hwnd); }),
        s_list.end());
}

void ProcListStartPolling(HWND hWnd) {
    SetTimer(hWnd, TIMER_ID_POLL, POLL_INTERVAL_MS, PollTimerProc);
}
