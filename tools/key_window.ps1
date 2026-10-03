# Hold a key in the OpenWheels window without touching the real keyboard (posts WM_KEYDOWN/UP).
#   powershell -ExecutionPolicy Bypass -File tools\key_window.ps1 -Vk 0x26 -HoldMs 2000   (0x26 = Up arrow)
param([int]$Vk, [int]$HoldMs = 500, [string]$ProcessName = "OpenWheels")
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class OwKey {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
}
"@
$p = Get-Process -Name $ProcessName -ErrorAction Stop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
$h = $p.MainWindowHandle
$scan = [OwKey]::MapVirtualKey([uint32]$Vk, 0)
$ext = if ($Vk -in 0x25,0x26,0x27,0x28) { 1 -shl 24 } else { 0 }   # arrow keys are extended keys
$down = [IntPtr](1 -bor ($scan -shl 16) -bor $ext)
$up = [IntPtr](1 -bor ($scan -shl 16) -bor $ext -bor (1 -shl 30) -bor (1 -shl 31))
[void][OwKey]::PostMessage($h, 0x0100, [IntPtr]$Vk, $down)   # WM_KEYDOWN
Start-Sleep -Milliseconds $HoldMs
[void][OwKey]::PostMessage($h, 0x0101, [IntPtr]$Vk, $up)     # WM_KEYUP
"key 0x{0:x} held {1} ms" -f $Vk, $HoldMs
