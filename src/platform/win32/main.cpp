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
//   --open <file> | <file>  open a .happywheels / level .xml (user levels, like the iOS "Open in")
//   --play-online <id>    download a browser Happy Wheels level by id and play it (src/online)
//   --play-level <xml>    play a level XML file (mobile/editor format) as a user level
//   --select-character <id>  with --play-level: open character select first, on character <id>
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
#include <functional>
#include <string>
#include <vector>

#include "platform/CCStdC.h"
#include "cocos2d.h"
#include "AppDelegate.h"
#include "platform/common/IOSBundle.h"
#include "platform/common/Localization.h"
#include "LevelSession.h"
#include "qol/QoL.h"
#include "MainMenu.h"
#include "online/OnlinePlay.h"
#include "online/FlashLevelConverter.h"

#ifdef OW_WITH_PC_LAYER  // enable once src/game links (PCInput.cpp, WorldDumpRunner.cpp)
#include "platform/win32/CrashHandler.h"
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
    // Release folder layout: ios/ next to the exe (tools/package_windows.ps1).
    if (GetFileAttributesW((dir + L"ios/Localizable.strings").c_str()) != INVALID_FILE_ATTRIBUTES)
        return dir + L"ios/";
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
    int playOnline = 0;      // --play-online <level id>
    std::wstring playLevel;  // --play-level <level.xml>
    int selectCharacter = 0;  // --select-character <id>
    std::wstring convertIn, convertOut;  // --convert-flash <in> <out> (PC-only test hook)
    std::wstring openFile;   // .happywheels / level .xml to open (command line or drag-and-drop onto the exe)
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
        else if (a == L"--open") o.openFile = next();
        else if (a == L"--play-online") o.playOnline = _wtoi(next().c_str());
        else if (a == L"--play-level") o.playLevel = next();
        else if (a == L"--select-character") o.selectCharacter = _wtoi(next().c_str());
        else if (a == L"--convert-flash") { o.convertIn = next(); o.convertOut = next(); }
        else if (a.size() > 4 && a[0] != L'-') o.openFile = a;   // file passed by Explorer / drag-and-drop
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

// --convert-flash <in> <out>: see _tWinMain.
int convertFlashLevel(const std::wstring& in, const std::wstring& out)
{
    std::string xml;
    if (FILE* f = _wfopen(in.c_str(), L"rb"))
    {
        char buffer[65536];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) xml.append(buffer, n);
        fclose(f);
    }
    online::ConversionReport report;
    const std::string mobile = online::FlashLevelConverter::toMobile(xml, &report);
    std::string text = report.ok ? "ok\n" : "error: " + report.error + "\n";
    text += "character " + std::to_string(report.character) + (report.forceCharacter ? " forced" : "") +
            "\ndropped " + std::to_string(report.droppedItems) + " substituted " +
            std::to_string(report.substitutedItems) + (report.hasUserVehicle ? " vehicle" : "") + "\n";
    for (const std::string& w : report.warnings) text += "warning: " + w + "\n";
    if (FILE* f = _wfopen((out + L".report.txt").c_str(), L"wb"))
    {
        fwrite(text.data(), 1, text.size(), f);
        fclose(f);
    }
    if (!report.ok) return 1;
    FILE* f = _wfopen(out.c_str(), L"wb");
    if (!f) return 2;
    fwrite(mobile.data(), 1, mobile.size(), f);
    fclose(f);
    return 0;
}

