#pragma once
// UI (PC addition): restyled - the user-level screen (main menu -> grey play button), in the
// style of the online level browser (online/OnlineLevelBrowser, online/OnlineUi). It replaces the
// revived iOS UserLevelSelectUIView panel as the main menu's entry (UserLevelSelectUIView::scene())
// and keeps its flows: PLAY (LevelSession, as -[UserLevelSelectUIView playBtnPressed:]), EDIT
// (EditorLayer::createSceneWithLevelMO), DELETE (HWWindow confirmation, LevelStore deleteObject +
// save), NEW LEVEL (EditorLayer::createScene), plus SHARE (ShareAction .happywheels export, as the
// editor's SHARE LEVEL), SEND TO NEARBY and RECEIVE (src/net/NearbyPanels.h).
//
// Layout (design units, MainMenu's 70-unit grid):
//   header: the menus' round back button, "Your Levels" (Clarendon), tabs "My Levels" (chapter
//           5000, editor saves and custom files) / "Received" (chapter 5001: levels from nearby
//           players and imported .happywheels files), NEW LEVEL and RECEIVE on the right;
//   left:   the levels as big rows (dark bar; the main menu's white-bordered blue button art when
//           selected): portrait of the forced character (generic + "ANY" when the player picks),
//           the name in Clarendon, a muted line with character and last edit date;
//   right:  an HWWindow-style panel: name, portrait card, character / created / edited, the
//           description, EDIT / SEND TO NEARBY / SHARE / DELETE and the big main-menu PLAY button.
//   Empty tab: "No levels yet" with what to do.
// Input: click / tap rows, double-click plays, drag or wheel scrolls, Up/Down select, Enter plays,
// Delete deletes (with confirmation), Tab switches tabs, Esc goes back to the main menu.

#include <string>
#include <vector>

#include "cocos2d.h"
#include "HWWindowDelegate.h"
#include "LevelMO.h"
#include "ui/UIScale9Sprite.h"


namespace online {
namespace ui {
class Button;
}
}  // namespace online

class UserLevelsScreen : public cocos2d::Layer, public HWWindowDelegate
{
public:
    static cocos2d::Scene* createScene();
    CREATE_FUNC(UserLevelsScreen);
    bool init() override;
    void onEnter() override;
    void onExit() override;

    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override;

private:
    UserLevelsScreen() = default;
    ~UserLevelsScreen() override;

    struct Row {
        cocos2d::Node* node = nullptr;
        cocos2d::ui::Scale9Sprite* bg = nullptr;
        cocos2d::Sprite* selectedBg = nullptr;
        cocos2d::Sprite* portrait = nullptr;
        cocos2d::Node* tagNode = nullptr;
        cocos2d::Label* tag = nullptr;
        cocos2d::Label* name = nullptr;
        cocos2d::Label* sub = nullptr;
        int bound = -1;
    };

    void buildHeader();
    void buildList();
    void buildDetail();
    void buildInput();

    void reload();                 // re-reads the store, keeps the selection by level id
    void setTab(int tab);
    void select(int index, bool scrollIntoView);
    LevelMO* selectedLevel() const;

    void refreshTabs();
    void refreshList();
    void refreshRows();
    void bindRow(Row& row, int index);
    void refreshDetail();
    void setListOffset(float offset);
    float maxListOffset() const;
    int rowAt(const cocos2d::Vec2& world) const;
    void setDescriptionOffset(float offset);
    void setPortrait(cocos2d::Sprite* sprite, int character, float fitHeight, float fitWidth);

    void goBack();
    void playSelected();
    void editSelected();
    void deleteSelected();
    void shareSelected();
    void sendSelected();
    void raceSelected();   // NET (PC addition): ghost race on this level
    void newLevel();
    void receive();

    cocos2d::Size _vs;
    cocos2d::Vec2 _origin;
    float _top = 0.0f;

    std::vector<cocos2d::RefPtr<LevelMO>> _levels;   // current tab, id_x ascending
    int _counts[2] = {0, 0};

    // header
    online::ui::Button* _tabs[2] = {nullptr, nullptr};

    // list
    cocos2d::Rect _rowsRect;
    float _rowH = 236.0f;
    cocos2d::Node* _rowsNode = nullptr;
    std::vector<Row> _rows;
    cocos2d::ui::Scale9Sprite* _scrollThumb = nullptr;
    cocos2d::Node* _listMessage = nullptr;
    int _hoverRow = -1;

    // detail
    cocos2d::Rect _detailRect;
    cocos2d::Node* _detail = nullptr;
    cocos2d::Node* _detailEmpty = nullptr;
    cocos2d::Label* _detailName = nullptr;
    float _detailNameMaxH = 0.0f;
    cocos2d::Sprite* _detailPortrait = nullptr;
    cocos2d::Node* _detailTag = nullptr;
    cocos2d::Label* _detailTagLabel = nullptr;
    cocos2d::Label* _detailChar = nullptr;
    cocos2d::Label* _detailCreated = nullptr;
    cocos2d::Label* _detailEdited = nullptr;
    cocos2d::Label* _detailSource = nullptr;
    cocos2d::Rect _descRect;
    cocos2d::Label* _desc = nullptr;
    float _descOffset = 0.0f;
    online::ui::Button* _playBtn = nullptr;

    // input
    enum class Drag { None, ListPending, List, Description };
    Drag _drag = Drag::None;
    cocos2d::Vec2 _touchStart;
    float _dragStartOffset = 0.0f;
    int _lastClickRow = -1;
    double _lastClickTime = 0.0;
    cocos2d::RefPtr<LevelMO> _pendingDelete;
};
