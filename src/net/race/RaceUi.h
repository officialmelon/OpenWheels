#pragma once
// NET (PC addition): ghost race screens, in the style of the Send to Nearby panels (net::ui::Modal,
// the online browser's widget kit and the game's HWWindow art).
//
// LobbyPanel ("Race"): the level (campaign chapter / level pickers for the host when the race was
//   opened from the main menu), the riders with their colour, rider and ready state, the local
//   rider picker (portrait, name, < >), the nearby players with Invite buttons (host) or a short
//   explanation (guest), invite by code, and one big button: Start Race (host) / Ready (guest).
// ResultsPanel ("Race Results"): places, times, Rematch / Lobby (host) and Leave.
// RaceHud: in the level - the race clock, each rider's colour, progress and state, the 3-2-1-GO
//   countdown, the finish banner and Give Up / End Race / Results buttons.

#include <string>
#include <vector>

#include "cocos2d.h"
#include "net/LanDiscovery.h"
#include "net/NetUi.h"

namespace online {
namespace ui {
class Button;
class SearchField;
class Dropdown;
}  // namespace ui
}  // namespace online

namespace race {

std::string formatTime(int ms);
std::string placeText(int place);   // "1st", "2nd"...

class LobbyPanel : public net::ui::Modal {
public:
    static LobbyPanel* create();
    ~LobbyPanel() override;
    void refresh();

protected:
    bool init();
    void onClosed() override;
    bool onKey(cocos2d::EventKeyboard::KeyCode key) override;
    void onTouch(const cocos2d::Vec2& world) override;

private:
    void buildLevelRow();
    void rebuildPlayers();
    void rebuildRider();
    void rebuildNearby();
    void mainPressed();
    void cycleCharacter(int step);
    void inviteCode();

    float _w = 0.0f;
    cocos2d::Node* _levelRow = nullptr;
    online::ui::Dropdown* _chapterDrop = nullptr;
    online::ui::Dropdown* _levelDrop = nullptr;
    std::string _levelKey;
    cocos2d::Rect _playersBox, _riderBox, _nearbyBox;
    cocos2d::Node* _playersNode = nullptr;
    cocos2d::Node* _riderNode = nullptr;
    cocos2d::Node* _nearbyNode = nullptr;
    cocos2d::Label* _status = nullptr;
    online::ui::Button* _mainBtn = nullptr;
    online::ui::SearchField* _codeField = nullptr;
    online::ui::Button* _codeBtn = nullptr;
    std::vector<net::Peer> _peers;
    int _observer = 0;
    std::string _riderKey;
    std::string _nearbyKey;
};

class ResultsPanel : public net::ui::Modal {
public:
    static ResultsPanel* create();
    void refresh();

protected:
    bool init();
    void onClosed() override;
    bool onKey(cocos2d::EventKeyboard::KeyCode key) override;

private:
    float _w = 0.0f;
    cocos2d::Node* _rows = nullptr;
    cocos2d::Node* _buttons = nullptr;
    std::string _key;
};

class RaceHud : public cocos2d::Node {
public:
    static RaceHud* create();
    void refresh();

private:
    bool init() override;
    void layoutButtons();

    cocos2d::Node* _panel = nullptr;
    cocos2d::Label* _clock = nullptr;
    cocos2d::Node* _rows = nullptr;
    cocos2d::Label* _countdown = nullptr;
    cocos2d::Label* _banner = nullptr;
    online::ui::Button* _giveUp = nullptr;
    online::ui::Button* _endRace = nullptr;
    online::ui::Button* _results = nullptr;
    int _lastCount = -1;
    std::string _rowsKey;
    float _panelW = 820.0f;
};

}  // namespace race
