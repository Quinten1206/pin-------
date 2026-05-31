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

        // Step 1: Write EXE PID + HWND to shared memory BEFORE injecting
        IntPtr hMapping = CreateFileMappingW(
            new IntPtr(-1), IntPtr.Zero, 4 /* PAGE_READWRITE */,
            0, 16, "DeskPins_SharedMem");

        if (hMapping != IntPtr.Zero)
        {
            IntPtr pView = MapViewOfFile(hMapping, 2 /* FILE_MAP_WRITE */, 0, 0, 16);
            if (pView != IntPtr.Zero)
            {
                Marshal.WriteInt32(pView, 0, (int)exePid);
                Marshal.WriteInt32(pView, 8, (int)hwnd);
                UnmapViewOfFile(pView);
            }
        }

        // Step 2: Open target process
        IntPtr hProcess = OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
            PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
            false, targetPid);

        if (hProcess == IntPtr.Zero)
        {
            if (hMapping != IntPtr.Zero) CloseHandle(hMapping);
            return false;
        }

        try
        {
            // Step 3: Allocate memory and write DLL path in target process
            byte[] dllPathBytes = System.Text.Encoding.Unicode.GetBytes(_dllPath + '\0');
            IntPtr pRemoteMem = VirtualAllocEx(hProcess, IntPtr.Zero,
                (uint)dllPathBytes.Length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

            if (pRemoteMem == IntPtr.Zero) return false;

            WriteProcessMemory(hProcess, pRemoteMem, dllPathBytes,
                (uint)dllPathBytes.Length, out _);

            // Step 4: Create remote thread calling LoadLibraryW
            IntPtr pLoadLibraryName = Marshal.StringToHGlobalAnsi("LoadLibraryW");
            IntPtr pLoadLibrary = GetProcAddress(
                GetModuleHandleW("kernel32.dll"), pLoadLibraryName);
            Marshal.FreeHGlobal(pLoadLibraryName);

            IntPtr hRemoteThread = CreateRemoteThread(hProcess, IntPtr.Zero, 0,
                pLoadLibrary, pRemoteMem, 0, out _);

            if (hRemoteThread == IntPtr.Zero)
            {
                VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
                return false;
            }

            // Wait for DLL to load
            WaitForSingleObject(hRemoteThread, 5000);

            // Get the remote DLL base address (LoadLibraryW return value)
            GetExitCodeThread(hRemoteThread, out uint remoteModuleBase);
            CloseHandle(hRemoteThread);
            VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);

            if (remoteModuleBase != 0)
            {
                // Load DLL locally to find InstallHook offset
                IntPtr hLocalDll = LoadLibraryW(_dllPath);
                if (hLocalDll != IntPtr.Zero)
                {
                    IntPtr pName = Marshal.StringToHGlobalAnsi("InstallHook");
                    IntPtr pInstallHookLocal = GetProcAddress(hLocalDll, pName);
                    Marshal.FreeHGlobal(pName);

                    long offset = pInstallHookLocal.ToInt64() - hLocalDll.ToInt64();
                    FreeLibrary(hLocalDll);

                    IntPtr pInstallHookRemote = (IntPtr)((long)remoteModuleBase + offset);

                    // Call InstallHook(hwnd) in target process
                    IntPtr hHookThread = CreateRemoteThread(hProcess, IntPtr.Zero, 0,
                        pInstallHookRemote, hwnd, 0, out _);
                    if (hHookThread != IntPtr.Zero)
                    {
                        WaitForSingleObject(hHookThread, 5000);
                        CloseHandle(hHookThread);
                    }
                }
            }

            return true;
        }
        catch
        {
            return false;
        }
        finally
        {
            CloseHandle(hProcess);
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
