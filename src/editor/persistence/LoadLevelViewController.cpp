// LoadLevelViewController -- port of the iOS saved-level browser (see LoadLevelViewController.h).
#include "LoadLevelViewController.h"

#include "EditorLayer.h"
#include "GameText.h"
#include "HWWindow.h"
#include "LevelMO.h"
#include "LevelSession.h"
#include "LevelStore.h"
#include "LevelTextView.h"
#include "LevelUIHelpers.h"
#include "ShareAction.h"
#include "platform/common/Localization.h"

USING_NS_CC;

namespace {

enum
{
    kAlertTagDelete = 0,
    kAlertTagEdit = 1,
};

std::string L(const char* key)
{
    return Localization::get(key);
}

std::string capitalizedL(const char* key)
{
    return levelui::capitalized(Localization::get(key));
}

}  // namespace

LoadLevelViewController* LoadLevelViewController::create()
{
    LoadLevelViewController* controller = new (std::nothrow) LoadLevelViewController();
    if (controller && controller->initWithNibName("LoadLevelViewController"))
    {
        controller->autorelease();
        return controller;
    }
    delete controller;
    return nullptr;
}

LoadLevelViewController::LoadLevelViewController()
    : _levelNameLabel(nullptr),
      _descriptionTextView(nullptr),
      _menuNameLabel(nullptr),
      _tableView(nullptr),
      _cancelBtn(nullptr),
      _playBtn(nullptr),
      _editBtn(nullptr),
      _deleteBtn(nullptr),
      _shareLevelBtn(nullptr),
      _createNewLevelBtn(nullptr)
{
}

// @ios 100058468  (dealloc)
LoadLevelViewController::~LoadLevelViewController()
{
}

// @ios 100057964
bool LoadLevelViewController::initWithNibName(const std::string& nibName)
{
    return EditorViewController::initWithNibName(nibName);
}

// ---- actions ----------------------------------------------------------------------------------

// @ios 100057998
void LoadLevelViewController::createNewLevelBtnPressed(Ref* sender)
{
    LevelSession::getInstance()->clearLevelData();
    close();
    // Port: on iOS the editor menu stays presented over the new editor (with a dangling
    // editorLayer); the port's views belong to the old scene, so dismiss the menu as well.
    EditorViewController::dismissRootPresented(false, nullptr);
    Director::getInstance()->replaceScene(EditorLayer::createScene());
}

// @ios 1000579f0
void LoadLevelViewController::cancelBtnPressed(Ref* sender)
{
    close();
}

// @ios 1000579f4
void LoadLevelViewController::playBtnPressed(Ref* sender)
{
    LevelIndexPath indexPath = _tableView->indexPathForSelectedRow();
    LevelMO* level = levelAtIndexPath(indexPath);
    if (!level)
    {
        return;
    }
    LevelSession* session = LevelSession::getInstance();
    session->setChapterIndex(indexPath.section == 1 ? LevelStoreChapterImported : LevelStoreChapterUser);
    session->setLevelIndex(level->id_x());
    session->setLevelDataWithManagedObject(level);
    bool forceCharacter = level->force_character();
    session->setCharacterIndex(level->playable_character());
    session->setVehicleIndex(0);
    // pushScene(force ? [GameplayLayer scene] : [CharacterSelectLayer scene])
    session->playLevel(forceCharacter);
    EditorViewController::dismissRootPresented(true, nullptr);
}

// @ios 100057b2c
void LoadLevelViewController::editBtnPressed(Ref* sender)
{
    LevelMO* level = levelAtIndexPath(_tableView->indexPathForSelectedRow());
    if (level && !level->levelTimes().empty())
    {
        showAlert(kAlertTagEdit, "Hey", OW_IOSTEXT(editorEditDeletesBestTimes, 0x1015dcf40), "No", "Yes");
        return;
    }
    confirmEditLevel();
}

