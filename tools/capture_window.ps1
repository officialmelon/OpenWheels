# Capture the OpenWheels window (even if covered by other windows) to a PNG.
#   powershell -ExecutionPolicy Bypass -File tools\capture_window.ps1 -Out shot.png [-ProcessName OpenWheels]
param([string]$Out = "openwheels_shot.png", [string]$ProcessName = "OpenWheels")
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class OwWin {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
}
"@
[void][OwWin]::SetProcessDPIAware()   # physical pixels (the game window is DPI-aware)
$p = Get-Process -Name $ProcessName -ErrorAction Stop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $p) { throw "no window for $ProcessName" }
$r = New-Object OwWin+RECT
[void][OwWin]::GetWindowRect($p.MainWindowHandle, [ref]$r)
$w = $r.R - $r.L; $h = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[void][OwWin]::PrintWindow($p.MainWindowHandle, $hdc, 2)   # PW_RENDERFULLCONTENT: includes GL content
$g.ReleaseHdc($hdc); $g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
"saved $Out ($w x $h)"
