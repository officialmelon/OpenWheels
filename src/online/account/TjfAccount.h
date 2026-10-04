#pragma once
// ONLINE (PC addition): the player's totaljerkface.com account (login session) for the online
// level browser. Not part of the 1:1 reconstruction. See docs/FLASH_LEVELS.md section 11.
//
// How the browser game authenticates (derived from the decompiled Flash v1.87 client and the
// site's own login page, js/login-*.min.js):
//   * The game never logs in itself. The player logs in on the web site; the site's Java backend
//     keeps the login in the servlet session (cookie JSESSIONID, HttpOnly). The game's requests
//     (user.hw, set_level.hw, replay.hw...) are plain form POSTs that the browser sends with that
//     cookie; the server answers "failure:not_logged_in" without it.
//   * Login = POST /user.hw  login_user_email, login_user_pass, action=login
//     -> "success:true" | "failure:userpass" | "failure:verify_email" | "lockout:<minutes>".
//     Logout = POST /user.hw action=logout.
//   * The page then hands the game the user id / name (Flash flashvars userID / userName; the
//     HTML5 page's HW_SETTINGS). The id matters: published levels are encrypted with
//     "eatshit" + <author id>. (The Flash preloader's `session` flashvar is NOT a login session:
//     it is the AES key that decrypts the game SWF's Blowfish key.)
//
// What is stored: only the site's cookies (the session token), the user id / name and the email
// (to pre-fill the form), in <writable>/online/tjf_session.txt, and only with "Remember me".
// The password is never stored or logged; it lives in the login request body only.

#include <functional>
#include <map>
#include <string>

namespace online {
namespace account {

// <writable>/online/ for the real site; <writable>/online/mock/ while OW_TJF_BASE points elsewhere,
// so a test session, test runs and cached test replays never mix with the player's own.
std::string storageDir();

// Custom event dispatched (Director's dispatcher) whenever the login state changes.
extern const char* const kAccountChangedEvent;  // "tjf_account_changed"

class TjfAccount {
public:
    static TjfAccount* get();

    bool loggedIn() const { return _loggedIn; }
    int userId() const { return _userId; }              // 0 = logged in but unknown (see setUserId)
    const std::string& userName() const { return _userName; }
    const std::string& email() const { return _email; } // last login email (form pre-fill)
    bool remember() const { return _remember; }
    // "Name" or "user #123" for the UI.
    std::string displayName() const;

    using Done = std::function<void(bool ok, const std::string& message)>;
    // Logs in with the player's own credentials (typed into LoginPanel). message: the site's
    // error text on failure; on success a note when the user id could not be detected.
    void login(const std::string& email, const std::string& password, bool remember, Done done);
    void logout(Done done);
    // The player's id when detection failed (from their profile link profile.tjf?uid=N).
    void setUserId(int userId);

    // A request answered "failure:not_logged_in": the session expired. Clears it (keeps email).
    void sessionExpired();

    // Cookie jar (HWApi hooks).
    std::string cookieHeader() const;
    void absorbHeaders(const std::string& headers);

    // Parsers (exposed for tests): user id / name from the site's pages.
    static bool parseUserFromPage(const std::string& html, int* userId, std::string* userName);

private:
    TjfAccount();
    void load();
    void save() const;
    void clearSession();
    void notify() const;
    void identify(Done done, int step);
    void finishLogin(Done done, const std::string& note);

    bool _loggedIn = false;
    int _userId = 0;
    std::string _userName;
    std::string _email;
    bool _remember = true;
    bool _busy = false;
    std::map<std::string, std::string> _cookies;
};

}  // namespace account
}  // namespace online
