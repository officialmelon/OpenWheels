<#
.SYNOPSIS
    Builds the Windows release folder and zip: dist\OpenWheels-windows\ and dist\OpenWheels-windows.zip.

.DESCRIPTION
    1. Builds the Release configuration (tools\build.ps1 -Config Release), which also generates
       gametext.tsv / soundlist.tsv and the browser-game art under generated\ from the player's
       own files in binary\.
    2. Copies OpenWheels.exe, its runtime DLLs, the .tsv tables and generated\.
    3. Copies the game's assets (binary\HappyWheels_Android\HW_Android\assets) to assets\ and the
       iOS bundle's resources (level-editor art, Localizable.strings; not its executable) to ios\,
       which the exe finds next to itself.
    4. Adds LICENSE, NOTICE.md and a short README.txt, then zips the folder.

.PARAMETER BuildDir
    Build tree (default: <repo>\build).
.PARAMETER Out
    Output folder (default: <repo>\dist).
#>
[CmdletBinding()]
param(
    [string]$BuildDir,
    [string]$Out
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) { $BuildDir = Join-Path $repo 'build' }
if (-not $Out) { $Out = Join-Path $repo 'dist' }

& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build.ps1') -Config Release -BuildDir $BuildDir
if ($LASTEXITCODE -ne 0) { throw "build failed" }

$bin = Join-Path $BuildDir 'bin\OpenWheels\Release'
$dest = Join-Path $Out 'OpenWheels-windows'
if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
New-Item -ItemType Directory -Force $dest | Out-Null

Copy-Item (Join-Path $bin 'OpenWheels.exe') $dest
Get-ChildItem $bin -Filter *.dll | Copy-Item -Destination $dest
Get-ChildItem $bin -Filter *.tsv | Copy-Item -Destination $dest
if (Test-Path (Join-Path $bin 'generated')) { Copy-Item -Recurse (Join-Path $bin 'generated') $dest }

$assets = Join-Path $repo 'binary\HappyWheels_Android\HW_Android\assets'
if (-not (Test-Path $assets)) { throw "game assets not found: $assets" }
Copy-Item -Recurse $assets (Join-Path $dest 'assets')

# iOS bundle: resources only (the editor's art and text); never the iOS executable or frameworks.
$ios = Join-Path $repo 'binary\HappyWheels_iOS\Payload\happywheels.app'
if (Test-Path $ios) {
    $iosDest = Join-Path $dest 'ios'
    New-Item -ItemType Directory -Force $iosDest | Out-Null
    Get-ChildItem $ios -File | Where-Object { $_.Extension -in '.png', '.plist', '.strings', '.ttf', '.otf', '.fnt', '.xml' } |
        Where-Object { $_.Name -ne 'Info.plist' -and $_.Name -notlike 'embedded*' } |
        Copy-Item -Destination $iosDest
}

Copy-Item (Join-Path $repo 'LICENSE') $dest
Copy-Item (Join-Path $repo 'NOTICE.md') $dest
@"
OpenWheels - an open-source reimplementation of Happy Wheels
https://github.com/officialmelon/OpenWheels

Run OpenWheels.exe.

Keyboard: Up/W accelerate, Down/S reverse, Left/A lean back, Right/D lean forward,
Space special action, Shift/Ctrl character actions, Z eject, Esc/P pause, R restart,
F11 fullscreen. Options -> "quality of life" has extra settings.

Unofficial fan project, not affiliated with or endorsed by Fancy Force.
Happy Wheels and its assets belong to Fancy Force / Jim Bonacci. See NOTICE.md.
"@ | Set-Content -Encoding UTF8 (Join-Path $dest 'README.txt')

$zip = Join-Path $Out 'OpenWheels-windows.zip'
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $dest -DestinationPath $zip
"==> $dest"
"==> $zip ($([math]::Round((Get-Item $zip).Length / 1MB, 1)) MB)"