// @ios 100057c10
void LoadLevelViewController::confirmEditLevel()
{
    // iOS indexes _levels whatever the selected section (EDIT is disabled for section 1).
    LevelIndexPath indexPath = _tableView->indexPathForSelectedRow();
    if (!indexPath.isValid() || indexPath.row >= static_cast<long>(_levels.size()))
    {
        return;
    }
    LevelMO* level = _levels.at(indexPath.row);
    Director::getInstance()->pushScene(EditorLayer::createSceneWithLevelMO(level));
    EditorViewController::dismissRootPresented(true, nullptr);
}

// @ios 100057c98
void LoadLevelViewController::deleteBtnPressed(Ref* sender)
{
    LevelMO* level = levelAtIndexPath(_tableView->indexPathForSelectedRow());
    if (!level)
    {
        return;
    }
    std::string title = StringUtils::format("%s \"%s\"", L("DELETE THE LEVEL").c_str(), level->name().c_str());
    showAlert(kAlertTagDelete, title, L("THIS CANT BE UNDONE"), capitalizedL("CANCEL"), capitalizedL("OK"));
}

// @ios 100057e1c
void LoadLevelViewController::shareBtnPressed(Ref* sender)
{
    // iOS indexes _levels whatever the selected section (SHARE is disabled for section 1).
    LevelIndexPath indexPath = _tableView->indexPathForSelectedRow();
    if (!indexPath.isValid() || indexPath.row >= static_cast<long>(_levels.size()))
    {
        return;
    }
    LevelMO* level = _levels.at(indexPath.row);
    ShareAction::shareLevelDataFile(level->data(), level->playable_character(), level->force_character(),
                                    level->name(), "", level->comments(), this);
}

// ---- UIViewController -------------------------------------------------------------------------

// @ios 1000583c0
void LoadLevelViewController::viewDidLoad()
{
    EditorViewController::viewDidLoad();
    // setContentSizeForViewInPopover:(480, 320): the editor-menu popover is that size.

    // Port: the nib's view hierarchy, autolayout resolved for the window (see the header).
    // Laid out in the controller's own frame: the window, or the editor-menu popover it covers
    // (iPad UIModalPresentationCurrentContext; frames are set before viewDidLoad).
    Size win = frame().size;
    float W = win.width;
    float H = win.height;
    float panelH = H - 60.0f;
    setBackgroundColor(uikit::color(0.706, 0.706, 0.706, 1.0));

    uikit::View* panel = uikit::View::create(Rect(0.0f, 0.0f, W, panelH));
    panel->setBackgroundColor(uikit::color(0.8, 0.8, 0.8, 1.0));
    addSubview(panel);

    float tableW = W - 258.0f;
    float columnX = tableW + 8.0f;
    _tableView = LevelListView::create(Size(tableW, panelH), this);
    panel->addSubview(_tableView, Rect(0.0f, 0.0f, tableW, panelH));

    float buttonsY = panelH - 8.0f - 40.0f;
    _deleteBtn = levelui::makeButton("[DEL]", 15.0f, CC_CALLBACK_1(LoadLevelViewController::deleteBtnPressed, this));
    panel->addSubview(_deleteBtn, Rect(columnX, buttonsY, 75.0f, 40.0f));
    _editBtn = levelui::makeButton("[EDIT]", 15.0f, CC_CALLBACK_1(LoadLevelViewController::editBtnPressed, this));
    panel->addSubview(_editBtn, Rect(columnX + 83.0f, buttonsY, 75.0f, 40.0f));
    _shareLevelBtn = levelui::makeButton("[SHARE]", 15.0f, CC_CALLBACK_1(LoadLevelViewController::shareBtnPressed, this));
    panel->addSubview(_shareLevelBtn, Rect(columnX + 166.0f, buttonsY, 76.0f, 40.0f));

    float playY = buttonsY - 8.0f - 40.0f;
    _playBtn = levelui::makeButton("[PLAY]", 19.0f, CC_CALLBACK_1(LoadLevelViewController::playBtnPressed, this));
    panel->addSubview(_playBtn, Rect(columnX, playY, 242.0f, 40.0f));

    float descriptionY = playY - 8.0f - 119.0f;
    _descriptionTextView = LevelTextView::create(Rect(columnX, descriptionY, 242.0f, 119.0f), 14.0f, false);
    _descriptionTextView->setBackgroundColor(uikit::color(0.697, 0.697, 0.697, 1.0));
    _descriptionTextView->setTextColor(uikit::whiteColor());
    panel->addSubview(_descriptionTextView);

    _levelNameLabel = levelui::makeLabel("", 17.0f, false, 0, uikit::whiteColor());
    panel->addSubview(_levelNameLabel, Rect(columnX, descriptionY - 8.0f - 21.0f, 242.0f, 21.0f));

    _menuNameLabel = levelui::makeLabel("", 17.0f, false, 0, uikit::whiteColor());
    addSubview(_menuNameLabel, Rect(10.0f, H - 20.0f - 21.0f, 242.0f, 21.0f));
    _cancelBtn = levelui::makeButton("[CANCEL]", 15.0f, CC_CALLBACK_1(LoadLevelViewController::cancelBtnPressed, this));
    addSubview(_cancelBtn, Rect(W - 10.0f - 117.0f, panelH + 10.0f, 117.0f, 40.0f));
    _createNewLevelBtn = levelui::makeButton("[NEW LEVEL]", 15.0f,
                                             CC_CALLBACK_1(LoadLevelViewController::createNewLevelBtnPressed, this));
    addSubview(_createNewLevelBtn, Rect(W - 10.0f - 117.0f - 8.0f - 117.0f, panelH + 10.0f, 117.0f, 40.0f));
}

