# Windowed co-op client and the mouse (user request 2026-10-22): with the clients of a running
# session up, take the focus away from a client window (as alt-tab does), drag that window by its
# title bar, then click inside it. Checks: after the focus left, the cursor is free (no clip);
# the title-bar click does not clip it and the drag moves the window; the click inside clips the
# cursor to the window's client area again (the game has the mouse). Writes a JSON result.
param(
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$DragX = 160,
    [int]$DragY = 90
)
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class W {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool GetClipCursor(out RECT r);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, IntPtr e);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetDesktopWindow();
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
    [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT p);
    [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr h, uint f);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint f);
}
"@
function Rect($r) { "$($r.L),$($r.T),$($r.R),$($r.B)" }
function Clip { $r = New-Object W+RECT; [W]::GetClipCursor([ref]$r) | Out-Null; $r }
function FullScreen($r) {
    $w = [W]::GetSystemMetrics(78); $h = [W]::GetSystemMetrics(79) # SM_CXVIRTUALSCREEN, SM_CYVIRTUALSCREEN
    ($r.R - $r.L) -ge $w -and ($r.B - $r.T) -ge $h
}
function Click($x, $y) {
    [W]::SetCursorPos($x, $y) | Out-Null; Start-Sleep -Milliseconds 150
    [W]::mouse_event(0x2, 0, 0, 0, [IntPtr]::Zero); Start-Sleep -Milliseconds 120
    [W]::mouse_event(0x4, 0, 0, 0, [IntPtr]::Zero); Start-Sleep -Milliseconds 400
}
$clients = @(Get-Process AnomalyDX11 -ErrorAction Stop | Where-Object { $_.MainWindowHandle -ne [IntPtr]::Zero })
if ($clients.Count -lt 1) { throw 'No client window.' }
# raise the first client's window above the others (topmost, then back to normal), then test the
# window on top at its centre (the two clients open at the same place)
foreach ($after in @([IntPtr](-1), [IntPtr](-2))) { # HWND_TOPMOST, HWND_NOTOPMOST
    [W]::SetWindowPos($clients[0].MainWindowHandle, $after, 0, 0, 0, 0, 0x13) | Out-Null # NOSIZE|NOMOVE|NOACTIVATE
}
Start-Sleep -Milliseconds 300
$w0 = New-Object W+RECT; [W]::GetWindowRect($clients[0].MainWindowHandle, [ref]$w0) | Out-Null
$mid = New-Object W+POINT; $mid.X = [int](($w0.L + $w0.R) / 2); $mid.Y = [int](($w0.T + $w0.B) / 2)
$top = [W]::GetAncestor([W]::WindowFromPoint($mid), 2) # GA_ROOT
$owner = $clients | Where-Object { $_.MainWindowHandle -eq $top } | Select-Object -First 1
if (-not $owner) { throw 'No client window on top at the test point.' }
$h = $top
$result = [ordered]@{ window = $owner.Id; windows = @($clients | ForEach-Object { $_.Id }) }

# 1. the game window has the focus and the mouse: click in its client area
$c = New-Object W+RECT; [W]::GetClientRect($h, [ref]$c) | Out-Null
$p = New-Object W+POINT; $p.X = [int](($c.R - $c.L) / 2); $p.Y = [int](($c.B - $c.T) / 2); [W]::ClientToScreen($h, [ref]$p) | Out-Null
Click $p.X $p.Y
$result.focused_is_window = ([W]::GetForegroundWindow() -eq $h)
$result.focused_clip = Rect (Clip)
$result.focused_captured = -not (FullScreen (Clip))

