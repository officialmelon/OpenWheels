// UserLevelSelectUIView -- the user-level picker (see UserLevelSelectUIView.h; dead on iOS,
// revived by the port with three documented fixes marked "iOS bug").
#include "UserLevelSelectUIView.h"

#include "EditorLayer.h"
#include "Globals.h"
#include "HWWindow.h"
#include "LevelMO.h"
#include "LevelSession.h"
#include "LevelStore.h"
#include "LevelTextView.h"
#include "LevelUIHelpers.h"
#include "MainMenu.h"
#include "platform/common/Localization.h"
#include "UserLevelsScreen.h"  // UI (PC addition): restyled

USING_NS_CC;

namespace {

std::string L(const char* key)
{
    return Localization::get(key);
}

std::string capitalizedL(const char* key)
{
    return levelui::capitalized(Localization::get(key));
}

}  // namespace

UserLevelSelectUIView* UserLevelSelectUIView::create(const Rect& frame)
{
    UserLevelSelectUIView* view = new (std::nothrow) UserLevelSelectUIView();
    if (view && view->initWithFrame(frame))
    {
        view->autorelease();
        return view;
    }
    delete view;
    return nullptr;
}

Scene* UserLevelSelectUIView::scene()
{
    // UI (PC addition): restyled - the modern screen replaces this panel as the main menu's entry.
    return UserLevelsScreen::createScene();
}

UserLevelSelectUIView::UserLevelSelectUIView()
    : tableView(nullptr),
      nameLabel(nullptr),
      descriptionText(nullptr),
      editLevelBtn(nullptr),
      playLevelBtn(nullptr),
      deleteBtn(nullptr)
{
}

// @ios 100107028  (dealloc)
UserLevelSelectUIView::~UserLevelSelectUIView()
{
    CC_SAFE_RELEASE(_scrollView);   // kept alive after removeFromSuperview in initWithFrame:
}

// @ios 100106bb8
bool UserLevelSelectUIView::initWithFrame(const Rect& frame)
{
    if (!EditorUIView::initWithFrame(frame))
    {
        return false;
    }
    // Swap the scroll view for a plain view with the same frame and background colour.
    Rect scrollFrame = _scrollView->frame();
    Color4B background(_scrollView->getBackGroundColor(), _scrollView->getBackGroundColorOpacity());
    _scrollView->retain();   // iOS keeps the ivar pointing at the removed view
    _scrollView->removeFromSuperview();
    uikit::View* plain = uikit::View::create(scrollFrame);
    plain->setBackgroundColor(background);
    addSubview(plain);

    // ChapterMO "chapterIndex == 5000" valueForKey:@"levels", sorted by id_x.
    levels = LevelStore::getInstance()->levelsSortedById(LevelStoreChapterUser);

    Rect tableFrame(scrollFrame.origin.x, scrollFrame.origin.y, scrollFrame.size.width * 0.5,
                    scrollFrame.size.height);
    tableView = LevelListView::create(tableFrame.size, this);
    addSubview(tableView, tableFrame);
    tableView->reloadData();

    Size winSize = uikit::windowSize();
    Rect closeFrame = subviewFrame(_closeBtn);
    double newX = closeFrame.origin.x - (static_cast<double>(_space) + static_cast<double>(_btnWidth << 1));
    Rect newFrame(static_cast<float>(newX), closeFrame.origin.y, static_cast<float>(_btnWidth << 1),
                  static_cast<float>(_btnHeight));
    ui::Button* newLevelBtn = EditorUIView::blueButtonWithTitle(capitalizedL("NEW LEVEL"), newFrame);
    newLevelBtn->addClickEventListener(CC_CALLBACK_1(UserLevelSelectUIView::newLevelBtnPressed, this));
    addSubview(newLevelBtn, newFrame);

    double halfWidth = winSize.width * 0.5;
    float labelX = static_cast<float>(halfWidth + _space);
    float labelW = static_cast<float>(halfWidth - static_cast<double>(_space * 3));
    float labelY = static_cast<float>(_space * 2);
    float labelH = static_cast<float>(_btnHeight);
    nameLabel = levelui::makeLabel("", 17.0f, false, 0, uikit::blackColor());   // UILabel defaults
    addSubview(nameLabel, Rect(labelX, labelY, labelW, labelH));
    nameLabel->setString("");

    float descriptionY = static_cast<float>(labelH + labelY);   // nameLabel.frame.size.height + 2*space
    descriptionText = LevelTextView::create(Rect(labelX, descriptionY, labelW, labelH), 12.0f, false);
    descriptionText->setText("");
    descriptionText->setBackgroundColor(uikit::clearColor());
    addSubview(descriptionText);
    return true;
}

