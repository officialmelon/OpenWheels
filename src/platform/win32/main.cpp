// Win32 entry point: WinMain -> Application::getInstance()->run(), as in
// cocos2d-x 3.17.2 templates/cpp-template-default/proj.win32/main.cpp.
//
// Everything in this file is PC platform glue - the Android build's equivalent lives in
// Java (Cocos2dxActivity) and javaactivity-android.cpp. The game itself (src/game) is the
// faithful reconstruction and is not modified for PC.
//
// GL view: like the Android platform layer (Cocos2dxRenderer.nativeInit), the platform creates
// the GL view with the "device" frame size before the app runs, so the game's
// applicationDidFinishLaunching() already finds Director::getOpenGLView() set and its own
// GLViewImpl::create() fallback stays unused, as on Android. The frame height selects the
// original's asset tier (tiny/small/medium/large), exactly as a device's screen height would.
//
// Game files: the player's own extracted Android assets folder (never shipped with OpenWheels)
// becomes the FileUtils resource root - Android reads the same paths from the APK's assets/.
//
// Logging: on win32 cocos2d::log() writes every line to OutputDebugString *and* to the CRT
// stdout. A /SUBSYSTEM:WINDOWS process has no usable stdout, so by default stdout/stderr are
// redirected into openwheels.log next to the exe (truncated on each start). --console opens a
// console window instead.
//
// Command line:
//   --assets <dir>        extracted game assets (folder containing shared/, sounds/, large/...)
//   --ios-app <dir>       optional iOS happywheels.app (level editor art + Localizable.strings)
//   --width <px> --height <px>   window ("device") size, default 1600x900
//   --console             log to a console window
//   --dump-world <out.json> [--level levels/<chapter>/<file>.xml] [--frames N] [--script f:hex,...]
//                         verification: play the level with scripted controls at exactly 1/60 s
//                         per frame and write the Box2D world (tools/re/worlddiff.py vs the oracle)
//   --dump-at a,b,c       with --dump-world: also write <out>_f<N>.json at those frames (oracle naming)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <tchar.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <vector>

#include "platform/CCStdC.h"
#include "cocos2d.h"
#include "AppDelegate.h"
#include "platform/common/IOSBundle.h"
#include "platform/common/Localization.h"

#ifdef OW_WITH_PC_LAYER  // enable once src/game links (PCInput.cpp, WorldDumpRunner.cpp)
#include "platform/win32/PCInput.h"
#include "platform/win32/WorldDumpRunner.h"
#endif

USING_NS_CC;

