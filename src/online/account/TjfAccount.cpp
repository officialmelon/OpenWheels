// ONLINE (PC addition): see TjfAccount.h.
#include "online/account/TjfAccount.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

#include "cocos2d.h"
#include "online/HWApi.h"
#include "tinyxml2/tinyxml2.h"

USING_NS_CC;

namespace online {
namespace account {

const char* const kAccountChangedEvent = "tjf_account_changed";

namespace {

std::string sessionFile() { return storageDir() + "tjf_session.txt"; }

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

// A cookie name: no spaces, separators or '='.
bool cookieName(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s)
        if (c <= ' ' || c == ';' || c == ',' || c == '=' || c == '"') return false;
    return true;
}

// Set-Cookie values of a raw header block. curl delivers one "Set-Cookie:" line per cookie;
// Android's HttpURLConnection joins them with ',' (and Expires dates contain commas).
std::vector<std::string> setCookieValues(const std::string& headers) {
    std::vector<std::string> out;
    std::istringstream in(headers);
    std::string line;
    while (std::getline(in, line)) {
        const size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        if (lower(trim(line.substr(0, colon))) != "set-cookie") continue;
        const std::string value = trim(line.substr(colon + 1));
        size_t start = 0;
        for (size_t i = 0; i <= value.size(); ++i) {
            if (i < value.size() && value[i] != ',') continue;
            // Split here only when a new "name=" follows.
            bool split = i == value.size();
            if (!split) {
                const std::string rest = value.substr(i + 1);
                const size_t eq = rest.find('=');
                split = eq != std::string::npos && cookieName(trim(rest.substr(0, eq)));
            }
            if (split) {
                out.push_back(trim(value.substr(start, i - start)));
                start = i + 1;
            }
        }
    }
    return out;
}

std::string form(std::initializer_list<std::pair<const char*, std::string>> fields) {
    std::string body;
    for (const auto& f : fields) {
        if (!body.empty()) body += '&';
        body += f.first;
        body += '=';
        body += HWApi::urlEncode(f.second);
    }
    return body;
}

std::string stripTags(const std::string& s) {
    std::string out;
    bool tag = false;
    for (char c : s) {
        if (c == '<') tag = true;
        else if (c == '>') tag = false;
        else if (!tag) out += c;
    }
    return trim(out);
}

}  // namespace

std::string storageDir() {
    const bool real = HWApi::siteUrl() == "https://totaljerkface.com/";
    return FileUtils::getInstance()->getWritablePath() + (real ? "online/" : "online/mock/");
}

TjfAccount* TjfAccount::get() {
    static TjfAccount* instance = nullptr;
    if (!instance) {
        instance = new TjfAccount();
        HWApi* api = HWApi::getInstance();
        api->setCookieProvider([]() { return TjfAccount::get()->cookieHeader(); });
        api->setHeaderObserver([](const std::string& headers) { TjfAccount::get()->absorbHeaders(headers); });
    }
    return instance;
}

TjfAccount::TjfAccount() { load(); }

std::string TjfAccount::displayName() const {
    if (!_userName.empty()) return _userName;
    if (_userId > 0) return "user #" + std::to_string(_userId);
    return "you";
}

void TjfAccount::load() {
    const std::string text = FileUtils::getInstance()->getStringFromFile(sessionFile());
    if (text.empty()) return;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::vector<std::string> f;
        std::string part;
        std::istringstream ls(line);
        while (std::getline(ls, part, '\t')) f.push_back(part);
        if (f.size() >= 3 && f[0] == "cookie") _cookies[f[1]] = f[2];
        else if (f.size() >= 2 && f[0] == "user_id") _userId = std::atoi(f[1].c_str());
        else if (f.size() >= 2 && f[0] == "user_name") _userName = f[1];
        else if (f.size() >= 2 && f[0] == "email") _email = f[1];
        else if (f.size() >= 2 && f[0] == "logged_in") _loggedIn = f[1] == "1";
    }
    _remember = true;
    if (_cookies.empty()) _loggedIn = false;
}

