// ONLINE (PC addition): see BrowserExtras.h.
#include "online/account/BrowserExtras.h"

#include "online/OnlineUi.h"
#include "online/account/AccountPanels.h"
#include "online/account/TjfAccount.h"
#include "online/account/TjfServices.h"
#include "online/account/TjfUi.h"
#include "online/replays/FlashReplay.h"
#include "online/replays/ReplayPanel.h"
#include "online/replays/ReplayRuntime.h"

USING_NS_CC;

namespace online {

using account::TjfAccount;
using account::TjfServices;

namespace {

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

}  // namespace

const char* BrowserExtras::specialName(int special) {
    switch (special) {
        case Favorites: return "Favorites";
        case MyLevels: return "My Levels";
        default: return "";
    }
}

BrowserExtras* BrowserExtras::create() {
    auto* e = new (std::nothrow) BrowserExtras();
    if (e && e->init()) {
        e->autorelease();
        return e;
    }
    delete e;
    return nullptr;
}

bool BrowserExtras::init() {
    if (!Node::init()) return false;
    TjfAccount::get();  // cookie hooks before the first request
    return true;
}

void BrowserExtras::onEnter() {
    Node::onEnter();
    _accountListener = Director::getInstance()->getEventDispatcher()->addCustomEventListener(
        account::kAccountChangedEvent, [this](EventCustom*) {
            refreshAccount();
            refreshHeart();
        });
    refreshAccount();
    // Favorite state for the hearts: once per login.
    TjfAccount* a = TjfAccount::get();
    if (a->loggedIn() && !TjfServices::get()->favoritesKnown() && !_favRequest) {
        RefPtr<BrowserExtras> self(this);
        _favRequest = TjfServices::get()->listFavorites(
            [self](bool, const std::string&, const std::vector<OnlineLevelInfo>&) {
                self->_favRequest = 0;
                self->refreshHeart();
            });
    }
}

void BrowserExtras::onExit() {
    if (_accountListener) {
        Director::getInstance()->getEventDispatcher()->removeEventListener(_accountListener);
        _accountListener = nullptr;
    }
    HWApi::getInstance()->cancel(_recordRequest);
    HWApi::getInstance()->cancel(_favRequest);
    _recordRequest = _favRequest = 0;
    unschedule("record_dwell");
    Node::onExit();
}

float BrowserExtras::buildAccountButton(Node* parent, const Vec2& centre, float h) {
    const float w = accountButtonWidth();
    _accountBtn = ui::Button::create("LOG IN", Size(w, h), ui::Button::window("pink"), 50.0f);
    _accountBtn->setIcon(tjfui::icon("user", h * 0.42f));
    _accountBtn->setPosition(centre);
    _accountBtn->setName("tjf_account");
    _accountBtn->setCallback([this]() {
        if (TjfAccount::get()->loggedIn() && TjfAccount::get()->userId() > 0) {
            RefPtr<BrowserExtras> self(this);
            account::AccountPanel::show([self](int special) {
                if (self->showSpecial) self->showSpecial(special);
            });
        } else {
            account::LoginPanel::show("", nullptr);
        }
    });
    parent->addChild(_accountBtn, 5);
    refreshAccount();
    return w;
}

void BrowserExtras::refreshAccount() {
    if (!_accountBtn) return;
    TjfAccount* a = TjfAccount::get();
    if (a->loggedIn()) {
        _accountBtn->setStyle(ui::Button::window("blue"));
        ui::setEllipsized(_accountBtn->label(), a->displayName(), accountButtonWidth() - 150.0f);
        _accountBtn->setText(_accountBtn->label()->getString());
    } else {
        _accountBtn->setStyle(ui::Button::window("pink"));
        _accountBtn->setText("LOG IN");
    }
}

float BrowserExtras::buildDetail(Node* detail, float authorY, float L, float R, float y) {
    const Vec2 heartPos(R - 60.0f, authorY);
    const Vec2 ratePos(R - 130.0f - 20.0f - 95.0f, authorY);
    const float barH = 132.0f;
    const Rect replaysRect(L, y - barH, R - L, barH);
    // Favorite: a heart on the light panel (pink when it is one of the player's favorites).
    ui::Button::Style heart;
    heart.color = Color3B(60, 60, 70);
    heart.opacity = 0;
    heart.radius = 40.0f;
    heart.textColor = Color3B(170, 170, 180);
    _heartBtn = ui::Button::create("", Size(130.0f, 120.0f), heart);
    _heartBtn->setIcon(tjfui::icon("heart_outline", 96.0f));
    _heartBtn->setPosition(heartPos);
    _heartBtn->setName("tjf_heart");
    _heartBtn->setCallback([this]() { toggleFavorite(); });
    detail->addChild(_heartBtn);

    _rateBtn = ui::Button::create("RATE", Size(190.0f, 84.0f), ui::Button::window("yellow"), 38.0f, ui::kFontBodyBold);
    _rateBtn->setPosition(ratePos);
    _rateBtn->setName("tjf_rate");
    _rateBtn->setCallback([this]() { openRate(); });
    detail->addChild(_rateBtn);

    // REPLAYS bar: label + icon on the left, the level's record (fastest finished replay) right.
    _replaysBtn = ui::Button::create("REPLAYS", replaysRect.size, ui::Button::chunky("pink"), 58.0f);
    _replaysBtn->setPosition(replaysRect.getMidX(), replaysRect.getMidY());
    _replaysBtn->setName("tjf_replays");
    _replaysBtn->setCallback([this]() { openReplays(); });
    detail->addChild(_replaysBtn);
    _recordLabel = tjfui::label("", ui::kFontBodyBold, 40.0f, Color3B::WHITE, Vec2(1.0f, 0.5f));
    _recordLabel->setPosition(replaysRect.size.width - 60.0f, replaysRect.size.height * 0.5f + 4.0f);
    _replaysBtn->addChild(_recordLabel, 2);
    _replaysIcon = tjfui::icon("replay", 66.0f);
    _replaysBtn->setIcon(_replaysIcon);
    refreshRecord();
    return y - barH - 34.0f;
}

void BrowserExtras::onSelect(const OnlineLevelInfo* level) {
    unschedule("record_dwell");
    HWApi::getInstance()->cancel(_recordRequest);
    _recordRequest = 0;
    _hasLevel = level != nullptr;
    if (level) _level = *level;
    refreshHeart();
    refreshRecord();
    if (!_hasLevel || replays::cachedRecord(_level.id)) return;
    // The record costs one request: only for a level the player stays on for a moment.
    const int id = _level.id;
    scheduleOnce([this, id](float) {
        if (!_hasLevel || _level.id != id) return;
        fetchRecord(id);
    }, 1.2f, "record_dwell");
}

void BrowserExtras::fetchRecord(int levelId) {
    RefPtr<BrowserExtras> self(this);
    _recordRequest = replays::fetchRecord(levelId, [self, levelId](const replays::LevelRecord&) {
        self->_recordRequest = 0;
        if (self->_hasLevel && self->_level.id == levelId) self->refreshRecord();
    });
}

void BrowserExtras::refreshRecord() {
    if (!_recordLabel) return;
    std::string text;
    if (_hasLevel) {
        if (const replays::LevelRecord* r = replays::cachedRecord(_level.id)) {
            if (r->has) text = "Record " + replays::formatTime(r->best.frames) + " \xC2\xB7 " + trim(r->best.userName);
            else text = r->count ? "no finished runs yet" : "no replays yet";
        }
    }
    ui::setEllipsized(_recordLabel, text, _replaysBtn->getContentSize().width * 0.5f - 60.0f);
    // Label + icon centred (setText lays them out), moved into the left half when a record shows.
    _replaysBtn->setText("REPLAYS");
    if (!text.empty()) {
        const float shift = -(_replaysBtn->getContentSize().width * 0.25f - 40.0f);
        _replaysBtn->label()->setPositionX(_replaysBtn->label()->getPositionX() + shift);
        if (_replaysIcon) _replaysIcon->setPositionX(_replaysIcon->getPositionX() + shift);
    }
}

void BrowserExtras::refreshHeart() {
    if (!_heartBtn) return;
    const bool fav = _hasLevel && TjfServices::get()->isFavorite(_level.id);
    ui::Button::Style s;
    s.color = Color3B(60, 60, 70);
    s.opacity = 0;
    s.radius = 40.0f;
    s.textColor = fav ? ui::kPink : Color3B(170, 170, 180);
    _heartBtn->setStyle(s);
    _heartBtn->setIcon(tjfui::icon(fav ? "heart" : "heart_outline", 96.0f));
    _heartBtn->setEnabled(!_favBusy);
}

void BrowserExtras::toggleFavorite() {
    if (!_hasLevel || _favBusy) return;
    const OnlineLevelInfo level = _level;
    RefPtr<BrowserExtras> self(this);
    account::ensureLoggedIn("Log in to keep favorite levels.", [self, level]() {
        BrowserExtras* e = self.get();
        const bool add = !TjfServices::get()->isFavorite(level.id);
        e->_favBusy = true;
        e->refreshHeart();
        TjfServices::get()->setFavorite(level.id, add, [self, add, level](const account::Reply& reply) {
            BrowserExtras* x = self.get();
            x->_favBusy = false;
            x->refreshHeart();
            if (!reply.ok && reply.reason != "duplicate") {
                tjfui::alert("Favorites", reply.message);
                return;
            }
            ui::showToast(add ? "Added to favorites" : "Removed from favorites", {trim(level.name)}, 0.0f);
        });
    });
}

void BrowserExtras::openRate() {
    if (!_hasLevel) return;
    const OnlineLevelInfo level = _level;
    account::ensureLoggedIn("Log in to rate levels.",
                            [level]() { account::RatePanel::show("level", level.id, trim(level.name)); });
}

void BrowserExtras::openReplays() {
    if (_hasLevel) replays::ReplayPanel::show(_level);
}

RequestId BrowserExtras::loadSpecial(int special, HWApi::ListCallback done) {
    if (special == Favorites) {
        return TjfServices::get()->listFavorites(
            [done](bool ok, const std::string& error, const std::vector<OnlineLevelInfo>& levels) {
                done(ok, error, levels, 1, 0);
            });
    }
    return TjfServices::get()->listMyLevels([done](bool ok, const std::string& error,
                                                   const std::vector<OnlineLevelInfo>& priv,
                                                   const std::vector<OnlineLevelInfo>& pub) {
        std::vector<OnlineLevelInfo> all = pub;
        for (OnlineLevelInfo l : priv) {
            l.name = "[private] " + trim(l.name);
            all.push_back(l);
        }
        done(ok, error, all, 1, 0);
    });
}

bool BrowserExtras::requireLogin(const std::string& why, std::function<void()> onLoggedIn) {
    return account::ensureLoggedIn(why, std::move(onLoggedIn));
}

std::string BrowserExtras::emptyHint(int special) const {
    if (special == Favorites) return "You have no favorite levels yet.\nTap the heart on a level to add it.";
    return "You haven't saved any levels on totaljerkface.com yet.";
}

void BrowserExtras::levelStarted(const OnlineLevelInfo& level) { replays::beginOnlineRun(level); }

}  // namespace online
