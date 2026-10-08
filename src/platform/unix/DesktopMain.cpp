// PC platform glue for Linux and macOS, see DesktopMain.h. Mirrors src/platform/win32/main.cpp
// option for option; only the OS calls differ (paths, logging, message box).

#include "platform/unix/DesktopMain.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include "cocos2d.h"
#include "AppDelegate.h"
#include "platform/common/IOSBundle.h"
#include "platform/common/Localization.h"
#include "LevelSession.h"
#include "editor/flash/FlashEditorHooks.h"  // EDITOR (browser features, PC addition)
#include "qol/QoL.h"
#include "MainMenu.h"
#include "online/OnlinePlay.h"
#include "online/account/TjfTestDriver.h"  // ONLINE (PC addition)
#include "online/FlashLevelConverter.h"
#include "net/LevelTransfer.h"        // NET (PC addition)
#include "net/race/RaceSession.h"     // NET (PC addition)
#include "platform/desktop/DesktopWindow.h"
#include "platform/unix/CrashHandler.h"

#ifdef OW_WITH_PC_LAYER
#include "platform/desktop/PCInput.h"
#include "platform/desktop/WorldDumpRunner.h"
#endif

USING_NS_CC;

namespace openwheels {
namespace desktop {
namespace {

std::string exeDirectory()
{
    std::string path;
#if defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size + 1, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) == 0)
    {
        char resolved[PATH_MAX];
        path = realpath(buffer.data(), resolved) ? resolved : buffer.data();
    }
#else
    char buffer[4096];
    const ssize_t n = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (n > 0) path.assign(buffer, (size_t)n);
#endif
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? std::string("./") : path.substr(0, slash + 1);
}

bool isDirectory(const std::string& p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool isFile(const std::string& p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string withSlash(std::string p)
{
    if (!p.empty() && p.back() != '/') p += '/';
    return p;
}

std::string parentOf(const std::string& dir)
{
    if (dir.size() < 2) return std::string();
    const size_t slash = dir.find_last_of('/', dir.size() - 2);
    return slash == std::string::npos ? std::string() : dir.substr(0, slash + 1);
}

// A folder looks like the game's assets if it has the original's search-path roots.
bool looksLikeAssets(const std::string& dir)
{
    return isDirectory(dir + "shared/levels") && isDirectory(dir + "sounds");
}

// --assets wins; then <exe>/assets (macOS: also the bundle's Resources/assets and an assets folder
// next to the .app); then ~/.local/share/OpenWheels/assets (Linux) or
// ~/Library/Application Support/OpenWheels/assets (macOS); then the repo's binary/ reference
// folder found by walking up.
std::string findAssets(const std::string& fromArgs)
{
    if (!fromArgs.empty()) return withSlash(fromArgs);
    const std::string exe = exeDirectory();
    std::vector<std::string> candidates = {exe + "assets/"};
#if defined(__APPLE__)
    candidates.push_back(exe + "../Resources/assets/");
    candidates.push_back(exe + "../../../assets/");  // OpenWheels.app/Contents/MacOS -> next to the .app
#endif
    if (const char* home = getenv("HOME"))
    {
#if defined(__APPLE__)
        candidates.push_back(std::string(home) + "/Library/Application Support/OpenWheels/assets/");
#else
        const char* xdg = getenv("XDG_DATA_HOME");
        candidates.push_back((xdg && *xdg ? std::string(xdg) : std::string(home) + "/.local/share") +
                             "/OpenWheels/assets/");
#endif
    }
    for (const std::string& c : candidates)
        if (looksLikeAssets(c)) return c;
    std::string dir = exe;
    for (int up = 0; up < 8 && !dir.empty(); ++up)
    {
        const std::string candidate = dir + "binary/HappyWheels_Android/HW_Android/assets/";
        if (looksLikeAssets(candidate)) return candidate;
        dir = parentOf(dir);
    }
    return std::string();
}

// Optional iOS app bundle (happywheels.app): level-editor art + Localizable.strings.
std::string findIOSBundle(const std::string& fromArgs)
{
    if (!fromArgs.empty()) return withSlash(fromArgs);
    std::string dir = exeDirectory();
    if (isFile(dir + "ios/Localizable.strings")) return dir + "ios/";
    for (int up = 0; up < 8 && !dir.empty(); ++up)
    {
        const std::string candidate = dir + "binary/HappyWheels_iOS/Payload/happywheels.app/";
        if (isFile(candidate + "Localizable.strings")) return candidate;
        dir = parentOf(dir);
    }
    return std::string();
}

struct Options
{
    std::string assets;
    std::string iosApp;
    int playOnline = 0;
    std::string onlineTest;
    std::string playLevel;
    int selectCharacter = 0;
    std::string convertIn, convertOut;
    std::string openFile;
    std::string editFile;
    float width = 1600.0f;
    float height = 900.0f;
    bool explicitSize = false;  // --width / --height given: keep that window size (else maximized)
    bool console = false;
    std::string dumpWorld;
    std::string level = "levels/01_business_guy/01_business_guy_tutorial_level.xml";
    int frames = 120;
    std::string script = "0:00";
    std::string dumpAt;
    std::string raceTest;
    int racePlayers = 2;
    int raceCharacter = 0;
    std::string playerName;
};

Options parseOptions(int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? std::string(argv[++i]) : std::string(); };
        if (a == "--assets") o.assets = next();
        else if (a == "--ios-app") o.iosApp = next();
        else if (a == "--open") o.openFile = next();
        else if (a == "--edit") o.editFile = next();
        else if (a == "--play-online") o.playOnline = atoi(next().c_str());
        else if (a == "--online-test") o.onlineTest = next();
        else if (a == "--play-level") o.playLevel = next();
        else if (a == "--select-character") o.selectCharacter = atoi(next().c_str());
        else if (a == "--convert-flash") { o.convertIn = next(); o.convertOut = next(); }
        else if (a.compare(0, 4, "-psn") == 0) {}   // macOS Finder process serial number
        else if (a.size() > 4 && a[0] != '-') o.openFile = a;   // file passed by the desktop / Finder
        else if (a == "--width") { o.width = (float)atof(next().c_str()); o.explicitSize = true; }
        else if (a == "--height") { o.height = (float)atof(next().c_str()); o.explicitSize = true; }
        else if (a == "--console") o.console = true;
        else if (a == "--dump-world") o.dumpWorld = next();
        else if (a == "--level") o.level = next();
        else if (a == "--frames") o.frames = atoi(next().c_str());
        else if (a == "--script") o.script = next();
        else if (a == "--dump-at") o.dumpAt = next();
        else if (a == "--race-test") o.raceTest = next();
        else if (a == "--race-players") o.racePlayers = atoi(next().c_str());
        else if (a == "--race-char") o.raceCharacter = atoi(next().c_str());
        else if (a == "--player-name") o.playerName = next();
    }
    if (o.width < 320.0f) o.width = 320.0f;
    if (o.height < 240.0f) o.height = 240.0f;
    return o;
}

// The log goes next to the executable when that folder is writable (portable / build folder),
// else into the per-user data folder. --console keeps the terminal's stdout.
std::string logDirectory()
{
    const std::string exe = exeDirectory();
    if (access(exe.c_str(), W_OK) == 0) return exe;
    return FileUtils::getInstance()->getWritablePath();
}

void setupLogging(bool console)
{
    if (console) return;
    const std::string logPath = logDirectory() + "openwheels.log";
    if (std::freopen(logPath.c_str(), "w", stdout)) setvbuf(stdout, nullptr, _IONBF, 0);
    if (std::freopen(logPath.c_str(), "a", stderr)) setvbuf(stderr, nullptr, _IONBF, 0);
}

int convertFlashLevel(const std::string& in, const std::string& out)
{
    std::string xml;
    if (FILE* f = std::fopen(in.c_str(), "rb"))
    {
        char buffer[65536];
        size_t n;
        while ((n = std::fread(buffer, 1, sizeof(buffer), f)) > 0) xml.append(buffer, n);
        std::fclose(f);
    }
    online::ConversionReport report;
    const std::string mobile = online::FlashLevelConverter::toMobile(xml, &report);
    std::string text = report.ok ? "ok\n" : "error: " + report.error + "\n";
    text += "character " + std::to_string(report.character) + (report.forceCharacter ? " forced" : "") +
            "\ndropped " + std::to_string(report.droppedItems) + " substituted " +
            std::to_string(report.substitutedItems) + (report.hasUserVehicle ? " vehicle" : "") + "\n";
    for (const std::string& w : report.warnings) text += "warning: " + w + "\n";
    if (FILE* f = std::fopen((out + ".report.txt").c_str(), "wb"))
    {
        std::fwrite(text.data(), 1, text.size(), f);
        std::fclose(f);
    }
    if (!report.ok) return 1;
    FILE* f = std::fopen(out.c_str(), "wb");
    if (!f) return 2;
    std::fwrite(mobile.data(), 1, mobile.size(), f);
    std::fclose(f);
    return 0;
}

// Runs `action` once the main menu is the running scene (after the splash / consent screens).
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

}  // namespace

int run(int argc, char** argv)
{
    const Options opt = parseOptions(argc, argv);
    setupLogging(opt.console);
    openwheels::pc::installCrashHandler(logDirectory());

    if (!opt.convertIn.empty())
        return convertFlashLevel(opt.convertIn, opt.convertOut);

    const std::string assets = findAssets(opt.assets);
    if (assets.empty())
    {
        const char* message =
            "OpenWheels needs the game files from your own copy of Happy Wheels (Android).\n\n"
            "Extract the APK's assets folder and start OpenWheels with\n"
            "    --assets <path-to-assets>\n"
            "or place it next to the OpenWheels executable as an 'assets' folder.";
        std::fprintf(stderr, "%s\n", message);
        MessageBox(message, "OpenWheels - game files not found");
        return 1;
    }

    AppDelegate app;

    auto fileUtils = FileUtils::getInstance();
    fileUtils->setDefaultResourceRootPath(assets);
    fileUtils->addSearchPath(exeDirectory(), false);
#if defined(__APPLE__)
    // App bundle: build-generated tables and art may also sit in Contents/Resources.
    fileUtils->addSearchPath(exeDirectory() + "../Resources/", false);
#endif
    cocos2d::log("OpenWheels: assets = %s", assets.c_str());
    const std::string iosApp = findIOSBundle(opt.iosApp);
    if (!iosApp.empty())
    {
        openwheels::setIOSBundlePath(iosApp);
        cocos2d::log("OpenWheels: iOS bundle = %s (%d localized strings)", iosApp.c_str(),
                     (int)Localization::size());
    }

    Application::getInstance()->initGLContextAttrs();
    // Resizable window, maximized unless --width/--height (or --dump-world) ask for a fixed size;
    // fullscreen from the QOL page / F11 (platform/desktop/DesktopWindow.cpp).
    const bool verification = !opt.dumpWorld.empty();
    auto glview = GLViewImpl::createWithRect("OpenWheels", Rect(0.0f, 0.0f, opt.width, opt.height), 1.0f,
                                             !verification);
    Director::getInstance()->setOpenGLView(glview);
    openwheels::desktop::installWindowManagement(glview, !opt.explicitSize && !verification, !verification);
    UserDefault::getInstance()->setBoolForKey("terms_of_use_accepted", true);

#ifdef OW_WITH_PC_LAYER
    openwheels::pc::installKeyboardControls();
    if (!opt.dumpWorld.empty())
        return openwheels::pc::runWorldDump(opt.dumpWorld, opt.level, opt.frames, opt.script, opt.dumpAt);
#endif

    if (!opt.openFile.empty())
    {
        const std::string path = opt.openFile;
        runOnMainMenu([path]() { LevelSession::getInstance()->openHappyWheelsFile(path); });
    }

    if (!opt.editFile.empty())
    {
        const std::string xml = FileUtils::getInstance()->getStringFromFile(opt.editFile);
        std::string name = opt.editFile.substr(opt.editFile.find_last_of('/') + 1);
        name = name.substr(0, name.find_last_of('.'));
        runOnMainMenu([xml, name]() { flashed::openLevelInEditor(xml, name); });
    }

    if (!opt.playLevel.empty())
    {
        const std::string xml = FileUtils::getInstance()->getStringFromFile(opt.playLevel);
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

    if (!opt.onlineTest.empty())
    {
        const std::string scenario = opt.onlineTest;
        runOnMainMenu([scenario]() { online::runTjfTestScenario(scenario); });
    }

    if (!opt.playerName.empty() || !opt.raceTest.empty())
    {
        const std::string name = opt.playerName, spec = opt.raceTest;
        const int players = opt.racePlayers, character = opt.raceCharacter;
        runOnMainMenu([name, spec, players, character]() {
            if (!name.empty()) net::LevelTransfer::getInstance()->setPlayerName(name);
            if (!spec.empty()) race::RaceSession::get()->setAutoTest(spec, players, character);
        });
    }

    if (opt.playOnline > 0)
    {
        const int levelId = opt.playOnline;
        runOnMainMenu([levelId]() { online::playOnlineLevelById(levelId); });
    }

    return Application::getInstance()->run();
}

}  // namespace desktop
}  // namespace openwheels
