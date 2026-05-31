using System.Runtime.InteropServices;
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
