#pragma once
// ONLINE (PC addition): browse, search and play the browser game's user levels
// (totaljerkface.com) through HWApi. Not part of the 1:1 reconstruction; reached from the main
// menu's online button (MainMenu tag 5).
//
// Layout (70-unit grid, like MainMenu): a header (back button, title, search field with a
// "by Name / by Author" dropdown), a slim filter bar over the list (sort and period dropdowns,
// result count, paging), the result list (about six big recycled rows; wheel/drag scrolling)
// and an HWWindow-style detail panel for the selected level with a main-menu-style PLAY button.
//
// The query, results, selection and scroll position live in a process-wide BrowserState, so the
// browser reopens where the player left it. Leaving a level that was started here comes back to
// the browser: MainMenu::createScene asks sceneForReturnFromLevel() first.

#include <string>
#include <vector>

#include "cocos2d.h"
#include "ui/UIScale9Sprite.h"
#include "online/HWApi.h"
#include "online/OnlineLevel.h"

namespace online {

class BrowserExtras;  // ONLINE (PC addition): account / replay additions (online/account)

namespace ui {
class Button;
class Dropdown;
class SearchField;
class StarBar;
}  // namespace ui

struct BrowserState {
    LevelQuery query;                       // last query sent (term, sort, period, page)
    bool featured = false;                  // showing the featured list instead of query
    std::string fieldText;                  // search field contents
    SearchBy searchBy = SearchBy::Name;     // "by Name / by Author"
    std::vector<OnlineLevelInfo> levels;    // current results
    int special = 0;                        // ONLINE (PC addition): BrowserExtras::Favorites / MyLevels list
    int perPage = 0;                        // server page size of the last result (500)
    bool loaded = false;                    // levels is the result of query/featured
    int selected = -1;
    float listOffset = 0.0f;
    // Set when a level is started from the browser; consumed by sceneForReturnFromLevel.
    bool returnPending = false;
    cocos2d::RefPtr<cocos2d::Scene> parkedScene;  // the browser scene the level was pushed over
};

class OnlineLevelBrowser : public cocos2d::Layer {
public:
    static cocos2d::Scene* createScene();
    static BrowserState& state();
    // Non-null once after a level started from the browser ends (exit, back from character
    // select, next level): a fresh browser scene to show instead of the main menu.
    static cocos2d::Scene* sceneForReturnFromLevel();

    CREATE_FUNC(OnlineLevelBrowser);
    bool init() override;
    void onEnter() override;
    void onExit() override;
    void update(float dt) override;

private:
    OnlineLevelBrowser() = default;
    ~OnlineLevelBrowser() override;

    struct Row {
        cocos2d::Node* node = nullptr;
        cocos2d::ui::Scale9Sprite* bg = nullptr;   // dark bar
        cocos2d::Sprite* selectedBg = nullptr;     // blue main-menu button art
        cocos2d::Sprite* portrait = nullptr;
        cocos2d::Node* tagNode = nullptr;
        cocos2d::Label* tag = nullptr;
        cocos2d::Label* name = nullptr;
        cocos2d::Label* author = nullptr;
        ui::StarBar* stars = nullptr;
        cocos2d::Label* rating = nullptr;
        int bound = -1;
    };

    // building
    void buildHeader();
    void buildFilters();
    void buildList();
    void buildDetail();
    void buildInput();

    // actions
    void goBack();
    void submitSearch();
    void scheduleSearch();
    void setSortIndex(int index);
    void setPeriodIndex(int index);
    void searchAuthor(const std::string& author);
    void changePage(int delta);
    void load();
    void playSelected();
    void finishPlay(const std::string& xml);