// @ios 100057ef0
void LoadLevelViewController::viewWillAppear(bool animated)
{
    EditorViewController::viewWillAppear(animated);
    _menuNameLabel->setString(capitalizedL("YOUR LEVELS"));
    _levelNameLabel->setString("");
    _descriptionTextView->setText("");
    skinButton(_cancelBtn, 0);
    levelui::setButtonTitle(_cancelBtn, capitalizedL("CANCEL"));
    skinButton(_createNewLevelBtn, 2);
    levelui::setButtonTitle(_createNewLevelBtn, capitalizedL("NEW LEVEL"));
    skinButton(_playBtn, 0);
    levelui::setButtonTitle(_playBtn, capitalizedL("PLAY"));
    skinButton(_editBtn, 1);
    levelui::setButtonTitle(_editBtn, capitalizedL("EDIT"));
    skinButton(_deleteBtn, 1);
    levelui::setButtonTitle(_deleteBtn, capitalizedL("DELETE"));
    skinButton(_shareLevelBtn, 1);
    levelui::setButtonTitle(_shareLevelBtn, capitalizedL("SHARE"));

    // ChapterMO "chapterIndex == 5000" / "== 5001", levels sorted by id_x.
    LevelStore* store = LevelStore::getInstance();
    _levels = store->levelsSortedById(LevelStoreChapterUser);
    _importedLevels = store->levelsSortedById(LevelStoreChapterImported);
    _tableView->reloadData();   // UITableView loads its rows when it appears
    displayLevelInfo(LevelIndexPath());
}

// @ios 100058418
void LoadLevelViewController::didReceiveMemoryWarning()
{
    EditorViewController::didReceiveMemoryWarning();
}

// @ios 10005844c
void LoadLevelViewController::close()
{
    if (EditorViewController* presenting = presentingViewController())
    {
        presenting->dismissViewControllerAnimated(true, nullptr);
    }
    else
    {
        dismissViewControllerAnimated(true, nullptr);
    }
}

// ---- alerts -----------------------------------------------------------------------------------

void LoadLevelViewController::showAlert(int tag, const std::string& title, const std::string& message,
                                        const std::string& cancelTitle, const std::string& otherTitle)
{
    if (levelui::showAlert(tag, title, message, cancelTitle, otherTitle, this))
    {
        retain();  // released in hwWindowWasDismissed
    }
}