# 2. the focus goes elsewhere (the desktop's shell window), as alt-tab does
$shell = (Get-Process explorer | Select-Object -First 1).MainWindowHandle
if ($shell -eq [IntPtr]::Zero) { $shell = [W]::GetDesktopWindow() }
[W]::SetForegroundWindow($shell) | Out-Null; Start-Sleep -Milliseconds 600
if ([W]::GetForegroundWindow() -eq $h) {
    # a background process may not move the focus: click a point outside every client window
    $sw = [W]::GetSystemMetrics(0); $sh = [W]::GetSystemMetrics(1)
    $spot = $null
    foreach ($cand in @(@(($sw - 30), [int]($sh / 2)), @(30, [int]($sh / 2)), @([int]($sw / 2), ($sh - 80)))) {
        $inside = $false
        foreach ($cl in $clients) { $r = New-Object W+RECT; [W]::GetWindowRect($cl.MainWindowHandle, [ref]$r) | Out-Null
            if ($cand[0] -ge $r.L -and $cand[0] -le $r.R -and $cand[1] -ge $r.T -and $cand[1] -le $r.B) { $inside = $true } }
        if (-not $inside) { $spot = $cand; break }
    }
    if ($spot) { Click $spot[0] $spot[1] }
    $result.focus_moved_by = 'click'
} else { $result.focus_moved_by = 'SetForegroundWindow' }
$result.focus_left = ([W]::GetForegroundWindow() -ne $h)
$result.after_focus_left_free = FullScreen (Clip)
[W]::SetCursorPos(10, 10) | Out-Null; Start-Sleep -Milliseconds 200
$q = New-Object W+POINT; [W]::GetCursorPos([ref]$q) | Out-Null
$result.after_focus_left_cursor = "$($q.X),$($q.Y)"
$result.cursor_moves_freely = ($q.X -eq 10 -and $q.Y -eq 10)

# 3. drag the window by its title bar
$w0 = New-Object W+RECT; [W]::GetWindowRect($h, [ref]$w0) | Out-Null
$tx = [int](($w0.L + $w0.R) / 2); $ty = $w0.T + 10
[W]::SetCursorPos($tx, $ty) | Out-Null; Start-Sleep -Milliseconds 200
[W]::mouse_event(0x2, 0, 0, 0, [IntPtr]::Zero); Start-Sleep -Milliseconds 300
$result.title_click_free = FullScreen (Clip)
$result.title_click_foreground = ([W]::GetForegroundWindow() -eq $h)
for ($i = 1; $i -le 10; $i++) { [W]::SetCursorPos($tx + [int]($DragX * $i / 10), $ty + [int]($DragY * $i / 10)) | Out-Null; Start-Sleep -Milliseconds 40 }
[W]::mouse_event(0x4, 0, 0, 0, [IntPtr]::Zero); Start-Sleep -Milliseconds 500
$w1 = New-Object W+RECT; [W]::GetWindowRect($h, [ref]$w1) | Out-Null
$result.window_before = Rect $w0
$result.window_after = Rect $w1
$result.window_moved = ($w1.L - $w0.L) -eq $DragX -and ($w1.T - $w0.T) -eq $DragY
$result.after_drag_free = FullScreen (Clip)
$result.foreground_is_window = ([W]::GetForegroundWindow() -eq $h)

# 4. a click inside gives the game the mouse again
[W]::GetClientRect($h, [ref]$c) | Out-Null
$p.X = [int](($c.R - $c.L) / 2); $p.Y = [int](($c.B - $c.T) / 2); [W]::ClientToScreen($h, [ref]$p) | Out-Null
Click $p.X $p.Y
$result.after_inside_click_foreground = ([W]::GetForegroundWindow() -eq $h)
$result.after_inside_click_clip = Rect (Clip)
$result.recaptured = -not (FullScreen (Clip))

# leave the user's desktop as it was: focus elsewhere frees the cursor
[W]::SetForegroundWindow($shell) | Out-Null; Start-Sleep -Milliseconds 300
$result.pass = $result.focused_captured -and $result.focus_left -and $result.after_focus_left_free -and $result.cursor_moves_freely -and
    $result.title_click_free -and $result.window_moved -and $result.after_drag_free -and $result.recaptured
$result | ConvertTo-Json | Set-Content $Out
$result | ConvertTo-Json
if ($result.pass) { exit 0 } else { exit 1 }