void TjfAccount::save() const {
    FileUtils* fu = FileUtils::getInstance();
    if (!_remember) {
        // Only the email for the form; no session.
        if (_email.empty()) {
            fu->removeFile(sessionFile());
            return;
        }
        fu->createDirectory(storageDir());
        fu->writeStringToFile("email\t" + _email + "\n", sessionFile());
        return;
    }
    std::string out = "# OpenWheels: totaljerkface.com session (cookies only, never the password)\n";
    out += std::string("logged_in\t") + (_loggedIn ? "1" : "0") + "\n";
    if (_userId > 0) out += "user_id\t" + std::to_string(_userId) + "\n";
    if (!_userName.empty()) out += "user_name\t" + _userName + "\n";
    if (!_email.empty()) out += "email\t" + _email + "\n";
    for (const auto& c : _cookies) out += "cookie\t" + c.first + "\t" + c.second + "\n";
    fu->createDirectory(storageDir());
    fu->writeStringToFile(out, sessionFile());
}

void TjfAccount::clearSession() {
    _loggedIn = false;
    _userId = 0;
    _userName.clear();
    _cookies.clear();
    save();
}

void TjfAccount::notify() const {
    Director::getInstance()->getEventDispatcher()->dispatchCustomEvent(kAccountChangedEvent);
}

std::string TjfAccount::cookieHeader() const {
    std::string out;
    for (const auto& c : _cookies) {
        if (!out.empty()) out += "; ";
        out += c.first + "=" + c.second;
    }
    return out;
}

void TjfAccount::absorbHeaders(const std::string& headers) {
    bool changed = false;
    for (const std::string& sc : setCookieValues(headers)) {
        const size_t semi = sc.find(';');
        const std::string pair = sc.substr(0, semi);
        const size_t eq = pair.find('=');
        if (eq == std::string::npos) continue;
        const std::string name = trim(pair.substr(0, eq));
        const std::string value = trim(pair.substr(eq + 1));
        if (!cookieName(name)) continue;
        const std::string attrs = lower(semi == std::string::npos ? std::string() : sc.substr(semi));
        const bool remove = value.empty() || attrs.find("max-age=0") != std::string::npos ||
                            attrs.find("expires=thu, 01 jan 1970") != std::string::npos;
        if (remove) {
            changed |= _cookies.erase(name) > 0;
        } else if (_cookies[name] != value) {
            _cookies[name] = value;
            changed = true;
        }
    }
    if (changed && _loggedIn && _remember) save();
}

bool TjfAccount::parseUserFromPage(const std::string& html, int* userId, std::string* userName) {
    int id = 0;
    std::string name;
    std::smatch m;
    // HW_SETTINGS (HTML5 page) / flashvars (Flash page): userID: "123", userName: "..."
    static const std::regex idRe("[\"']?(?:userID|userId|user_id|userid|uid)[\"']?\\s*[:=]\\s*[\"']?(\\d+)");
    static const std::regex nameRe("[\"']?(?:userName|username|user_name)[\"']?\\s*[:=]\\s*[\"']([^\"']+)[\"']");
    const size_t settings = html.find("HW_SETTINGS");
    const size_t flashvars = html.find("flashvars");
    for (size_t at : {settings, flashvars}) {
        if (at == std::string::npos || id) continue;
        const std::string region = html.substr(at, 4000);
        if (std::regex_search(region, m, idRe)) id = std::atoi(m[1].str().c_str());
        if (std::regex_search(region, m, nameRe)) name = m[1].str();
    }
    // The site header (<div id="login">) links the logged-in player's profile.
    if (!id) {
        const size_t login = html.find("id=\"login\"");
        if (login != std::string::npos) {
            const std::string region = html.substr(login, 3000);
            static const std::regex profileRe("profile\\.tjf\\?uid=(\\d+)[^>]*>([\\s\\S]*?)</a>");
            if (std::regex_search(region, m, profileRe)) {
                id = std::atoi(m[1].str().c_str());
                if (name.empty()) name = stripTags(m[2].str());
            }
        }
    }
    if (id <= 0) return false;
    if (userId) *userId = id;
    if (userName && !name.empty()) *userName = name;
    return true;
}