// @ios 100107078
void UserLevelSelectUIView::newLevelBtnPressed(Ref* sender)
{
    Director::getInstance()->pushScene(EditorLayer::createScene());
    removeFromSuperview();
}

// @ios 1001070c0
void UserLevelSelectUIView::deleteBtnPressed(Ref* sender)
{
    LevelMO* level = selectedLevel();
    if (!level)
    {
        return;
    }
    std::string title = StringUtils::format("%s \"%s\"", L("DELETE THE LEVEL").c_str(), level->name().c_str());
    if (levelui::showAlert(0, title, L("THIS CANT BE UNDONE"), capitalizedL("CANCEL"), capitalizedL("OK"), this))
    {
        retain();  // released in hwWindowWasDismissed
    }
}

// @ios 100107220
void UserLevelSelectUIView::editBtnPressed(Ref* sender)
{
    LevelMO* level = selectedLevel();
    if (!level)
    {
        return;
    }
    LevelSession::getInstance()->setLevelDataWithManagedObject(level);
    // iOS bug: pushes [EditorLayer scene], i.e. an EMPTY editor (no levelMO). The port opens the
    // selected level, like -[LoadLevelViewController confirmEditLevel].
    Director::getInstance()->pushScene(EditorLayer::createSceneWithLevelMO(level));
    removeFromSuperview();
}

// @ios 1001072ac
void UserLevelSelectUIView::playBtnPressed(Ref* sender)
{
    LevelMO* level = selectedLevel();
    if (!level)
    {
        return;
    }
    LevelSession* session = LevelSession::getInstance();
    session->setChapterIndex(LevelStoreChapterUser);
    session->setLevelDataWithManagedObject(level);
    bool forceCharacter = level->force_character();
    session->setCharacterIndex(level->playable_character());
    session->setVehicleIndex(0);
    session->playLevel(forceCharacter);
    removeFromSuperview();
}

void UserLevelSelectUIView::hwWindowButtonPressed(int buttonTag, HWWindow* window)
{
    alertViewClickedButtonAtIndex(window->getTag(), levelui::buttonIndex(buttonTag, window));
}

void UserLevelSelectUIView::hwWindowWasDismissed(HWWindow* window)
{
    release();
}

// @ios 100107398
void UserLevelSelectUIView::alertViewClickedButtonAtIndex(int alertTag, long buttonIndex)
{
    if (buttonIndex != 1)
    {
        return;
    }
    LevelIndexPath indexPath = tableView->indexPathForSelectedRow();
    if (!indexPath.isValid() || indexPath.row >= static_cast<long>(levels.size()))
    {
        return;
    }
    RefPtr<LevelMO> level = levels.at(indexPath.row);
    levels.erase(indexPath.row);
    LevelStore* store = LevelStore::getInstance();
    store->deleteObject(level.get());
    // iOS bug: the context was never saved here, so the deletion only reached the store with the
    // next save anywhere (or never). The port saves.
    store->save(nullptr);
    tableView->deleteRow(indexPath);
    displayLevelInfo(LevelIndexPath());
}

// @ios 1001074ac
long UserLevelSelectUIView::tableViewNumberOfRowsInSection(ui::ListView* listView, long section)
{
    return static_cast<long>(levels.size());
}

// @ios 1001074bc
std::string UserLevelSelectUIView::tableViewTitleForRow(ui::ListView* listView, long section, long row)
{
    return row < static_cast<long>(levels.size()) ? levels.at(row)->name() : std::string();
}