void LoadLevelViewController::hwWindowButtonPressed(int buttonTag, HWWindow* window)
{
    alertViewClickedButtonAtIndex(window->getTag(), levelui::buttonIndex(buttonTag, window));
}

void LoadLevelViewController::hwWindowWasDismissed(HWWindow* window)
{
    release();
}

// @ios 100058568
void LoadLevelViewController::alertViewClickedButtonAtIndex(int alertTag, long buttonIndex)
{
    if (alertTag == kAlertTagEdit)
    {
        if (buttonIndex == 1)
        {
            confirmEditLevel();
        }
    }
    else if (alertTag == kAlertTagDelete && buttonIndex == 1)
    {
        LevelIndexPath indexPath = _tableView->indexPathForSelectedRow();
        Vector<LevelMO*>& levels = indexPath.section != 0 ? _importedLevels : _levels;
        if (!indexPath.isValid() || indexPath.row >= static_cast<long>(levels.size()))
        {
            return;
        }
        RefPtr<LevelMO> level = levels.at(indexPath.row);
        levels.erase(indexPath.row);
        LevelStore* store = LevelStore::getInstance();
        store->deleteObject(level.get());
        store->save(nullptr);
        _tableView->deleteRow(indexPath);
        displayLevelInfo(LevelIndexPath());
    }
}

// ---- UITableView ------------------------------------------------------------------------------

// @ios 100058708
long LoadLevelViewController::numberOfSectionsInTableView(ui::ListView* tableView)
{
    return 2;
}

// @ios 100058710
long LoadLevelViewController::tableViewNumberOfRowsInSection(ui::ListView* tableView, long section)
{
    return static_cast<long>((section != 0 ? _importedLevels : _levels).size());
}

// @ios 100058734
std::string LoadLevelViewController::tableViewTitleForRow(ui::ListView* tableView, long section, long row)
{
    const Vector<LevelMO*>& levels = section != 0 ? _importedLevels : _levels;
    return row < static_cast<long>(levels.size()) ? levels.at(row)->name() : std::string();
}

// @ios 1000587d4
bool LoadLevelViewController::tableViewCanEditRow(ui::ListView* tableView, long section, long row)
{
    return false;
}

// @ios 1000587dc
void LoadLevelViewController::tableViewDidSelectRow(ui::ListView* tableView, long section, long row)
{
    displayLevelInfo(LevelIndexPath(section, row));
}

// @ios 1000587e4
void LoadLevelViewController::displayLevelInfo(const LevelIndexPath& indexPath)
{
    LevelMO* level = indexPath.isValid() ? levelAtIndexPath(indexPath) : nullptr;
    if (!level)
    {
        _levelNameLabel->setString("--");
        _descriptionTextView->setText(L("SELECT A LEVEL"));
        _shareLevelBtn->setVisible(false);
        _playBtn->setVisible(false);
        _editBtn->setVisible(false);
        _deleteBtn->setVisible(false);
    }
    else
    {
        _levelNameLabel->setString(level->name());
        std::string comments = level->comments();
        if (comments.empty())
        {
            comments = L("NO LEVEL DESCRIPTION");
        }
        _descriptionTextView->setText(comments);
        _shareLevelBtn->setVisible(true);
        _playBtn->setVisible(true);
        _editBtn->setVisible(true);
        _deleteBtn->setVisible(true);
        bool userSection = indexPath.section != 1;
        levelui::setButtonEnabled(_editBtn, userSection);
        levelui::setButtonEnabled(_shareLevelBtn, userSection);
    }
    _descriptionTextView->setTextColor(uikit::whiteColor());
}

