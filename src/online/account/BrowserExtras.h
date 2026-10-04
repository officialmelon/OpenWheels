#pragma once
// ONLINE (PC addition): the account / replay additions to the online level browser
// (OnlineLevelBrowser), kept out of the browser's own file: the header's account button, the
// detail panel's favorite (heart), RATE and REPLAYS buttons with the level's record, and the
// "Favorites" / "My Levels" lists. The browser calls these hooks; everything else lives here.

#include <functional>
#include <string>
#include <vector>

#include "cocos2d.h"
#include "online/HWApi.h"
#include "online/OnlineLevel.h"

namespace online {

namespace ui {
class Button;
}

class BrowserExtras : public cocos2d::Node {
public:
    // Special lists shown through the sort dropdown (BrowserState::special).
    enum Special { None = 0, Favorites = 1, MyLevels = 2 };
    static const char* specialName(int special);   // "Favorites", "My Levels"

    static BrowserExtras* create();

    // Header: the account button at `centre` (height h). Returns its width.
    float buildAccountButton(cocos2d::Node* parent, const cocos2d::Vec2& centre, float h);
    static float accountButtonWidth() { return 470.0f; }
    // Detail panel: RATE and the heart at the right end of the author line (authorY), and the
    // REPLAYS bar across the panel (L..R) below `y`. Returns the y below the bar.
    float buildDetail(cocos2d::Node* detail, float authorY, float L, float R, float y);
    // Width the author link must leave free at the right of the author line.
    static float authorLineReserve() { return 380.0f; }

    // The selected level changed (nullptr = none).
    void onSelect(const OnlineLevelInfo* level);
    // Loads a special list; the callback has HWApi::ListCallback's shape.
    RequestId loadSpecial(int special, HWApi::ListCallback done);
    // Asks for a login first when needed; false when the panel had to open (onLoggedIn runs after).
    bool requireLogin(const std::string& why, std::function<void()> onLoggedIn);
    std::string emptyHint(int special) const;
    // A level was started from the browser (its runs get recorded as replays).
    static void levelStarted(const OnlineLevelInfo& level);

    // Switch the browser to a special list (account panel buttons).
    std::function<void(int special)> showSpecial;

protected:
    bool init() override;
    void onEnter() override;
    void onExit() override;
    void refreshAccount();
    void refreshHeart();
    void refreshRecord();
    void toggleFavorite();
    void openRate();
    void openReplays();
    void fetchRecord(int levelId);

    ui::Button* _accountBtn = nullptr;
    ui::Button* _heartBtn = nullptr;
    ui::Button* _rateBtn = nullptr;
    ui::Button* _replaysBtn = nullptr;
    cocos2d::Label* _recordLabel = nullptr;
    cocos2d::EventListenerCustom* _accountListener = nullptr;
    bool _hasLevel = false;
    OnlineLevelInfo _level;
    RequestId _recordRequest = 0;
    RequestId _favRequest = 0;
    bool _favBusy = false;
    cocos2d::Sprite* _replaysIcon = nullptr;
};

}  // namespace online