// @ios 100107548
bool UserLevelSelectUIView::tableViewCanEditRow(ui::ListView* listView, long section, long row)
{
    return false;
}

// @ios 100107550
bool UserLevelSelectUIView::tableViewCanMoveRow(ui::ListView* listView, long section, long row)
{
    return true;
}

// @ios 100107558
void UserLevelSelectUIView::tableViewDidSelectRow(ui::ListView* listView, long section, long row)
{
    displayLevelInfo(LevelIndexPath(section, row));
}

// @ios 100107560
void UserLevelSelectUIView::displayLevelInfo(const LevelIndexPath& indexPath)
{
    if (!indexPath.isValid())
    {
        if (deleteBtn)
        {
            nameLabel->setString("");
            descriptionText->setText("");
            deleteBtn->removeFromParent();
            deleteBtn = nullptr;
            playLevelBtn->removeFromParent();
            playLevelBtn = nullptr;
            editLevelBtn->removeFromParent();
            editLevelBtn = nullptr;
        }
        return;
    }
    if (indexPath.row >= static_cast<long>(levels.size()))
    {
        return;
    }
    LevelMO* level = levels.at(indexPath.row);
    nameLabel->setString(level->name());
    // iOS bug: valueForKey:@"descriptionText" (no such key -> NSUnknownKeyException).
    descriptionText->setText(level->comments());
    if (deleteBtn)
    {
        return;
    }
    Size winSize = uikit::windowSize();
    unsigned int space = _space;
    int space2 = static_cast<int>(space * 2);
    double btnHeight = static_cast<double>(_btnHeight);
    float spaceF = static_cast<float>(space);
    float columnW = static_cast<float>((winSize.width - static_cast<double>(space2)) * 0.5);
    float smallW = (columnW - static_cast<float>(space2 + static_cast<int>(space))) * 0.5f;
    double x = static_cast<double>(columnW + static_cast<float>(space2));
    double y = static_cast<double>(static_cast<float>(
        winSize.height - static_cast<double>(space2 + static_cast<int>(space) + _btnHeight * 2)));

    Rect playFrame(static_cast<float>(x), static_cast<float>(y), static_cast<float>(columnW - (spaceF + spaceF)),
                   static_cast<float>(btnHeight));
    playLevelBtn = EditorUIView::pinkButtonWithTitle(capitalizedL("PLAY LEVEL"), playFrame);
    playLevelBtn->addClickEventListener(CC_CALLBACK_1(UserLevelSelectUIView::playBtnPressed, this));
    addSubview(playLevelBtn, playFrame);

    y = y - static_cast<double>(_space + _btnHeight);
    Rect deleteFrame(static_cast<float>(x), static_cast<float>(y), smallW, static_cast<float>(btnHeight));
    deleteBtn = EditorUIView::blueButtonWithTitle(capitalizedL("DELETE LEVEL"), deleteFrame);
    deleteBtn->addClickEventListener(CC_CALLBACK_1(UserLevelSelectUIView::deleteBtnPressed, this));
    addSubview(deleteBtn, deleteFrame);

    float spaceAgain = static_cast<float>(_space);
    Rect editFrame(static_cast<float>(static_cast<double>(smallW + spaceAgain) + x), static_cast<float>(y), smallW,
                   static_cast<float>(btnHeight));
    editLevelBtn = EditorUIView::blueButtonWithTitle(capitalizedL("EDIT LEVEL"), editFrame);
    editLevelBtn->addClickEventListener(CC_CALLBACK_1(UserLevelSelectUIView::editBtnPressed, this));
    addSubview(editLevelBtn, editFrame);
}

LevelMO* UserLevelSelectUIView::selectedLevel()
{
    LevelIndexPath indexPath = tableView->indexPathForSelectedRow();
    if (!indexPath.isValid() || indexPath.row >= static_cast<long>(levels.size()))
    {
        return nullptr;
    }
    return levels.at(indexPath.row);
}