// @ios 100058a04
ui::Widget* LoadLevelViewController::tableViewViewForHeaderInSection(ui::ListView* tableView, long section)
{
    float scale = uikit::pointsToDesign();
    const float headerHeight = 22.0f;   // plain UITableView section header
    ui::Layout* header = ui::Layout::create();
    header->setContentSize(Size(tableView->getContentSize().width, headerHeight * scale));
    header->setBackGroundColorType(ui::Layout::BackGroundColorType::SOLID);
    Color4B background = uikit::color(0.23921568627451, 0.53333333333333, 0.73725490196078, 0.75);
    header->setBackGroundColor(Color3B(background));
    header->setBackGroundColorOpacity(background.a);
    // UILabel at (5, 2), boldSystemFontOfSize:12, white on clear.
    ui::Text* label = levelui::makeLabel(L(section != 0 ? "IMPORTED LEVELS" : "YOUR LEVELS"), 12.0f, true, 0,
                                         uikit::color(1.0, 1.0, 1.0, 1.0));
    label->setContentSize(Size(tableView->getContentSize().width - 5.0f * scale, (headerHeight - 4.0f) * scale));
    label->setAnchorPoint(Vec2(0.0f, 1.0f));
    label->setPosition(Vec2(5.0f * scale, (headerHeight - 2.0f) * scale));
    header->addChild(label);
    return header;
}

// ---- properties -------------------------------------------------------------------------------

ui::Text* LoadLevelViewController::levelNameLabel() const { return _levelNameLabel; }              // @ios 100058b68
void LoadLevelViewController::setLevelNameLabel(ui::Text* label) { _levelNameLabel = label; }      // @ios 100058b78
LevelTextView* LoadLevelViewController::descriptionTextView() const { return _descriptionTextView; }  // @ios 100058b84
void LoadLevelViewController::setDescriptionTextView(LevelTextView* textView) { _descriptionTextView = textView; }  // @ios 100058b94
ui::Text* LoadLevelViewController::menuNameLabel() const { return _menuNameLabel; }                // @ios 100058ba0
void LoadLevelViewController::setMenuNameLabel(ui::Text* label) { _menuNameLabel = label; }        // @ios 100058bb0
LevelListView* LoadLevelViewController::tableView() const { return _tableView; }                   // @ios 100058bbc
void LoadLevelViewController::setTableView(LevelListView* tableView) { _tableView = tableView; }    // @ios 100058bcc
ui::Button* LoadLevelViewController::cancelBtn() const { return _cancelBtn; }                      // @ios 100058bd8
void LoadLevelViewController::setCancelBtn(ui::Button* button) { _cancelBtn = button; }            // @ios 100058be8
ui::Button* LoadLevelViewController::playBtn() const { return _playBtn; }                          // @ios 100058bf4
void LoadLevelViewController::setPlayBtn(ui::Button* button) { _playBtn = button; }                // @ios 100058c04
ui::Button* LoadLevelViewController::editBtn() const { return _editBtn; }                          // @ios 100058c10
void LoadLevelViewController::setEditBtn(ui::Button* button) { _editBtn = button; }                // @ios 100058c20
ui::Button* LoadLevelViewController::deleteBtn() const { return _deleteBtn; }                      // @ios 100058c2c
void LoadLevelViewController::setDeleteBtn(ui::Button* button) { _deleteBtn = button; }            // @ios 100058c3c
ui::Button* LoadLevelViewController::shareLevelBtn() const { return _shareLevelBtn; }              // @ios 100058c48
void LoadLevelViewController::setShareLevelBtn(ui::Button* button) { _shareLevelBtn = button; }    // @ios 100058c58
ui::Button* LoadLevelViewController::createNewLevelBtn() const { return _createNewLevelBtn; }      // @ios 100058c64
void LoadLevelViewController::setCreateNewLevelBtn(ui::Button* button) { _createNewLevelBtn = button; }  // @ios 100058c74

// ---- port helpers ---------------------------------------------------------------------------------

LevelMO* LoadLevelViewController::levelAtIndexPath(const LevelIndexPath& indexPath)
{
    if (!indexPath.isValid())
    {
        return nullptr;
    }
    // play/edit/delete: section == 1 -> _importedLevels, otherwise _levels
    const Vector<LevelMO*>& levels = indexPath.section == 1 ? _importedLevels : _levels;
    return indexPath.row < static_cast<long>(levels.size()) ? levels.at(indexPath.row) : nullptr;
}
