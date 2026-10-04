<#
.SYNOPSIS
    Builds the OpenWheels Android APK (android/ Gradle project) from the command line.

.DESCRIPTION
    1. Locates the Android SDK (ANDROID_HOME / ANDROID_SDK_ROOT / %LOCALAPPDATA%\Android\Sdk) and
       installs any missing package the build needs with sdkmanager: platforms;android-35,
       build-tools;35.0.0, cmake;3.22.1, ndk;27.3.13750724.
    2. Checks for a JDK 17+ (java on PATH, JAVA_HOME, or Android Studio's bundled JBR).
    3. Sets up the engine (tools\fetch_engine.ps1) if thirdparty\cocos2d-x is missing.
    4. Runs `gradlew assemble<Config>` in android\. Gradle itself stages the player's own game
       files into the APK (nothing is committed):
         binary\HappyWheels_Android\HW_Android\assets\{shared,sounds,large,medium,small,tiny}
         soundlist.tsv / gametext.tsv generated from the original libMyGame.so
           (tools\re\extract_soundlist.py / extract_gametext.py, needs Python 3 + unicorn as for Win32)
         optional: the iOS app's level-editor files (binary\HappyWheels_iOS\Payload\happywheels.app)
       and builds libOpenWheels.so for each ABI with NDK r27 (android\app\CMakeLists.txt).
    5. Copies the APK to build\android\OpenWheels-<config>.apk; -Install / -Run use adb.

.PARAMETER Config
    Debug (default) or Release. Release is signed with the keystore described by the properties
    file in OW_KEYSTORE_PROPERTIES (storeFile, storePassword, keyAlias, keyPassword; keep it outside
    the repo), or with the debug key when that is not set.
.PARAMETER Abis
    ':'-separated ABIs (default from android\gradle.properties: arm64-v8a:armeabi-v7a:x86).
    Use x86 alone for a quick emulator build, arm64-v8a alone for a quick phone build.
.PARAMETER Assets
    The original game's assets folder (default binary\HappyWheels_Android\HW_Android\assets).
.PARAMETER GameLib
    The original libMyGame.so (arm64) used to generate soundlist.tsv / gametext.tsv.
.PARAMETER IosApp
    Optional iOS happywheels.app folder (level editor art + Localizable.strings).
.PARAMETER BuildRoot
    Put all Gradle/NDK build output under this folder instead of android\app\build and
    android\app\.cxx (several GB for three ABIs) - e.g. on another drive. Default: $env:OW_BUILD_ROOT.
.PARAMETER Install
    adb install -r the APK on the connected device/emulator.
.PARAMETER Run
    Install and launch it.
.PARAMETER Clean
    gradlew clean first.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\build_android.ps1
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\build_android.ps1 -Abis x86 -Run
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug',
    [string]$Abis,
    [string]$Assets,
    [string]$GameLib,
    [string]$IosApp,
    [string]$BuildRoot = $env:OW_BUILD_ROOT,
    [switch]$Install,
    [switch]$Run,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$repo = Split-Path -Parent $PSScriptRoot
$proj = Join-Path $repo 'android'

$NdkVersion = '27.3.13750724'
$CMakeVersion = '3.22.1'
$Platform = 'android-35'
$BuildTools = '35.0.0'

function Fail([string]$msg) {
    Write-Host "build_android.ps1: $msg" -ForegroundColor Red
    exit 1
}
function Step([string]$msg) { Write-Host "==> $msg" -ForegroundColor Cyan }

# --- Android SDK -------------------------------------------------------------------------------
$sdk = $env:ANDROID_HOME
if (-not $sdk -or -not (Test-Path $sdk)) { $sdk = $env:ANDROID_SDK_ROOT }
if (-not $sdk -or -not (Test-Path $sdk)) { $sdk = Join-Path $env:LOCALAPPDATA 'Android\Sdk' }
if (-not (Test-Path $sdk)) {
    Fail "Android SDK not found. Install Android Studio or the command-line tools and set ANDROID_HOME."
}
$env:ANDROID_HOME = $sdk
$env:ANDROID_SDK_ROOT = $sdk

$sdkmanager = Get-ChildItem -Path (Join-Path $sdk 'cmdline-tools') -Recurse -Filter 'sdkmanager.bat' -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1
$need = @()
if (-not (Test-Path (Join-Path $sdk "platforms\$Platform"))) { $need += "platforms;$Platform" }
if (-not (Test-Path (Join-Path $sdk "build-tools\$BuildTools"))) { $need += "build-tools;$BuildTools" }
if (-not (Test-Path (Join-Path $sdk "cmake\$CMakeVersion"))) { $need += "cmake;$CMakeVersion" }
if (-not (Test-Path (Join-Path $sdk "ndk\$NdkVersion"))) { $need += "ndk;$NdkVersion" }
if (-not (Test-Path (Join-Path $sdk 'platform-tools'))) { $need += 'platform-tools' }
if ($need.Count -gt 0) {
    if (-not $sdkmanager) { Fail "missing SDK packages ($($need -join ', ')) and no sdkmanager in $sdk\cmdline-tools" }
    Step "installing SDK packages: $($need -join ', ')"
    ("y`n" * 20) | & $sdkmanager.FullName --licenses | Out-Null
    & $sdkmanager.FullName @need
    if ($LASTEXITCODE -ne 0) { Fail "sdkmanager failed ($LASTEXITCODE)" }
}

# --- JDK 17+ -------------------------------------------------------------------------------------
function Get-JavaMajor([string]$javaExe) {
    try {
        $out = & $javaExe -version 2>&1 | Out-String
        if ($out -match 'version "(\d+)') { return [int]$Matches[1] }
    } catch { }
    return 0
}
$javaOk = $false
if ($env:JAVA_HOME -and (Test-Path (Join-Path $env:JAVA_HOME 'bin\java.exe'))) {
    $javaOk = (Get-JavaMajor (Join-Path $env:JAVA_HOME 'bin\java.exe')) -ge 17
}
if (-not $javaOk) {
    $onPath = Get-Command java -ErrorAction SilentlyContinue
    if ($onPath -and (Get-JavaMajor $onPath.Source) -ge 17) {
        $env:JAVA_HOME = Split-Path -Parent (Split-Path -Parent $onPath.Source)
        $javaOk = $true
    }
}
if (-not $javaOk) {
    foreach ($jbr in @("$env:ProgramFiles\Android\Android Studio\jbr", "$env:LOCALAPPDATA\Programs\Android Studio\jbr")) {
        if (Test-Path (Join-Path $jbr 'bin\java.exe')) { $env:JAVA_HOME = $jbr; $javaOk = $true; break }
    }
}
if (-not $javaOk) { Fail "a JDK 17 or newer is required (set JAVA_HOME)." }

# --- Engine -------------------------------------------------------------------------------------
if (-not (Test-Path (Join-Path $repo 'thirdparty\cocos2d-x\cocos\cocos2d.h'))) {
    Step 'setting up cocos2d-x 3.17.2 (tools\fetch_engine.ps1)'
    & (Join-Path $PSScriptRoot 'fetch_engine.ps1')
    if ($LASTEXITCODE -ne 0) { Fail 'fetch_engine.ps1 failed' }
}

# --- Gradle -------------------------------------------------------------------------------------
$gradleArgs = @('--console=plain')
if ($Abis)    { $gradleArgs += "-POW_ABIS=$Abis" }
if ($Assets)  { $gradleArgs += "-POW_GAME_ASSETS=$((Resolve-Path $Assets).Path)" }
if ($GameLib) { $gradleArgs += "-POW_GAME_LIB=$((Resolve-Path $GameLib).Path)" }
if ($IosApp)  { $gradleArgs += "-POW_IOS_APP=$((Resolve-Path $IosApp).Path)" }
if ($BuildRoot) {
    New-Item -ItemType Directory -Force $BuildRoot | Out-Null
    $BuildRoot = (Resolve-Path $BuildRoot).Path
    $gradleArgs += "-POW_BUILD_ROOT=$($BuildRoot -replace '\\', '/')"
}
if ($Clean)   { $gradleArgs += 'clean' }
$gradleArgs += "assemble$Config"

Step "gradlew $($gradleArgs -join ' ')"
Push-Location $proj
try {
    & (Join-Path $proj 'gradlew.bat') @gradleArgs
    $code = $LASTEXITCODE
} finally {
    Pop-Location
}
if ($code -ne 0) { Fail "Gradle build failed ($code)" }

$variant = $Config.ToLower()
$appBuild = if ($BuildRoot) { Join-Path $BuildRoot 'app' } else { Join-Path $proj 'app\build' }
$apkDir = Join-Path $appBuild "outputs\apk\$variant"
$apk = Get-ChildItem $apkDir -Filter '*.apk' -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $apk) { Fail "no APK found in $apkDir" }
$outDir = Join-Path $repo 'build\android'
New-Item -ItemType Directory -Force $outDir | Out-Null
$outApk = Join-Path $outDir "OpenWheels-$variant.apk"
Copy-Item -Force $apk.FullName $outApk
Step ("APK: $outApk ({0:N1} MB)" -f ((Get-Item $outApk).Length / 1MB))

# --- Install / run ------------------------------------------------------------------------------
if ($Install -or $Run) {
    $adb = Join-Path $sdk 'platform-tools\adb.exe'
    Step 'adb install -r'
    & $adb install -r $outApk
    if ($LASTEXITCODE -ne 0) { Fail "adb install failed ($LASTEXITCODE)" }
    if ($Run) {
        & $adb shell am start -n org.openwheels.game/.AppActivity
    }
}