namespace {

std::wstring exeDirectory()
{
    wchar_t path[MAX_PATH] = {0};
    const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir(path, len);
    const size_t slash = dir.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : dir.substr(0, slash + 1);
}

std::string narrow(const std::wstring& w)
{
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

bool isDirectory(const std::wstring& p)
{
    const DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring withSlash(std::wstring p)
{
    for (auto& c : p) if (c == L'\\') c = L'/';
    if (!p.empty() && p.back() != L'/') p += L'/';
    return p;
}

// A folder looks like the game's assets if it has the original's search-path roots.
bool looksLikeAssets(const std::wstring& dir)
{
    return isDirectory(dir + L"shared/levels") && isDirectory(dir + L"sounds");
}

// --assets wins; then <exe>/assets; then the repo's binary/ reference folder found by walking up.
std::wstring findAssets(const std::wstring& fromArgs)
{
    if (!fromArgs.empty()) return withSlash(fromArgs);
    const std::wstring exe = withSlash(exeDirectory());
    if (looksLikeAssets(exe + L"assets/")) return exe + L"assets/";
    std::wstring dir = exe;
    for (int up = 0; up < 8 && !dir.empty(); ++up)
    {
        const std::wstring candidate = dir + L"binary/HappyWheels_Android/HW_Android/assets/";
        if (looksLikeAssets(candidate)) return candidate;
        dir = dir.substr(0, dir.find_last_of(L'/', dir.size() - 2) + 1);
    }
    return std::wstring();
}

// Optional iOS app bundle (happywheels.app): level-editor art + Localizable.strings.
std::wstring findIOSBundle(const std::wstring& fromArgs)
{
    if (!fromArgs.empty()) return withSlash(fromArgs);
    std::wstring dir = withSlash(exeDirectory());
    for (int up = 0; up < 8 && !dir.empty(); ++up)
    {
        const std::wstring candidate = dir + L"binary/HappyWheels_iOS/Payload/happywheels.app/";
        if (GetFileAttributesW((candidate + L"Localizable.strings").c_str()) != INVALID_FILE_ATTRIBUTES)
            return candidate;
        dir = dir.substr(0, dir.find_last_of(L'/', dir.size() - 2) + 1);
    }
    return std::wstring();
}

struct Options
{
    std::wstring assets;
    std::wstring iosApp;
    float width = 1600.0f;
    float height = 900.0f;
    bool console = false;
    std::wstring dumpWorld;
    std::string level = "levels/01_business_guy/01_business_guy_tutorial_level.xml";
    int frames = 120;
    std::string script = "0:00";
    std::string dumpAt;
};

Options parseOptions()
{
    Options o;
    bool explicitSize = false;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i)
    {
        const std::wstring a = argv[i];
        auto next = [&]() -> std::wstring { return (i + 1 < argc) ? std::wstring(argv[++i]) : std::wstring(); };
        if (a == L"--assets") o.assets = next();
        else if (a == L"--ios-app") o.iosApp = next();
        else if (a == L"--width") { o.width = (float)_wtof(next().c_str()); explicitSize = true; }
        else if (a == L"--height") { o.height = (float)_wtof(next().c_str()); explicitSize = true; }
        else if (a == L"--console") o.console = true;
        else if (a == L"--dump-world") o.dumpWorld = next();
        else if (a == L"--level") o.level = narrow(next());
        else if (a == L"--frames") o.frames = _wtoi(next().c_str());
        else if (a == L"--script") o.script = narrow(next());
        else if (a == L"--dump-at") o.dumpAt = narrow(next());
    }
    LocalFree(argv);
    if (!explicitSize)
    {
        // Default: the largest 16:9 frame up to 1600x900 that fits ~90% of the monitor's work
        // area (Windows silently shrinks bigger windows, which would desync cocos2d's frame size).
        SetProcessDPIAware();  // physical pixels, like GLFW will use
        RECT work = {0, 0, 1600, 900};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        const float maxW = (work.right - work.left) * 0.9f;
        const float maxH = (work.bottom - work.top) * 0.9f - 40.0f;  // title bar
        float scale = 1.0f;
        if (o.width * scale > maxW) scale = maxW / o.width;
        if (o.height * scale > maxH) scale = maxH / o.height;
        o.width = std::floor(o.width * scale);
        o.height = std::floor(o.height * scale);
    }
    if (o.width < 320.0f) o.width = 320.0f;
    if (o.height < 240.0f) o.height = 240.0f;
    return o;
}

// Note: the non-_s CRT open functions are used on purpose - fopen_s and
// friends open files exclusively, which would stop tools from tailing the log.
void setupLogging(bool console)
{
    if (console && AllocConsole())
    {
        _wfreopen(L"CONOUT$", L"w", stdout);
        _wfreopen(L"CONOUT$", L"w", stderr);
        return;
    }

    // Truncate once, then attach stdout and stderr in append mode so both
    // streams interleave in one file. Unbuffered so a crash loses nothing.
    const std::wstring logPath = exeDirectory() + L"openwheels.log";
    if (FILE* f = _wfopen(logPath.c_str(), L"w"))
        fclose(f);
    if (_wfreopen(logPath.c_str(), L"a", stdout))
        setvbuf(stdout, nullptr, _IONBF, 0);
    if (_wfreopen(logPath.c_str(), L"a", stderr))
        setvbuf(stderr, nullptr, _IONBF, 0);
}

} // namespace

int WINAPI _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    const Options opt = parseOptions();
    setupLogging(opt.console);

    const std::wstring assets = findAssets(opt.assets);
    if (assets.empty())
    {
        MessageBoxW(nullptr,
                    L"OpenWheels needs the game files from your own copy of Happy Wheels (Android).\n\n"
                    L"Extract the APK's assets folder and start OpenWheels with\n"
                    L"    --assets <path-to-assets>\n"
                    L"or place it next to OpenWheels.exe as an 'assets' folder.",
                    L"OpenWheels - game files not found", MB_ICONERROR | MB_OK);
        return 1;
    }

    // create the application instance
    AppDelegate app;

    // Resource root = the player's assets (Android: the APK's assets/). The game's own
    // AppDelegate then adds "shared", "sounds" and the size tier relative to it. The exe folder
    // holds the build-generated data tables (soundlist.tsv, gametext.tsv).
    auto fileUtils = FileUtils::getInstance();
    fileUtils->setDefaultResourceRootPath(narrow(assets));
    fileUtils->addSearchPath(narrow(withSlash(exeDirectory())), false);
    cocos2d::log("OpenWheels: assets = %s", narrow(assets).c_str());
    const std::wstring iosApp = findIOSBundle(opt.iosApp);
    if (!iosApp.empty())
    {
        openwheels::setIOSBundlePath(narrow(iosApp));
        cocos2d::log("OpenWheels: iOS bundle = %s (%d localized strings)", narrow(iosApp).c_str(),
                     (int)Localization::size());
    }

    // Android asks the app for its GL context attributes before the surface
    // exists; run() calls initGLContextAttrs() again, which is harmless.
    Application::getInstance()->initGLContextAttrs();
    auto glview = GLViewImpl::createWithRect("OpenWheels", Rect(0.0f, 0.0f, opt.width, opt.height));
    Director::getInstance()->setOpenGLView(glview);

#ifdef OW_WITH_PC_LAYER  // enable once src/game links (PCInput.cpp, WorldDumpRunner.cpp)
    openwheels::pc::installKeyboardControls();
    if (!opt.dumpWorld.empty())
    {
        return openwheels::pc::runWorldDump(narrow(opt.dumpWorld), opt.level, opt.frames, opt.script,
                                             opt.dumpAt);
    }
#endif

    return Application::getInstance()->run();
}
