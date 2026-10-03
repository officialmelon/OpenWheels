# Click inside the OpenWheels window without touching the real mouse (posts window messages).
#   powershell -ExecutionPolicy Bypass -File tools\click_window.ps1 -X 950 -Y 530   (client coords, physical px)
param([int]$X, [int]$Y, [string]$ProcessName = "OpenWheels", [int]$HoldMs = 80)
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class OwClick {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
}
"@
[void][OwClick]::SetProcessDPIAware()
$p = Get-Process -Name $ProcessName -ErrorAction Stop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
$h = $p.MainWindowHandle
$lp = [IntPtr](($Y -shl 16) -bor ($X -band 0xFFFF))
[void][OwClick]::PostMessage($h, 0x0200, [IntPtr]0, $lp)   # WM_MOUSEMOVE
Start-Sleep -Milliseconds 30
[void][OwClick]::PostMessage($h, 0x0201, [IntPtr]1, $lp)   # WM_LBUTTONDOWN
Start-Sleep -Milliseconds $HoldMs
[void][OwClick]::PostMessage($h, 0x0202, [IntPtr]0, $lp)   # WM_LBUTTONUP
"clicked $X,$Y"
