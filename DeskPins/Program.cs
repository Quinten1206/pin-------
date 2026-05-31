using System.Runtime.InteropServices;
using static DeskPins.NativeMethods;

namespace DeskPins;

internal static class Program
{
    private static TrayManager? _tray;
    private static PinController? _pinController;
    private static DllInjector? _injector;
    private static PinnedWindowTracker? _tracker;

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
            Visible = false,
            KeyPreview = true
        };

        // Register ESC as cancel key during pin mode
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
            _pinController.EnterPinMode();
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
        uint exePid = (uint)Environment.ProcessId;

        if (_tracker!.IsPinned(hwnd))
        {
            // Unpin
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            _tracker.Remove(hwnd);
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
        _pinController?.ExitPinMode();
        _tracker?.Clear();
        _pinController?.Dispose();
        _tracker?.Dispose();
        _tray?.Dispose();

        // Clean up temp files
        try
        {
            string dllPath = Path.Combine(Path.GetTempPath(), "DeskPinsHook.dll");
            if (File.Exists(dllPath)) File.Delete(dllPath);
        }
        catch { }

        Application.Exit();
    }
}