    // state -> view
    void refreshFilters();
    void refreshListState();       // loading / error / empty / list + count + paging
    void refreshRows(bool rebindAll);
    void bindRow(Row& row, int index);
    void styleRow(Row& row, int index);
    void refreshDetail();
    void refreshPlayButton();
    void select(int index, bool scrollIntoView);
    void setListOffset(float offset);
    float maxListOffset() const;
    int rowAt(const cocos2d::Vec2& world) const;
    void setCommentOffset(float offset);
    void setPortrait(cocos2d::Sprite* sprite, int character, float fitHeight, float fitWidth);

    cocos2d::Size _vs;
    cocos2d::Vec2 _origin;
    float _top = 0.0f;                 // visible top edge (y)

    // header / filters
    ui::SearchField* _field = nullptr;
    ui::Dropdown* _searchBy = nullptr;
    ui::Dropdown* _sort = nullptr;
    ui::Dropdown* _period = nullptr;
    cocos2d::Label* _countText = nullptr;
    cocos2d::Label* _pageText = nullptr;
    ui::Button* _prevBtn = nullptr;
    ui::Button* _nextBtn = nullptr;

    // list
    cocos2d::Rect _rowsRect;           // world rect of the visible rows
    float _rowH = 236.0f;              // row pitch (row + gap)
    cocos2d::Node* _rowsNode = nullptr;
    std::vector<Row> _rows;
    cocos2d::ui::Scale9Sprite* _scrollThumb = nullptr;
    cocos2d::Node* _listMessage = nullptr;   // loading / error / empty
    int _hoverRow = -1;

    // detail
    cocos2d::Rect _detailRect;
    cocos2d::Node* _detail = nullptr;
    cocos2d::Node* _detailEmpty = nullptr;
    cocos2d::Sprite* _detailPortrait = nullptr;
    cocos2d::Node* _detailTag = nullptr;
    cocos2d::Label* _detailTagLabel = nullptr;
    cocos2d::Label* _detailName = nullptr;
    float _detailNameMaxH = 0.0f;
    ui::Button* _authorBtn = nullptr;
    ui::StarBar* _detailStars = nullptr;
    cocos2d::Label* _detailRating = nullptr;
    cocos2d::Label* _detailVotes = nullptr;
    cocos2d::Label* _detailPlays = nullptr;
    cocos2d::Label* _detailDate = nullptr;
    cocos2d::Label* _detailChar = nullptr;
    float _statColW = 0.0f;
    cocos2d::Rect _commentRect;        // world rect of the comment viewport
    cocos2d::Label* _comment = nullptr;
    cocos2d::ui::Scale9Sprite* _commentThumb = nullptr;
    float _commentOffset = 0.0f;
    cocos2d::Node* _cachedBadge = nullptr;
    cocos2d::Label* _status = nullptr;
    cocos2d::Sprite* _statusSpinner = nullptr;
    ui::Button* _playBtn = nullptr;
    BrowserExtras* _extras = nullptr;   // ONLINE (PC addition): account button, favorite, rate, replays
    void setSpecial(int special);       // ONLINE (PC addition): Favorites / My Levels list
    ui::Button* _editBtn = nullptr;   // EDITOR (PC addition): open the level in the level editor
    void editSelected();              // EDITOR (PC addition)
    ui::Button* _sendBtn = nullptr;   // NET (PC addition): send a downloaded level to a nearby player
    void sendSelected();              // NET (PC addition)

    // requests
    RequestId _listRequest = 0;
    RequestId _downloadRequest = 0;
    int _generation = 0;
    bool _loading = false;
    std::string _error;
    bool _playing = false;   // downloading / converting / starting

    // input
    enum class Drag { None, ListPending, List, Scrollbar, Comment };
    Drag _drag = Drag::None;
    cocos2d::Vec2 _touchStart;
    float _dragStartOffset = 0.0f;
    float _velocity = 0.0f;
    float _lastDragY = 0.0f;
    double _lastDragTime = 0.0;
    int _lastClickRow = -1;
    double _lastClickTime = 0.0;
    bool _ctrlDown = false;
};

}  // namespace online
