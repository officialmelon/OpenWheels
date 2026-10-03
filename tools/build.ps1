<#
.SYNOPSIS
    Configures (when needed) and builds OpenWheels for Win32/x86 with the CMake
    that ships with Visual Studio.

.DESCRIPTION
    One multi-config Visual Studio build tree (default: build\) serves all
    configurations. The first run configures it with
        -G "Visual Studio 17 2022" -A Win32 -T v143
        -DCMAKE_GENERATOR_INSTANCE=<that Visual Studio's install dir, via vswhere>
    (the v3-deps-158 win32 prebuilts are 32-bit only; the instance must be
    pinned because VS 18's bundled CMake otherwise binds the 2022 generator to
    VS 18's MSBuild, which rejects v143 with MSB8052). Later runs reuse the
    cached generator/toolset and CMake re-configures by itself when
    CMakeLists.txt or the set of files under src\ changes.

    Only VS 18 installed? Use a separate -BuildDir with
    -Generator "Visual Studio 18 2026" -Toolset v145 (MSVC 14.5x; VS 18's
    MSBuild cannot drive v143 through CMake).

    Output: <BuildDir>\bin\OpenWheels\<Config>\OpenWheels.exe plus the engine's
    runtime DLLs, copied next to it by a post-build step. Run it from there;
    cocos2d::log output also lands in openwheels.log next to the exe.

    Never uses a cmake.exe from PATH (msys/devkitPro ones are not MSVC-aware):
    -CMake if given, else the cmake that configured the build tree, else the
    newest Visual Studio's bundled CMake.

.PARAMETER Config
    RelWithDebInfo (default: optimized + PDB), Debug or Release.
.PARAMETER BuildDir
    Build tree (default: <repo>\build).
.PARAMETER Generator
    Visual Studio generator for the first configure (e.g. "Visual Studio 18 2026"
    on a machine without VS 2022). Ignored once the build tree exists.
.PARAMETER Toolset
    Platform toolset for the first configure (default v143 = MSVC 14.4x).
.PARAMETER Reconfigure
    Re-run the configure step even though the build tree exists.
.PARAMETER Clean
    Rebuild the configuration from scratch (cmake --build --clean-first).
.PARAMETER CMake
    Explicit path to cmake.exe.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\build.ps1
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Config Debug
#>
[CmdletBinding()]
param(
    [ValidateSet('RelWithDebInfo', 'Debug', 'Release')]
    [string]$Config = 'RelWithDebInfo',
    [string]$BuildDir,
    [string]$Generator = 'Visual Studio 17 2022',
    [string]$Toolset = 'v143',
    [switch]$Reconfigure,
    [switch]$Clean,
    [string]$CMake
)

$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) {
    $BuildDir = Join-Path $repo 'build'
}
$cache = Join-Path $BuildDir 'CMakeCache.txt'

function Fail([string]$msg) {
    Write-Host "build.ps1: $msg" -ForegroundColor Red
    exit 1
}

function Find-CMake {
    if ($CMake) {
        if (-not (Test-Path $CMake)) { Fail "-CMake $CMake does not exist" }
        return $CMake
    }
    # The cmake that configured this tree: keeps cmake version and tree in sync.
    if (Test-Path $cache) {
        $m = Select-String -Path $cache -Pattern '^CMAKE_COMMAND:INTERNAL=(.+)$' | Select-Object -First 1
        if ($m) {
            $p = $m.Matches[0].Groups[1].Value
            if (Test-Path $p) { return $p }
        }
    }
    # Visual Studio's bundled CMake, newest Visual Studio first.
    $rel = 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        foreach ($inst in (& $vswhere -all -prerelease -products * -sort -property installationPath)) {
            $p = Join-Path $inst $rel
            if (Test-Path $p) { return $p }
        }
    }
    foreach ($vs in '18', '2022') {
        $p = Join-Path $env:ProgramFiles "Microsoft Visual Studio\$vs\Community\$rel"
        if (Test-Path $p) { return $p }
    }
    Fail "Visual Studio's bundled cmake.exe was not found; pass -CMake <path>."
}

# Install dir of the Visual Studio that matches a "Visual Studio <N> <year>" generator.
function Find-VSInstance([string]$gen) {
    if ($gen -notmatch '^Visual Studio (\d+) ') { Fail "unsupported generator '$gen' (use a Visual Studio generator)" }
    $major = [int]$Matches[1]
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { Fail "vswhere.exe not found ($vswhere)" }
    $inst = & $vswhere -version "[$major.0,$($major + 1).0)" -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath |
        Select-Object -First 1
    if (-not $inst) { Fail "no Visual Studio $major installation with the C++ x86/x64 tools for '$gen'" }
    return $inst
}

if (-not (Test-Path (Join-Path $repo 'thirdparty\cocos2d-x\cocos\cocos2d.h'))) {
    Fail 'cocos2d-x 3.17.2 is not set up in thirdparty\cocos2d-x - run tools\fetch_engine.ps1 first.'
}

$cmakeExe = Find-CMake
Write-Host "==> cmake: $cmakeExe"
$timer = [Diagnostics.Stopwatch]::StartNew()


if (-not (Test-Path $cache)) {
    $instance = (Find-VSInstance $Generator) -replace '\\', '/'
    Write-Host "==> configuring $BuildDir ($Generator @ $instance, Win32, $Toolset)" -ForegroundColor Cyan
    & $cmakeExe -S $repo -B $BuildDir -G $Generator -A Win32 -T $Toolset "-DCMAKE_GENERATOR_INSTANCE=$instance"
    if ($LASTEXITCODE -ne 0) { Fail "configure failed (exit $LASTEXITCODE)" }
} else {
    if ($Reconfigure) {
        Write-Host "==> re-configuring $BuildDir" -ForegroundColor Cyan
        & $cmakeExe -S $repo -B $BuildDir
        if ($LASTEXITCODE -ne 0) { Fail "configure failed (exit $LASTEXITCODE)" }
    }
}

Write-Host "==> building $Config" -ForegroundColor Cyan
$buildArgs = @('--build', $BuildDir, '--config', $Config, '--parallel')
if ($Clean) { $buildArgs += '--clean-first' }
& $cmakeExe @buildArgs
if ($LASTEXITCODE -ne 0) { Fail "build failed (exit $LASTEXITCODE)" }

$exe = Join-Path $BuildDir "bin\OpenWheels\$Config\OpenWheels.exe"
if (-not (Test-Path $exe)) { Fail "build succeeded but $exe is missing" }
Write-Host ("==> OK in {0:mm\:ss}: {1}" -f $timer.Elapsed, $exe) -ForegroundColor Green
