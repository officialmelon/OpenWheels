#include "qol/UpdateCheck.h"

#include <cstdlib>

#include "cocos2d.h"
#include "network/HttpClient.h"

#include "HWWindow.h"
#include "HWWindowDelegate.h"
#include "MainMenu.h"
#include "Settings.h"
#include "qol/AppVersion.h"

USING_NS_CC;

namespace qol {

namespace {

const char* const kLatestUrl = "https://api.github.com/repos/officialmelon/OpenWheels/releases/latest";
const char* const kDisabledKey = "qol_update_check_disabled";
const char* const kDismissedKey = "qol_update_dismissed_tag";

bool g_started = false;
bool g_shown = false;
std::string g_tag;   // the newer release found, "" while none
std::string g_page;  // its page on GitHub

// Pulls the string value of `"key":"..."` out of the compact JSON GitHub returns (the first match
// is the release's own field; "tag_name" / "html_url" appear before any nested object's).
std::string jsonString(const std::string& body, const std::string& key)
{
    const std::string needle = "\"" + key + "\"";
    size_t at = body.find(needle);
    if (at == std::string::npos) return std::string();
    at = body.find(':', at + needle.size());
    if (at == std::string::npos) return std::string();
    at = body.find('"', at + 1);
    if (at == std::string::npos) return std::string();
    std::string out;
    for (size_t i = at + 1; i < body.size() && body[i] != '"'; ++i)
    {
        if (body[i] == '\\' && i + 1 < body.size()) ++i;
        out += body[i];
    }
    return out;
}

std::vector<int> versionParts(const std::string& version)
{
    std::vector<int> parts;
    size_t i = 0;
    if (i < version.size() && (version[i] == 'v' || version[i] == 'V')) ++i;
    int value = 0;
    bool any = false;
    for (; i < version.size(); ++i)
    {
        const char c = version[i];
        if (c >= '0' && c <= '9')
        {
            value = value * 10 + (c - '0');
            any = true;
        }
        else if (c == '.')
        {
            parts.push_back(value);
            value = 0;
            any = false;
        }
        else
        {
            break;  // "-beta.3": only the numeric core counts
        }
    }
    if (any) parts.push_back(value);
    return parts;
}

class Prompt : public HWWindowDelegate
{
public:
    void hwWindowButtonPressed(int buttonTag, HWWindow*) override
    {
        if (buttonTag == 1)
        {
            Application::getInstance()->openURL(g_page);
        }
        else
        {
            UserDefault::getInstance()->setStringForKey(kDismissedKey, g_tag);
            UserDefault::getInstance()->flush();
        }
    }
};

Prompt g_prompt;

// The main menu is the scene on screen (the popup must not appear over a level).
bool mainMenuRunning()
{
    Scene* scene = Director::getInstance()->getRunningScene();
    if (!scene) return false;
    for (Node* child : scene->getChildren())
    {
        if (dynamic_cast<MainMenu*>(child)) return true;
    }
    return false;
}

}  // namespace

const char* appVersion() { return OW_APP_VERSION; }

bool updateCheckEnabled() { return !UserDefault::getInstance()->getBoolForKey(kDisabledKey, false); }

void setUpdateCheckEnabled(bool on)
{
    UserDefault::getInstance()->setBoolForKey(kDisabledKey, !on);
    UserDefault::getInstance()->flush();
}

bool isNewerVersion(const std::string& latest, const std::string& current)
{
    const std::vector<int> a = versionParts(latest), b = versionParts(current);
    if (a.empty() || b.empty()) return false;
    for (size_t i = 0; i < std::max(a.size(), b.size()); ++i)
    {
        const int x = i < a.size() ? a[i] : 0, y = i < b.size() ? b[i] : 0;
        if (x != y) return x > y;
    }
    return false;
}

void checkForUpdates()
{
    if (g_started || !updateCheckEnabled())
    {
        return;
    }
    g_started = true;
    auto* request = new (std::nothrow) network::HttpRequest();
    if (!request)
    {
        return;
    }
    // OW_UPDATE_URL: test hook, points the check at a local mock of the GitHub API.
    const char* override = std::getenv("OW_UPDATE_URL");
    request->setUrl(override && *override ? override : kLatestUrl);
    request->setRequestType(network::HttpRequest::Type::GET);
    request->setHeaders({std::string("User-Agent: OpenWheels/") + OW_APP_VERSION + " (+https://github.com/officialmelon/OpenWheels)",
                         "Accept: application/vnd.github+json"});
    request->setResponseCallback([](network::HttpClient*, network::HttpResponse* response) {
        if (!response || !response->isSucceed() || !response->getResponseData())
        {
            return;  // offline, rate limited, ...: try again next launch
        }
        const std::vector<char>* data = response->getResponseData();
        const std::string body(data->begin(), data->end());
        const std::string tag = jsonString(body, "tag_name");
        const std::string page = jsonString(body, "html_url");
        if (tag.empty() || (page.compare(0, 8, "https://") != 0 && !std::getenv("OW_UPDATE_URL")) || !isNewerVersion(tag, OW_APP_VERSION))
        {
            return;
        }
        g_tag = tag;
        g_page = page;
        if (mainMenuRunning())
        {
            showPendingUpdatePrompt();  // the answer came in while the menu is up
        }
    });
    network::HttpClient::getInstance()->setTimeoutForConnect(10);
    network::HttpClient::getInstance()->setTimeoutForRead(15);
    network::HttpClient::getInstance()->send(request);
    request->release();
}

void showPendingUpdatePrompt()
{
    if (g_shown || g_tag.empty() || !updateCheckEnabled())
    {
        return;
    }
    if (UserDefault::getInstance()->getStringForKey(kDismissedKey, "") == g_tag)
    {
        return;
    }
    g_shown = true;
    HWWindow* window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, &g_prompt, true, true);
    window->showAlertMessage("Update available",
                             "OpenWheels " + g_tag + " is out (you have v" + OW_APP_VERSION +
                                 ").\nOpen the download page?",
                             "Download", "Later", true);
}

}  // namespace qol
