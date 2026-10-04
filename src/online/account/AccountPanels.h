#pragma once
// ONLINE (PC addition): login / account / rating panels of the online level browser.

#include <functional>
#include <string>

#include "online/account/TjfUi.h"

namespace online {
namespace ui {
class Button;
}
namespace account {

// Runs `after` when logged in with a known user id; otherwise opens the login panel (which runs
// it after a successful login) and returns false.
bool ensureLoggedIn(const std::string& why, std::function<void()> after);

// "Log In": the player's own totaljerkface.com email and password. The password goes into the
// login request only (TjfAccount); the field is wiped when the panel closes.
class LoginPanel : public tjfui::Panel {
public:
    // why: one line on top ("Log in to keep favorites."); onLoggedIn runs after a login.
    static LoginPanel* show(const std::string& why, std::function<void()> onLoggedIn);

protected:
    bool init(const std::string& why, std::function<void()> onLoggedIn);
    bool onKey(cocos2d::EventKeyboard::KeyCode key) override;
    void onClosed() override;
    void submit();
    void submitUserId();
    void setBusy(bool busy);
    void showMessage(const std::string& text, bool error);

    tjfui::TextInput* _email = nullptr;
    tjfui::TextInput* _password = nullptr;
    tjfui::TextInput* _userId = nullptr;
    tjfui::CheckBox* _remember = nullptr;
    ui::Button* _loginBtn = nullptr;
    cocos2d::Label* _message = nullptr;
    cocos2d::Node* _idRow = nullptr;
    cocos2d::Sprite* _spinner = nullptr;
    std::function<void()> _onLoggedIn;
    bool _busy = false;
};

// Logged in: who, and the account's lists and actions.
class AccountPanel : public tjfui::Panel {
public:
    // showSpecial(1 favorites / 2 my levels) switches the browser's list.
    static AccountPanel* show(std::function<void(int)> showSpecial);

protected:
    bool init(std::function<void(int)> showSpecial);
    std::function<void(int)> _showSpecial;
};

// 1..5 stars for a level or a replay; submit() does the request.
class RatePanel : public tjfui::Panel {
public:
    // kind "level" | "replay"
    static RatePanel* show(const std::string& kind, int id, const std::string& name);

protected:
    bool init(const std::string& kind, int id, const std::string& name);
    void submit();
    std::string _kind;
    int _id = 0;
    tjfui::StarPicker* _stars = nullptr;
    ui::Button* _submit = nullptr;
    cocos2d::Label* _message = nullptr;
    bool _busy = false;
};

}  // namespace account
}  // namespace online