void TjfAccount::login(const std::string& email, const std::string& password, bool remember, Done done) {
    if (_busy) {
        if (done) done(false, "Already logging in...");
        return;
    }
    _busy = true;
    _remember = remember;
    _email = trim(email);
    _cookies.clear();
    _loggedIn = false;
    _userId = 0;
    _userName.clear();
    HWApi* api = HWApi::getInstance();
    // 1. The login page, like a browser: starts the servlet session (JSESSIONID).
    api->request("user_login.tjf", std::string(), true, [](const HWApi::RawResponse&) {});
    // 2. The site's login form (js/login-*.min.js postUserLogin).
    std::string body = form({{"login_user_email", _email}, {"login_user_pass", password}, {"action", "login"}});
    api->request("user.hw", body, false, [this, done](const HWApi::RawResponse& r) {
        if (!r.ok) {
            _busy = false;
            if (done) done(false, "Couldn't reach totaljerkface.com. Check your internet connection.");
            return;
        }
        const std::string reply = trim(r.body);
        const size_t colon = reply.find(':');
        const std::string head = reply.substr(0, colon);
        const std::string arg = colon == std::string::npos ? std::string() : reply.substr(colon + 1);
        if (head == "success" && arg == "true") {
            _loggedIn = true;
            identify(done, 0);
            return;
        }
        _busy = false;
        _cookies.clear();
        std::string message;
        if (head == "failure" && arg == "userpass") {
            message = "Login failed. No matching email and password.";
        } else if (head == "failure" && arg == "verify_email") {
            message = "totaljerkface.com emailed you a link to confirm it's you. Open it and follow the "
                      "instructions, then log in again here.";
        } else if (head == "lockout") {
            const int minutes = std::max(1, std::atoi(arg.c_str()));
            message = "Too many failed logins. Please wait " + std::to_string(minutes) +
                      (minutes > 1 ? " minutes." : " minute.");
        } else {
            message = "Login failed. System error, please try again later.";
        }
        if (done) done(false, message);
    });
    std::fill(body.begin(), body.end(), '\0');
}

// Who is logged in: the HTML5 game page (HW_SETTINGS), the site page (header profile link),
// then the player's own levels (get_cmb_by_user, every <lv ui> is theirs).
void TjfAccount::identify(Done done, int step) {
    HWApi* api = HWApi::getInstance();
    if (step == 0 || step == 1) {
        const char* page = step == 0 ? "happy-wheels-js/index.tjf" : "happy_wheels.tjf";
        api->request(page, std::string(), true, [this, done, step](const HWApi::RawResponse& r) {
            int id = 0;
            std::string name;
            if (r.ok && parseUserFromPage(r.body, &id, &name)) {
                _userId = id;
                if (!name.empty()) _userName = name;
                finishLogin(done, std::string());
                return;
            }
            identify(done, step + 1);
        });
        return;
    }
    if (step == 2) {
        api->request("get_level.hw", form({{"action", "get_cmb_by_user"}}), false,
                     [this, done](const HWApi::RawResponse& r) {
                         tinyxml2::XMLDocument doc;
                         if (r.ok && !HWApi::isServerError(r.body, nullptr) &&
                             doc.Parse(r.body.c_str()) == tinyxml2::XML_SUCCESS && doc.RootElement()) {
                             // <...><private><lvs><lv ui un/>...</lvs></private><published>...</published>
                             std::vector<tinyxml2::XMLElement*> stack{doc.RootElement()};
                             while (!stack.empty() && !_userId) {
                                 tinyxml2::XMLElement* e = stack.back();
                                 stack.pop_back();
                                 if (std::string(e->Name()) == "lv") {
                                     _userId = e->IntAttribute("ui");
                                     if (const char* un = e->Attribute("un")) _userName = un;
                                 }
                                 for (auto* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                                     stack.push_back(c);
                             }
                         }
                         finishLogin(done, _userId ? std::string()
                                                   : "Logged in, but your user id couldn't be detected. "
                                                     "Enter it below (the number in your profile link).");
                     });
        return;
    }
    finishLogin(done, std::string());
}

void TjfAccount::finishLogin(Done done, const std::string& note) {
    _busy = false;
    save();
    notify();
    if (done) done(true, note);
}

void TjfAccount::setUserId(int userId) {
    if (userId <= 0) return;
    _userId = userId;
    save();
    notify();
}

void TjfAccount::logout(Done done) {
    // The site's logOutUser(): POST user.hw action=logout. The local session goes either way.
    HWApi::getInstance()->request("user.hw", form({{"action", "logout"}}), false,
                                  [done](const HWApi::RawResponse& r) {
                                      if (done) done(r.ok, r.ok ? std::string() : r.error);
                                  });
    clearSession();
    notify();
}

void TjfAccount::sessionExpired() {
    if (!_loggedIn) return;
    clearSession();
    notify();
}

}  // namespace account
}  // namespace online
