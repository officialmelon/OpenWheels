<#
.SYNOPSIS
    Sets up the cocos2d-x 3.17.2 engine for OpenWheels in thirdparty\cocos2d-x.

.DESCRIPTION
    1. Downloads (only if missing) into thirdparty\_downloads\:
         cocos2d-x-3.17.2.zip  github.com/cocos2d/cocos2d-x, tag cocos2d-x-3.17.2
         v3-deps-158.zip       github.com/cocos2d/cocos2d-x-3rd-party-libs-bin, tag v3-deps-158
    2. Extracts the engine to thirdparty\cocos2d-x (skipped when it already
       exists, unless -Force).
    3. Installs the prebuilt dependencies the way the engine's own
       download-deps.py does: the deps archive goes into external\ (the
       engine's external\config.json is kept), then config.json's "move_dirs"
       are applied (external\fbx-conv -> tools\fbx-conv).
    4. Applies thirdparty\patches\*.patch in name order with `git apply -p1`
       (paths relative to the engine root, made against a pristine extraction
       of cocos2d-x-3.17.2.zip). Patches that are already applied are skipped,
       so the script can be re-run at any time. cocos2d-x 3.17.2 currently
       builds with MSVC v143 without any patch, so the folder may be empty.

    Box2D is the v3-deps-158 prebuilt (external\Box2D) and is never patched.

    Making a patch: put a pristine extraction in a\ and the fixed tree in b\
    (same parent folder), then let git write the file itself - piping through
    PowerShell would turn LF into CRLF and break the patch:
        git diff --no-index --no-prefix --output=<repo>\thirdparty\patches\cocos2d-x-3.17.2-msvc.patch a b

.PARAMETER Force
    Delete and recreate thirdparty\cocos2d-x even if it already exists.
.PARAMETER Redownload
    Download the zips again even if they are already present.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\fetch_engine.ps1
#>
[CmdletBinding()]
param(
    [switch]$Force,
    [switch]$Redownload
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'   # progress bars make Invoke-WebRequest/Expand-Archive crawl on PS 5.1

$repo       = Split-Path -Parent $PSScriptRoot
$thirdparty = Join-Path $repo 'thirdparty'
$downloads  = Join-Path $thirdparty '_downloads'
$engine     = Join-Path $thirdparty 'cocos2d-x'
$patchDir   = Join-Path $thirdparty 'patches'
$staging    = Join-Path $thirdparty '_extract'

# Sha256 = the archives the OpenWheels build was set up and verified with.
# GitHub generates these zips on the fly, so a different hash only warns.
$engineZip = @{
    Name   = 'cocos2d-x-3.17.2.zip'
    Url    = 'https://github.com/cocos2d/cocos2d-x/archive/cocos2d-x-3.17.2.zip'
    Root   = 'cocos2d-x-cocos2d-x-3.17.2'
    Sha256 = '2C5029040877578AAF25D4F622F767003BCAD9AA39F181E08D136EE596F6854D'
}
$depsZip = @{
    Name   = 'v3-deps-158.zip'
    Url    = 'https://github.com/cocos2d/cocos2d-x-3rd-party-libs-bin/archive/v3-deps-158.zip'
    Root   = 'cocos2d-x-3rd-party-libs-bin-3-deps-158'   # GitHub drops the tag's leading 'v'
    Sha256 = 'D591197016FF6F16FB8FFF07B13BC7CB381EFBE9A177EA9A0C4089C303139757'
}

function Step([string]$msg) { Write-Host "==> $msg" -ForegroundColor Cyan }

function Get-Archive($a) {
    $path = Join-Path $downloads $a.Name
    if ($Redownload -or -not (Test-Path $path)) {
        New-Item -ItemType Directory -Force $downloads | Out-Null
        Step "downloading $($a.Url)"
        [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
        $part = "$path.part"
        Invoke-WebRequest -Uri $a.Url -OutFile $part -UseBasicParsing
        Move-Item -Force $part $path
    }
    $hash = (Get-FileHash -Algorithm SHA256 $path).Hash
    if ($hash -ne $a.Sha256) {
        Write-Warning "$($a.Name): SHA256 $hash differs from the verified archive ($($a.Sha256))."
    }
    return $path
}

# Extracts $zip into $dest and returns the path of the archive's top-level folder.
function Expand-Zip([string]$zip, [string]$dest, [string]$root) {
    New-Item -ItemType Directory -Force $dest | Out-Null
    # Windows' own bsdtar is much faster than Expand-Archive. Call it by full
    # path: a `tar` from msys/Git on PATH would parse "C:" as a remote host.
    $tar = Join-Path $env:SystemRoot 'System32\tar.exe'
    if (Test-Path $tar) {
        & $tar -xf $zip -C $dest
        if ($LASTEXITCODE -ne 0) { throw "tar failed ($LASTEXITCODE) on $zip" }
    } else {
        Expand-Archive -LiteralPath $zip -DestinationPath $dest -Force
    }
    $top = Join-Path $dest $root
    if (-not (Test-Path $top)) { throw "$zip does not contain the expected folder $root" }
    return $top
}

function Install-Engine {
    $ezip = Get-Archive $engineZip
    $dzip = Get-Archive $depsZip

    if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
    try {
        Step "extracting $($engineZip.Name)"
        $src = Expand-Zip $ezip $staging $engineZip.Root
        Move-Item $src $engine

        Step "installing $($depsZip.Name) into external\ (download-deps.py layout)"
        $external = Join-Path $engine 'external'
        # download-deps.py: empty external\ except config.json, copy the deps in.
        Get-ChildItem -Force $external | Where-Object { $_.Name -ne 'config.json' } |
            Remove-Item -Recurse -Force
        $deps = Expand-Zip $dzip $staging $depsZip.Root
        Get-ChildItem -Force $deps | Where-Object { $_.Name -ne 'config.json' } |
            Move-Item -Destination $external
        # ... then apply config.json's move_dirs (fbx-conv -> tools\fbx-conv).
        $config = Get-Content -Raw (Join-Path $external 'config.json') | ConvertFrom-Json
        if ($config.move_dirs) {
            foreach ($p in $config.move_dirs.PSObject.Properties) {
                $from = Join-Path $external $p.Name
                $to = Join-Path (Join-Path $engine $p.Value) $p.Name
                if (Test-Path $from) {
                    if (Test-Path $to) { Remove-Item -Recurse -Force $to }
                    Move-Item $from $to
                }
            }
        }
    } finally {
        if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
    }
}

# Runs git quietly and reports success. ErrorActionPreference is relaxed locally:
# Windows PowerShell 5.1 turns redirected native stderr into terminating errors.
function Test-Git([string[]]$GitArgs) {
    $ErrorActionPreference = 'Continue'
    & git @GitArgs 2>&1 | Out-Null
    return ($LASTEXITCODE -eq 0)
}

function Install-Patches {
    $patches = @(Get-ChildItem -File (Join-Path $patchDir '*.patch') -ErrorAction SilentlyContinue | Sort-Object Name)
    if ($patches.Count -eq 0) {
        Step 'no engine patches in thirdparty\patches (none needed for MSVC v143)'
        return
    }
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw 'git is required to apply thirdparty\patches\*.patch' }
    # Run from the repo root: --directory re-roots the patch's a/ b/ paths into
    # the (git-ignored) engine folder; works whether or not it is inside a repo.
    $dirArg = '--directory=thirdparty/cocos2d-x'
    Push-Location $repo
    try {
        foreach ($p in $patches) {
            $rel = "thirdparty/patches/$($p.Name)"
            if (Test-Git @('apply', '--check', '-p1', $dirArg, $rel)) {
                & git apply -p1 --whitespace=nowarn $dirArg $rel
                if ($LASTEXITCODE -ne 0) { throw "git apply failed for $rel" }
                Step "applied $($p.Name)"
            } elseif (Test-Git @('apply', '--check', '--reverse', '-p1', $dirArg, $rel)) {
                Step "already applied: $($p.Name)"
            } else {
                throw "$rel neither applies nor is already applied to thirdparty\cocos2d-x (re-run with -Force for a pristine engine)"
            }
        }
    } finally {
        Pop-Location
    }
}

if (Test-Path $engine) {
    if ($Force) {
        Step "removing existing $engine (-Force)"
        Remove-Item -Recurse -Force $engine
    } else {
        Step "$engine already exists (use -Force to recreate it)"
    }
}
if (-not (Test-Path $engine)) { Install-Engine }
Install-Patches

if (-not (Test-Path (Join-Path $engine 'cocos\cocos2d.h')) -or
    -not (Test-Path (Join-Path $engine 'external\Box2D\prebuilt\win32\release\libbox2d.lib'))) {
    throw "engine setup incomplete: $engine"
}
Step "engine ready: $engine"
