using System.Runtime.InteropServices;
using static DeskPins.NativeMethods;

namespace DeskPins;

internal class PinController : IDisposable
{
    private IntPtr _mouseHook;
    private IntPtr _pinCursor;
    private HookProc? _mouseProc;
    private bool _isActive;

    public event Action<IntPtr>? WindowClicked;

    public bool IsActive => _isActive;

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

        if (_pinCursor != IntPtr.Zero)
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

                // Walk up to find root window
                hWnd = GetAncestor(hWnd);

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

    private static IntPtr GetAncestor(IntPtr hwnd)
    {
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
        if (hwnd == GetDesktopWindow()) return false;
        if (hwnd == GetShellWindow()) return false;

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
