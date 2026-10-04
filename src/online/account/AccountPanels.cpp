// ONLINE (PC addition): see AccountPanels.h.
#include "online/account/AccountPanels.h"

#include <cctype>

#include "online/OnlineUi.h"
#include "online/account/PublishPanel.h"
#include "online/account/TjfAccount.h"
#include "online/account/TjfServices.h"
#include "online/replays/ReplayPanel.h"

USING_NS_CC;

namespace online {
namespace account {

namespace {

const Color3B kError(214, 72, 72);
const Color3B kOk(50, 140, 80);

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

Label* caption(const std::string& text) { return tjfui::label(text, ui::kFontBodyBold, 36.0f, ui::kInkDim); }

}  // namespace

bool ensureLoggedIn(const std::string& why, std::function<void()> after) {
    TjfAccount* account = TjfAccount::get();
    if (account->loggedIn() && account->userId() > 0) {
        if (after) after();
        return true;
    }
    LoginPanel::show(why, std::move(after));
    return false;
}

// ---- LoginPanel ---------------------------------------------------------------------------------

LoginPanel* LoginPanel::show(const std::string& why, std::function<void()> onLoggedIn) {
    auto* p = new (std::nothrow) LoginPanel();
    if (p && p->init(why, std::move(onLoggedIn))) {
        p->autorelease();
        p->present();
        return p;
    }
    delete p;
    return nullptr;
}

bool LoginPanel::init(const std::string& why, std::function<void()> onLoggedIn) {
    if (!initPanel(Size(1900.0f, 1500.0f), "Log In")) return false;
    _onLoggedIn = std::move(onLoggedIn);
    TjfAccount* account = TjfAccount::get();
    const float L = 130.0f, W = _size.width - 260.0f;
    float y = _top - 10.0f;

    Label* intro = tjfui::textBlock(
        why.empty() ? "Log in with your totaljerkface.com account to keep favorites, rate levels and replays, "
                      "upload your replays and publish levels."
                    : why,
        ui::kFontBody, 46.0f, ui::kInk, W);
    intro->setPosition(L, y);
    _content->addChild(intro);
    y -= intro->getContentSize().height + 60.0f;

    Label* c1 = caption("EMAIL");
    c1->setPosition(L, y);
    _content->addChild(c1);
    y -= 90.0f;
    _email = tjfui::TextInput::create(Size(W, 130.0f), "your@email.com", false, 60);
    _email->setPosition(L + W * 0.5f, y);
    _email->setText(account->email());
    _content->addChild(_email);
    y -= 120.0f;

    Label* c2 = caption("PASSWORD");
    c2->setPosition(L, y);
    _content->addChild(c2);
    y -= 90.0f;
    _password = tjfui::TextInput::create(Size(W, 130.0f), "password", true, 45);
    _password->setPosition(L + W * 0.5f, y);
    _password->onSubmit = [this]() { submit(); };
    _content->addChild(_password);
    y -= 130.0f;

    y -= 40.0f;
    _remember = tjfui::CheckBox::create("Remember me on this computer", account->remember(), ui::kInk);
    _remember->setPosition(L, y - 35.0f);
    _content->addChild(_remember);

    _loginBtn = ui::Button::create("LOG IN", Size(520.0f, 150.0f), ui::Button::window("blue"), 60.0f);
    _loginBtn->setPosition(_size.width - 130.0f - 260.0f, y);
    _loginBtn->setCallback([this]() { submit(); });
    _content->addChild(_loginBtn);
    _spinner = ui::createSpinner(70.0f, ui::kInkDim);
    _spinner->setPosition(_loginBtn->getPositionX() - 260.0f - 70.0f, y);
    _spinner->setVisible(false);
    _content->addChild(_spinner);
    y -= 140.0f;

    _message = tjfui::textBlock("", ui::kFontBodyBold, 42.0f, kError, W);
    _message->setPosition(L, y);
    _content->addChild(_message);

    // Fallback when the site didn't reveal the user id (see TjfAccount::identify).
    _idRow = Node::create();
    _idRow->setVisible(false);
    _content->addChild(_idRow);
    Label* c3 = caption("YOUR USER ID (THE NUMBER IN profile.tjf?uid=...)");
    c3->setPosition(L, 330.0f);
    _idRow->addChild(c3);
    _userId = tjfui::TextInput::create(Size(W - 560.0f, 120.0f), "e.g. 1234567", false, 10);
    _userId->setAllowed("0123456789");
    _userId->setPosition(L + (W - 560.0f) * 0.5f, 240.0f);
    _idRow->addChild(_userId);
    auto* save = ui::Button::create("SAVE", Size(500.0f, 130.0f), ui::Button::window("blue"), 56.0f);
    save->setPosition(L + W - 250.0f, 240.0f);
    save->setCallback([this]() { submitUserId(); });
    _idRow->addChild(save);

    Label* foot = tjfui::textBlock(
        "Your password goes only to totaljerkface.com and is never saved. \"Remember me\" keeps the site's "
        "session cookie on this computer. No account yet? Register at totaljerkface.com.",
        ui::kFontBody, 36.0f, ui::kInkDim, W);
    foot->setPosition(L, 140.0f);
    _content->addChild(foot);

    if (account->loggedIn() && account->userId() <= 0) {
        showMessage("Logged in, but your user id is unknown. Enter it below.", true);
        _idRow->setVisible(true);
    }
    _firstField = _email->text().empty() ? _email : _password;
    return true;
}

bool LoginPanel::onKey(EventKeyboard::KeyCode key) {
    using K = EventKeyboard::KeyCode;
    switch (key) {
        case K::KEY_TAB:
            if (_email->focused()) _password->focus();
            else _email->focus();
            return true;
        case K::KEY_ENTER:
        case K::KEY_KP_ENTER:
            if (_userId->focused()) submitUserId();
            else if (_email->focused()) _password->focus();
            else submit();
            return true;
        case K::KEY_V:
            if (!_ctrlDown) return false;
            if (_email->focused()) _email->paste();
            else if (_password->focused()) _password->paste();
            else if (_userId->focused()) _userId->paste();
            return true;
        default:
            return false;
    }
}

void LoginPanel::onClosed() {
    _password->clearSecret();
    _email->unfocus();
    _password->unfocus();
    _userId->unfocus();
}

void LoginPanel::setBusy(bool busy) {
    _busy = busy;
    _loginBtn->setEnabled(!busy);
    _spinner->setVisible(busy);
}

void LoginPanel::showMessage(const std::string& text, bool error) {
    _message->setString(text);
    _message->setColor(error ? kError : kOk);
}

void LoginPanel::submit() {
    if (_busy || _closing) return;
    const std::string email = trim(_email->text());
    if (email.size() < 6 || email.find('@') == std::string::npos) {
        showMessage("Please enter the email address of your totaljerkface.com account.", true);
        _email->focus();
        return;
    }
    if (_password->text().empty()) {
        showMessage("Please enter your password.", true);
        _password->focus();
        return;
    }
    setBusy(true);
    showMessage("Logging in...", false);
    RefPtr<LoginPanel> self(this);
    TjfAccount::get()->login(email, _password->text(), _remember->checked(),
                             [self](bool ok, const std::string& message) {
                                 LoginPanel* p = self.get();
                                 if (p->_closing) return;
                                 p->setBusy(false);
                                 p->_password->clearSecret();
                                 if (!ok) {
                                     p->showMessage(message, true);
                                     p->_password->focus();
                                     return;
                                 }
                                 if (TjfAccount::get()->userId() <= 0) {
                                     p->showMessage(message, true);
                                     p->_idRow->setVisible(true);
                                     p->_userId->focus();
                                     return;
                                 }
                                 auto after = p->_onLoggedIn;
                                 p->dismiss();
                                 ui::showToast("Logged in", {"Welcome, " + TjfAccount::get()->displayName() + "!"}, 0.0f);
                                 if (after) after();
                             });
    _password->clearSecret();
}

void LoginPanel::submitUserId() {
    const int id = std::atoi(_userId->text().c_str());
    if (id <= 0) {
        showMessage("Enter the number from your profile link (profile.tjf?uid=...).", true);
        return;
    }
    TjfAccount::get()->setUserId(id);
    auto after = _onLoggedIn;
    dismiss();
    if (after) after();
}

// ---- AccountPanel -------------------------------------------------------------------------------

AccountPanel* AccountPanel::show(std::function<void(int)> showSpecial) {
    auto* p = new (std::nothrow) AccountPanel();
    if (p && p->init(std::move(showSpecial))) {
        p->autorelease();
        p->present();
        return p;
    }
    delete p;
    return nullptr;
}

bool AccountPanel::init(std::function<void(int)> showSpecial) {
    if (!initPanel(Size(1700.0f, 1500.0f), "Your Account")) return false;
    _showSpecial = std::move(showSpecial);
    TjfAccount* account = TjfAccount::get();
    const float L = 130.0f, W = _size.width - 260.0f;
    float y = _top;

    Sprite* avatar = tjfui::icon("user", 150.0f);
    avatar->setColor(ui::kBlue);
    avatar->setPosition(L + 75.0f, y - 60.0f);
    _content->addChild(avatar);
    Label* name = tjfui::label(account->displayName(), ui::kFontHeading, 76.0f, ui::kInk);
    name->setPosition(L + 200.0f, y - 30.0f);
    ui::setEllipsized(name, account->displayName(), W - 220.0f);
    _content->addChild(name);
    std::string sub = account->userId() > 0 ? "totaljerkface.com user #" + std::to_string(account->userId())
                                            : "totaljerkface.com (user id unknown)";
    if (!account->remember()) sub += "  \xC2\xB7  until you quit";
    Label* subl = tjfui::label(sub, ui::kFontBody, 42.0f, ui::kInkDim);
    subl->setPosition(L + 202.0f, y - 105.0f);
    _content->addChild(subl);
    y -= 250.0f;

    const float bw = (W - 50.0f) * 0.5f, bh = 170.0f;
    struct Item {
        const char* text;
        const char* icon;
        std::string colour;
        std::function<void()> action;
    };
    RefPtr<AccountPanel> self(this);
    const std::vector<Item> items = {
        {"FAVORITES", "heart", "pink", [self]() { auto f = self->_showSpecial; self->dismiss(); if (f) f(1); }},
        {"MY LEVELS", "user", "blue", [self]() { auto f = self->_showSpecial; self->dismiss(); if (f) f(2); }},
        {"MY REPLAYS", "replay", "blue", [self]() { self->dismiss(); replays::MyReplaysPanel::show(); }},
        {"PUBLISH A LEVEL", "upload", "pink", [self]() { self->dismiss(); PublishPanel::showPicker(); }},
    };
    for (size_t i = 0; i < items.size(); ++i) {
        auto* b = ui::Button::create(items[i].text, Size(bw, bh), ui::Button::chunky(items[i].colour), 54.0f);
        Sprite* ic = tjfui::icon(items[i].icon, 70.0f);
        b->setIcon(ic);
        b->setPosition(L + bw * 0.5f + (i % 2) * (bw + 50.0f), y - bh * 0.5f - (i / 2) * (bh + 40.0f));
        b->setCallback(items[i].action);
        _content->addChild(b);
    }
    y -= 2 * (bh + 40.0f) + 40.0f;

    Label* note = tjfui::textBlock(
        "Favorites and My Levels open in the level list. Replays of your runs are kept on this computer until "
        "you upload them.",
        ui::kFontBody, 40.0f, ui::kInkDim, W);
    note->setPosition(L, y);
    _content->addChild(note);

    auto* logout = ui::Button::create("LOG OUT", Size(480.0f, 140.0f), ui::Button::window("yellow"), 54.0f);
    logout->setPosition(_size.width * 0.5f, 150.0f);
    logout->setCallback([self]() {
        self->dismiss();
        TjfAccount::get()->logout(nullptr);
        ui::showToast("Logged out", {"You are no longer logged in to totaljerkface.com."}, 0.0f);
    });
    _content->addChild(logout);
    return true;
}

// ---- RatePanel ----------------------------------------------------------------------------------

RatePanel* RatePanel::show(const std::string& kind, int id, const std::string& name) {
    auto* p = new (std::nothrow) RatePanel();
    if (p && p->init(kind, id, name)) {
        p->autorelease();
        p->present();
        return p;
    }
    delete p;
    return nullptr;
}

bool RatePanel::init(const std::string& kind, int id, const std::string& name) {
    if (!initPanel(Size(1500.0f, 1000.0f), kind == "replay" ? "Rate Replay" : "Rate Level")) return false;
    _kind = kind;
    _id = id;
    const float W = _size.width - 260.0f;
    Label* n = tjfui::label("", ui::kFontHeading, 64.0f, ui::kInk, Vec2(0.5f, 0.5f));
    ui::setEllipsized(n, name, W);
    n->setPosition(_size.width * 0.5f, _top - 20.0f);
    _content->addChild(n);

    _stars = tjfui::StarPicker::create(150.0f);
    _stars->setPosition(_size.width * 0.5f - _stars->getContentSize().width * 0.5f, _top - 300.0f);
    _content->addChild(_stars);
    _stars->onPick = [this](int rating) {
        _submit->setEnabled(true);
        _submit->setText("SUBMIT " + std::to_string(rating) + (rating == 1 ? " STAR" : " STARS"));
    };

    _message = tjfui::label("Pick 1 to 5 stars. Your vote counts on totaljerkface.com.", ui::kFontBody, 42.0f,
                            ui::kInkDim, Vec2(0.5f, 0.5f));
    _message->setPosition(_size.width * 0.5f, _top - 400.0f);
    _content->addChild(_message);

    _submit = ui::Button::create("SUBMIT", Size(720.0f, 150.0f), ui::Button::window("blue"), 56.0f);
    _submit->setPosition(_size.width * 0.5f, 150.0f);
    _submit->setEnabled(false);
    _submit->setCallback([this]() { submit(); });
    _content->addChild(_submit);
    return true;
}

void RatePanel::submit() {
    if (_busy || _stars->rating() <= 0) return;
    _busy = true;
    _submit->setEnabled(false);
    _message->setString("Sending your vote...");
    RefPtr<RatePanel> self(this);
    auto done = [self](const Reply& reply) {
        RatePanel* p = self.get();
        p->_busy = false;
        if (reply.ok) {
            if (!p->_closing) p->dismiss();
            ui::showToast("You voted! Good job.", {"Thanks for rating."}, 0.0f);
            return;
        }
        if (p->_closing) return;
        p->_message->setString(reply.message);
        p->_message->setColor(kError);
        p->_submit->setEnabled(true);
    };
    if (_kind == "replay") {
        replays::ReplayPanel::rateReplay(_id, _stars->rating(), done);
    } else {
        TjfServices::get()->rateLevel(_id, _stars->rating(), done);
    }
}

}  // namespace account
}  // namespace online