// QOL page "fullscreen" (and F11): exclusive fullscreen on the window's monitor, restoring the
// windowed position and size afterwards. Applies the saved choice and the FPS counter at start-up.
void installFullscreen(GLViewImpl* glview, bool interactive)
{
    static int windowed[4] = {0, 0, 0, 0};
    qol::setFullscreenHandler([glview](bool on) {
        GLFWwindow* window = glview->getWindow();
        if (!window) return;
        const bool isFullscreen = glfwGetWindowMonitor(window) != nullptr;
        if (on == isFullscreen) return;
        if (on)
        {
            glfwGetWindowPos(window, &windowed[0], &windowed[1]);
            glfwGetWindowSize(window, &windowed[2], &windowed[3]);
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            int count = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&count);
            const int cx = windowed[0] + windowed[2] / 2, cy = windowed[1] + windowed[3] / 2;
            for (int i = 0; i < count; ++i)
            {
                int mx, my;
                glfwGetMonitorPos(monitors[i], &mx, &my);
                const GLFWvidmode* m = glfwGetVideoMode(monitors[i]);
                if (cx >= mx && cx < mx + m->width && cy >= my && cy < my + m->height) monitor = monitors[i];
            }
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }
        else
        {
            glfwSetWindowMonitor(window, nullptr, windowed[0], windowed[1], windowed[2], windowed[3], 0);
        }
    });
    if (!interactive) return;
    auto keys = EventListenerKeyboard::create();
    keys->onKeyPressed = [](EventKeyboard::KeyCode key, Event*) {
        if (key == EventKeyboard::KeyCode::KEY_F11) qol::setFullscreen(!qol::fullscreen());
    };
    Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(keys, 2);
    static int s_applyTarget = 0;
    Director::getInstance()->getScheduler()->schedule(
        [](float) {
            qol::applyDisplaySettings();
            if (qol::fullscreen()) qol::setFullscreen(true);
        },
        &s_applyTarget, 0.0f, 0, 0.5f, false, "ow_apply_display");
}

// Runs `action` once the main menu is the running scene (after the splash / consent screens),
// so command-line level launches push their scene onto the menu like a player's tap would.
void runOnMainMenu(std::function<void()> action)
{
    static int s_target = 0;
    static int s_key = 0;
    const std::string key = "ow_on_main_menu_" + std::to_string(++s_key);
    Director::getInstance()->getScheduler()->schedule(
        [action, key](float) {
            Scene* scene = Director::getInstance()->getRunningScene();
            if (!scene) return;
            for (Node* child : scene->getChildren())
            {
                if (dynamic_cast<MainMenu*>(child))
                {
                    Director::getInstance()->getScheduler()->unschedule(key, &s_target);
                    action();
                    return;
                }
            }
        },
        &s_target, 0.25f, CC_REPEAT_FOREVER, 0.5f, false, key);
}

int WINAPI _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    openwheels::pc::installCrashHandler();
    openwheels::pc::installHangWatchdog();
    {
        static int s_heartbeatTarget = 0;
        Director::getInstance()->getScheduler()->schedule([](float) { openwheels::pc::heartbeat(); },
                                                          &s_heartbeatTarget, 0.0f, false, "ow_heartbeat");
    }
    const Options opt = parseOptions();
    setupLogging(opt.console);

    // PC-only test hook (not in the original): convert a browser level with
    // online::FlashLevelConverter, write <out> and <out>.report.txt, exit (0 = converted).
    if (!opt.convertIn.empty())
        return convertFlashLevel(opt.convertIn, opt.convertOut);

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
    installFullscreen(glview, opt.dumpWorld.empty());
    // The original's first-run "accept the Privacy Policy" prompt is Fancy Force's policy for its
    // ad/analytics SDKs, which OpenWheels doesn't have: pre-accept it (as on Android).
    UserDefault::getInstance()->setBoolForKey("terms_of_use_accepted", true);

#ifdef OW_WITH_PC_LAYER  // enable once src/game links (PCInput.cpp, WorldDumpRunner.cpp)
    openwheels::pc::installKeyboardControls();
    if (!opt.dumpWorld.empty())
    {
        return openwheels::pc::runWorldDump(narrow(opt.dumpWorld), opt.level, opt.frames, opt.script,
                                             opt.dumpAt);
    }
#endif

    if (!opt.openFile.empty())
    {
        // Like iOS "Open in": hand the file to the level store once the game is up and running.
        const std::string path = narrow(opt.openFile);
        runOnMainMenu([path]() { LevelSession::getInstance()->openHappyWheelsFile(path); });
    }

    if (!opt.playLevel.empty())
    {
        const std::string xml = FileUtils::getInstance()->getStringFromFile(narrow(opt.playLevel));
        const int selectCharacter = opt.selectCharacter;
        runOnMainMenu([xml, selectCharacter]() {
            LevelSession* session = LevelSession::getInstance();
            session->clearLevelData();
            session->setChapterIndex(5001);
            session->setLevelDataXML(xml);
            session->setForceCharacter(selectCharacter == 0);
            session->setCharacterIndex(selectCharacter ? selectCharacter : 1);
            session->playLevel(selectCharacter == 0);
        });
    }

    if (opt.playOnline > 0)
    {
        const int levelId = opt.playOnline;
        runOnMainMenu([levelId]() { online::playOnlineLevelById(levelId); });
    }

    return Application::getInstance()->run();
}
