# Drive a co-op client window as a player would (S06-T07 checks): raise the window of the
# client that started N-th (1 = first), click inside it so it takes the mouse, then play the
# steps: a DirectInput key by scan code ("key:0x03" is the 2 key, "key:0x13" is R), "hold:<scan>:<ms>", "click"
# (the fire button) or "wait:<ms>". It drives the real mouse and keyboard.
param(
    [Parameter(Mandatory = $true)][int]$Client,
    [Parameter(Mandatory = $true)][string]$Steps
)
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class D {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort vk; public ushort scan; public uint flags; public uint time; public IntPtr extra; }
    [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT { public int dx; public int dy; public uint data; public uint flags; public uint time; public IntPtr extra; }
    [StructLayout(LayoutKind.Explicit)] public struct INPUT { [FieldOffset(0)] public uint type; [FieldOffset(8)] public KEYBDINPUT ki; [FieldOffset(8)] public MOUSEINPUT mi; }
    [DllImport("user32.dll")] public static extern uint SendInput(uint n, INPUT[] inputs, int size);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint f);
    public static void Key(ushort scan, bool up) {
        INPUT[] i = new INPUT[1]; i[0].type = 1; i[0].ki.scan = scan; i[0].ki.flags = 0x8u | (up ? 0x2u : 0u);
        SendInput(1, i, Marshal.SizeOf(typeof(INPUT)));
    }
    public static void Button(bool up) {
        INPUT[] i = new INPUT[1]; i[0].type = 0; i[0].mi.flags = up ? 0x4u : 0x2u;
        SendInput(1, i, Marshal.SizeOf(typeof(INPUT)));
    }
}
"@
$clients = @(Get-Process AnomalyDX11 | Where-Object { $_.MainWindowHandle -ne [IntPtr]::Zero } | Sort-Object StartTime)
if ($clients.Count -lt $Client) { throw "No client $Client (found $($clients.Count))." }
$h = $clients[$Client - 1].MainWindowHandle
foreach ($after in @([IntPtr](-1), [IntPtr](-2))) { [D]::SetWindowPos($h, $after, 0, 0, 0, 0, 0x13) | Out-Null }
$c = New-Object D+RECT; [D]::GetClientRect($h, [ref]$c) | Out-Null
$p = New-Object D+POINT; $p.X = [int](($c.R - $c.L) / 2); $p.Y = [int](($c.B - $c.T) / 2); [D]::ClientToScreen($h, [ref]$p) | Out-Null
[D]::SetCursorPos($p.X, $p.Y) | Out-Null; Start-Sleep -Milliseconds 150
[D]::Button($false); Start-Sleep -Milliseconds 80; [D]::Button($true); Start-Sleep -Milliseconds 500
$log = @()
foreach ($step in $Steps.Split(',')) {
    if ($step -like 'key:*') {
        $scan = [Convert]::ToUInt16($step.Substring(4), 16)
        [D]::Key($scan, $false); Start-Sleep -Milliseconds 90; [D]::Key($scan, $true); Start-Sleep -Milliseconds 400
    } elseif ($step -like 'hold:*') {
        # hold:<scan>:<ms>, e.g. hold:0x10:1500 holds Q (lean left) for 1.5 s
        $parts = $step.Split(':')
        $scan = [Convert]::ToUInt16($parts[1], 16)
        [D]::Key($scan, $false); Start-Sleep -Milliseconds ([int]$parts[2]); [D]::Key($scan, $true); Start-Sleep -Milliseconds 300
    } elseif ($step -eq 'click') {
        [D]::Button($false); Start-Sleep -Milliseconds 90; [D]::Button($true); Start-Sleep -Milliseconds 400
    } elseif ($step -like 'wait:*') {
        Start-Sleep -Milliseconds ([int]$step.Substring(5))
    }
    $log += "$([DateTime]::UtcNow.ToString('o')) $step"
}
"client=$Client pid=$($clients[$Client - 1].Id)"
$log
