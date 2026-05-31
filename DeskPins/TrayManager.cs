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
        // Create a simple 16x16 icon programmatically
        var bmp = new Bitmap(16, 16);
        using var g = Graphics.FromImage(bmp);
        g.Clear(Color.Transparent);
        using var brush = new SolidBrush(Color.FromArgb(220, 40, 40));
        g.FillEllipse(brush, 4, 1, 8, 8);   // pin head
        g.FillRectangle(brush, 7, 8, 2, 8);  // pin shaft
        return Icon.FromHandle(bmp.GetHicon());
    }

    public void Dispose()
    {
        _notifyIcon.Visible = false;
        _notifyIcon.Dispose();
        _menu.Dispose();
    }
}
