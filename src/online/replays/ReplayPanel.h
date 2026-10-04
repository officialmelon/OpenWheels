#pragma once
// ONLINE (PC addition): the replay screens of the online level browser.
//   ReplayPanel     a level's replays: the player's own runs (this session / saved on this PC) on
//                   top, then the site's replays (sortable: fastest, top rated, newest, oldest);
//                   watch, rate, save, upload.
//   MyReplaysPanel  every run saved on this PC, across levels.
// See online/replays/ReplayRuntime.h for how runs are recorded and replays played.

#include <functional>
#include <string>
#include <vector>

#include "online/HWApi.h"
#include "online/OnlineLevel.h"
#include "online/account/TjfServices.h"
#include "online/account/TjfUi.h"
#include "online/replays/FlashReplay.h"

namespace online {
namespace ui {
class Button;
class Dropdown;
class StarBar;
}  // namespace ui
namespace replays {

// The fastest completed replay of a level (get_all_by_level sorted by completion time).
struct LevelRecord {
    bool loaded = false;
    int count = 0;            // replays listed (the server pages at 500)
    bool has = false;         // a completed replay exists
    ReplayInfo best;
};
const LevelRecord* cachedRecord(int levelId);
RequestId fetchRecord(int levelId, std::function<void(const LevelRecord&)> done);

class ReplayListPanel : public tjfui::Panel {
protected:
    struct Entry {
        bool local = false;
        int runSerial = 0;        // a run of this session (ReplayRuntime), 0 = none
        SavedRun saved;           // local: the saved file (saved.file empty = not saved yet)
        ReplayInfo online;        // !local
        OnlineLevelInfo level;
    };
    struct Row {
        cocos2d::Node* node = nullptr;
        cocos2d::Node* bg = nullptr;
        cocos2d::Node* selectedBg = nullptr;
        cocos2d::Label* rank = nullptr;
        cocos2d::Label* name = nullptr;
        cocos2d::Label* sub = nullptr;
        cocos2d::Label* time = nullptr;
        ui::StarBar* stars = nullptr;
        int bound = -1;
    };

    bool initList(const std::string& title, const std::string& subtitle, bool withSort);
    void buildDetail();
    void setEntries(std::vector<Entry> entries);
    void setListMessage(const std::string& text, bool spinner);
    void refreshRows();
    void bindRow(Row& row, int index);
    void select(int index);
    void refreshDetail();
    void setOffset(float offset);
    float maxOffset() const;
    void onScroll(const cocos2d::Vec2& world, float amount) override;
    bool onKey(cocos2d::EventKeyboard::KeyCode key) override;
    void onClosed() override;
    void setStatus(const std::string& text, bool busy);

    // actions
    void watchSelected();
    void rateSelected();
    void saveSelected();
    void uploadSelected();
    void deleteSelected();
    virtual void reload() = 0;
    SavedRun runOf(const Entry& e) const;
    void startWatching(const OnlineLevelInfo& level, const ReplayInfo& info, const ReplayInput& input,
                       const std::string& flashXml, const std::string& title);

    std::vector<Entry> _entries;
    int _selected = -1;
    float _offset = 0.0f;
    cocos2d::Rect _listRect;
    float _rowH = 156.0f;
    cocos2d::Node* _rowsNode = nullptr;
    std::vector<Row> _rows;
    cocos2d::Node* _listMessage = nullptr;
    ui::Dropdown* _sort = nullptr;
    cocos2d::Label* _count = nullptr;
    // detail
    cocos2d::Rect _detailRect;
    cocos2d::Node* _detail = nullptr;
    cocos2d::Label* _dName = nullptr;
    cocos2d::Label* _dTime = nullptr;
    cocos2d::Label* _dLine1 = nullptr;
    cocos2d::Label* _dLine2 = nullptr;
    ui::StarBar* _dStars = nullptr;
    cocos2d::Label* _dRating = nullptr;
    cocos2d::Label* _dComment = nullptr;
    cocos2d::Label* _dNote = nullptr;
    ui::Button* _watchBtn = nullptr;
    ui::Button* _rateBtn = nullptr;
    ui::Button* _saveBtn = nullptr;
    ui::Button* _uploadBtn = nullptr;
    ui::Button* _deleteBtn = nullptr;
    cocos2d::Label* _status = nullptr;
    cocos2d::Sprite* _spinner = nullptr;
    RequestId _request = 0;
    bool _busy = false;
    int _generation = 0;
    // drag scrolling
    cocos2d::Vec2 _touchStart;
    float _dragStart = 0.0f;
    bool _dragging = false;
};

class ReplayPanel : public ReplayListPanel {
public:
    static ReplayPanel* show(const OnlineLevelInfo& level);
    static void rateReplay(int replayId, int rating, account::ReplyCallback done);

protected:
    bool init(const OnlineLevelInfo& level);
    void reload() override;
    OnlineLevelInfo _level;
};

class MyReplaysPanel : public ReplayListPanel {
public:
    static MyReplaysPanel* show();

protected:
    bool init();
    void reload() override;
};

// Upload form: comment + the honest note + confirm.
class UploadReplayPanel : public tjfui::Panel {
public:
    static UploadReplayPanel* show(const SavedRun& run, std::function<void(int newId)> uploaded);

protected:
    bool init(const SavedRun& run, std::function<void(int)> uploaded);
    bool onKey(cocos2d::EventKeyboard::KeyCode key) override;
    void submit();
    SavedRun _run;
    std::function<void(int)> _uploaded;
    tjfui::TextInput* _comment = nullptr;
    ui::Button* _uploadBtn = nullptr;
    cocos2d::Label* _message = nullptr;
    bool _busy = false;
};

}  // namespace replays
}  // namespace online
