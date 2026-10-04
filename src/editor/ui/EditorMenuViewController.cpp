#include "EditorMenuViewController.h"

#include "CharacterRef.h"
#include "EditorLayer.h"
#include "HWWindow.h"
#include "LevelMO.h"
#include "LevelUIHelpers.h"
#include "LevelSession.h"
#include "LoadLevelViewController.h"
#include "MainMenu.h"
#include "SaveLevelViewController.h"
#include "ShareAction.h"
#include "net/NearbyPanels.h"  // NET (PC addition)
#include "Special.h"
#include "platform/common/Localization.h"

USING_NS_CC;

namespace {
const int kModalPresentationCurrentContext = 3;
const float kPopoverWidth = 480.0f;   // setContentSizeForViewInPopover:
const float kPopoverHeight = 320.0f;

std::string capitalizedKey(const char* key) { return uikit::capitalizedString(Localization::get(key)); }

void setControlEnabled(ui::Button* button, bool enabled)
{
    button->setEnabled(enabled);
    button->setCascadeOpacityEnabled(true);
    button->setOpacity(enabled ? 255 : 90);  // UIButton disabled title dimming
}
}  // namespace

EditorMenuViewController::EditorMenuViewController()
    : _levelMO(nullptr), _loadLevelViewController(nullptr), _editorLayer(nullptr), _popoverController(nullptr),
      _saveBtn(nullptr), _loadBtn(nullptr), _createNewLevelBtn(nullptr), _closeBtn(nullptr), _lockBtn(nullptr),
      _unlockBtn(nullptr), _mainMenuBtn(nullptr), _shareBtn(nullptr), _rotateSwitch(nullptr), _rotateSwitchLabel(nullptr),
      _pasteInPlaceSwitch(nullptr), _pasteInPlaceLabel(nullptr), _snapRotLabel(nullptr), _snapRotSwitch(nullptr),
      _centerToCharBtn(nullptr), _menuNameLabel(nullptr)
{
}

// @ios 10010c308 (dealloc)
EditorMenuViewController::~EditorMenuViewController()
{
    uikit::NotificationCenter::removeObserver(this);
    CC_SAFE_RELEASE_NULL(_levelMO);
    CC_SAFE_RELEASE_NULL(_loadLevelViewController);
    // nib top-level objects outside the view hierarchy
    CC_SAFE_RELEASE_NULL(_snapRotLabel);
    CC_SAFE_RELEASE_NULL(_snapRotSwitch);
}

EditorMenuViewController* EditorMenuViewController::create(const std::string& nibName, LevelMO* levelMO)
{
    EditorMenuViewController* vc = new (std::nothrow) EditorMenuViewController();
    if (vc && vc->initWithNibName(nibName, levelMO))
    {
        vc->autorelease();
        return vc;
    }
    delete vc;
    return nullptr;
}

EditorMenuViewController* EditorMenuViewController::create(LevelMO* levelMO)
{
    return create("EditorMenuViewController", levelMO);
}

// @ios 10010bbe0
bool EditorMenuViewController::initWithNibName(const std::string& nibName, LevelMO* levelMO)
{
    if (!EditorViewController::initWithNibName(nibName)) return false;
    CC_SAFE_RETAIN(levelMO);
    _levelMO = levelMO;
    return true;
}

// @ios 10010bc40
void EditorMenuViewController::viewDidLoad()
{
    EditorViewController::viewDidLoad();
    setContentSizeForViewInPopover(Size(kPopoverWidth, kPopoverHeight));
    loadNibLayout();
}

void EditorMenuViewController::loadNibLayout()
{
    // EditorMenuViewController.nib (objects-11.0+), constraints resolved for the current bounds.
    const float W = frame().size.width, H = frame().size.height;
    setBackgroundColor(uikit::color(0.7058823529, 0.7058823529, 0.7058823529, 1.0));
    const float cw = W - 5.0f;  // container trailing = safe area - 5
    uikit::View* container = uikit::View::create(Rect(0, 0, cw, H - 60.0f));
    container->setBackgroundColor(uikit::color(0.8, 0.8, 0.8, 1.0));
    container->setUserInteractionEnabled(false);
    addSubview(container);

    auto makeButton = [](const std::function<void(Ref*)>& action) {
        ui::Button* button = ui::Button::create();
        uikit::setButtonTitle(button, "", 15.0f, uikit::color(0.0, 0.478, 1.0, 1.0));
        button->addClickEventListener(action);
        return button;
    };
    // left column (fixed, autoresizing)
    _saveBtn = makeButton([this](Ref* s) { saveBtnPressed(s); });
    container->addSubview(_saveBtn, Rect(9, 8, 160, 40));
    _createNewLevelBtn = makeButton([this](Ref* s) { createNewLevelBtnPressed(s); });
    container->addSubview(_createNewLevelBtn, Rect(9, 56, 160, 40));
    _loadBtn = makeButton([this](Ref* s) { loadBtnPressed(s); });
    container->addSubview(_loadBtn, Rect(9, 104, 160, 40));
    _shareBtn = makeButton([this](Ref* s) { shareBtnPressed(s); });
    container->addSubview(_shareBtn, Rect(9, 152, 160, 40));
    // NET (PC addition): SEND NEARBY in the free middle column, level with SHARE LEVEL.
    _sendNearbyBtn = makeButton([this](Ref* s) { sendNearbyBtnPressed(s); });
    container->addSubview(_sendNearbyBtn, Rect(177, 152, cw - 12.0f - 160.0f - 8.0f - 177.0f, 40));
    _mainMenuBtn = makeButton([this](Ref* s) { exitEditorBtnPressed(s); });
    container->addSubview(_mainMenuBtn, Rect(9, 201, 160, 38));

    // right column (trailing-anchored)
    const float rightX = cw - 12.0f - 160.0f;
    const float switchX = cw - 20.0f - uikit::Switch::kWidth;
    const float labelX = switchX - 8.0f - 75.0f;
    _lockBtn = makeButton([this](Ref* s) { lockBtnPressed(s); });
    container->addSubview(_lockBtn, Rect(rightX, 8, 160, 40));
    _unlockBtn = makeButton([this](Ref* s) { unlockBtnPressed(s); });
    container->addSubview(_unlockBtn, Rect(rightX, 56, 160, 40));
    const float rotateY = 56.0f + 40.0f + 12.0f;  // unlockBtn.bottom + 12
    _rotateSwitchLabel = uikit::makeLabel("", false, 17.0f, 1);
    container->addSubview(_rotateSwitchLabel, Rect(labelX, rotateY, 75, 31));
    _rotateSwitch = uikit::Switch::create();
    _rotateSwitch->setCallback([this](uikit::Switch* s) { rotateSwitchValueChanged(s); });
    container->addSubview(_rotateSwitch, Rect(switchX, rotateY, uikit::Switch::kWidth, uikit::Switch::kHeight));
    const float pasteLabelY = rotateY + 31.0f + 12.0f;
    _pasteInPlaceLabel = uikit::makeLabel("", false, 17.0f, 1);
    container->addSubview(_pasteInPlaceLabel, Rect(labelX, pasteLabelY, 75, 31));
    _pasteInPlaceSwitch = uikit::Switch::create();
    _pasteInPlaceSwitch->setCallback([this](uikit::Switch* s) { pasteInPlaceBtnPressed(s); });
    container->addSubview(_pasteInPlaceSwitch,
                          Rect(switchX, rotateY + 31.0f + 13.0f, uikit::Switch::kWidth, uikit::Switch::kHeight));
    _centerToCharBtn = makeButton([this](Ref* s) { centerToCharBtnPressed(s); });
    container->addSubview(_centerToCharBtn, Rect(rightX, pasteLabelY + 31.0f + 18.0f, 160, 40));

    // bottom bar
    const float nameY = H - 19.0f - 21.0f;
    _menuNameLabel = uikit::makeLabel("", true, 17.0f, 0);
    _menuNameLabel->setTextColor(uikit::whiteColor());
    addSubview(_menuNameLabel, Rect(10, nameY, 242, 21));
    _closeBtn = makeButton([this](Ref* s) { closeMenuBtnPressed(s); });
    addSubview(_closeBtn, Rect(W - 160.0f, nameY + 10.5f - 19.5f, 160, 39));

    // Snap-rotation controls: nib objects outside the view hierarchy (never visible).
    _snapRotLabel = uikit::makeLabel("", false, 17.0f, 1);
    _snapRotLabel->retain();
    _snapRotSwitch = uikit::Switch::create();
    _snapRotSwitch->retain();
}

// @ios 10010bc98
void EditorMenuViewController::viewWillAppear(bool animated)
{
    EditorViewController::viewWillAppear(animated);
    // [[HWTracker sharedInstance] setCategory:@"editor_level_menu"] - analytics, dropped on PC.
    menuNameLabel()->setString(capitalizedKey("EDITOR MENU"));
    struct Skin
    {
        ui::Button* button;
        int style;
        const char* key;
    };
    const Skin skins[] = {{saveBtn(), 1, "SAVE LEVEL"},       {loadBtn(), 1, "LOAD LEVEL"},
                          {shareBtn(), 1, "SHARE LEVEL"},     {createNewLevelBtn(), 1, "NEW LEVEL"},
                          {mainMenuBtn(), 0, "MAIN MENU"},    {closeBtn(), 0, "CLOSE MENU"}};
    for (const Skin& s : skins)
    {
        skinButton(s.button, s.style);
        s.button->setTitleText(capitalizedKey(s.key));
    }
    skinButton(_sendNearbyBtn, 1);  // NET (PC addition)
    _sendNearbyBtn->setTitleText(capitalizedKey("SEND NEARBY"));
    const ssize_t lockedCount = editorLayer()->lockedRefs().size();
    skinButton(lockBtn(), 1);
    lockBtn()->setTitleText(capitalizedKey("LOCK SELECTION"));
    std::string unlockTitle;
    if (lockedCount != 0)
    {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s %i %s", Localization::get("UNLOCK").c_str(), static_cast<int>(lockedCount),
                      Localization::get("ITEMS").c_str());
        unlockTitle = buf;
    }
    else
    {
        unlockTitle = Localization::get("UNLOCK ALL");
    }
    skinButton(centerToCharBtn(), 1);
    centerToCharBtn()->setTitleText(capitalizedKey("CENTER TO CHARACTER"));
    skinButton(unlockBtn(), 1);
    unlockBtn()->setTitleText(uikit::capitalizedString(unlockTitle));

    rotateSwitch()->setOn(editorLayer()->rotateItemsIndependently());
    customizeSwitch(rotateSwitch());
    formatLabel(rotateSwitchLabel());
    rotateSwitchLabel()->setString(capitalizedKey("ROTATE ITEMS\nSEPARATELY"));
    snapRotSwitch()->setOn(editorLayer()->snapToAngle());
    customizeSwitch(snapRotSwitch());
    formatLabel(snapRotLabel());
    snapRotLabel()->setString(capitalizedKey("SNAP TO\nANGLE"));
    pasteInPlaceSwitch()->setOn(editorLayer()->pasteInPlace());
    customizeSwitch(pasteInPlaceSwitch());
    formatLabel(pasteInPlaceLabel());
    pasteInPlaceLabel()->setString(capitalizedKey("PASTE IN\nPLACE"));

    const Vector<Special*>& selected = editorLayer()->selectedRefs();
    const ssize_t count = selected.size();
    bool canLock = count > 0;
    if (count == 1) canLock = dynamic_cast<CharacterRef*>(selected.at(0)) == nullptr;
    setControlEnabled(lockBtn(), canLock);
    setControlEnabled(unlockBtn(), lockedCount != 0);
}

// @ios 10010c2a8
void EditorMenuViewController::viewWillDisappear(bool animated)
{
    EditorViewController::viewWillDisappear(animated);
    uikit::NotificationCenter::removeObserver(this);
}

// @ios 10010c2d4
void EditorMenuViewController::didReceiveMemoryWarning() { EditorViewController::didReceiveMemoryWarning(); }

// @ios 10010c474
void EditorMenuViewController::saveLevelCancel(void* /*notificationObject*/)
{
    // iOS sends removeObserver:forKeyPath: (the KVO selector) for both names here.
    uikit::NotificationCenter::removeObserver(this, uikit::notification::kSaveLevelCancel, nullptr);
    uikit::NotificationCenter::removeObserver(this, uikit::notification::kSaveLevelDone, nullptr);
}

// @ios 10010c4c8
void EditorMenuViewController::saveLevelDone(void* /*notificationObject*/)
{
    uikit::NotificationCenter::removeObserver(this, uikit::notification::kSaveLevelCancel, nullptr);
    uikit::NotificationCenter::removeObserver(this, uikit::notification::kSaveLevelDone, nullptr);
}

// @ios 10010c51c
void EditorMenuViewController::formatLabel(ui::Text* label)
{
    label->setFontName(uikit::fontFile(true));
    label->setFontSize(12.0f * uikit::pointsToDesign());  // Helvetica-Bold 12
    uikit::setLabelShadow(label, uikit::grayColor(), Size(0.5f, 0.5f));
    label->setTextColor(uikit::whiteColor());
    // numberOfLines 0: the text area is the label's frame (ui::Text with a fixed content size).
    label->setTextHorizontalAlignment(TextHAlignment::CENTER);
    // adjustsFontSizeToFitWidth: applied by fitLabelWidth when the text is set (two short lines).
}

// @ios 10010c5cc
void EditorMenuViewController::customizeSwitch(uikit::Switch* aSwitch)
{
    aSwitch->setOnTintColor(uikit::color(0.239215686917305, 0.5333333611488342, 0.7803921699523926, 1.0));
}

// @ios 10010c61c
void EditorMenuViewController::saveBtnPressed(Ref* /*sender*/)
{
    SaveLevelViewController* vc = SaveLevelViewController::create(_levelMO);
    if (vc == nullptr) return;
    vc->setEditorLayer(editorLayer());
    uikit::NotificationCenter::addObserver(this, uikit::notification::kSaveLevelCancel, vc,
                                           [this](void* object, void*) { saveLevelCancel(object); });
    uikit::NotificationCenter::addObserver(this, uikit::notification::kSaveLevelDone, vc,
                                           [this](void* object, void*) { saveLevelDone(object); });
    vc->setModalPresentationStyle(kModalPresentationCurrentContext);
    presentViewController(vc, true, nullptr);
}

// @ios 10010c708
void EditorMenuViewController::loadBtnPressed(Ref* /*sender*/)
{
    if (editorLayer()->unsavedChanges())
    {
        showLoseChangesAlertViewWithAlertType(2);
        return;
    }
    confirmLoadLevel();
}

// @ios 10010c748
void EditorMenuViewController::confirmLoadLevel()
{
    // [[CCDirector sharedDirector] setModalPresentationStyle:UIModalPresentationCurrentContext] - no
    // effect on the port's root.
    LoadLevelViewController* vc = LoadLevelViewController::create();
    if (vc == nullptr) return;
    CC_SAFE_RETAIN(vc);
    CC_SAFE_RELEASE(_loadLevelViewController);
    _loadLevelViewController = vc;
    vc->setModalPresentationStyle(kModalPresentationCurrentContext);
    // @ios 10010c80c: completion block -> showLoadLevelVCComplete
    presentViewController(vc, true, [this]() { showLoadLevelVCComplete(); });
}

// @ios 10010c814
void EditorMenuViewController::viewDidAppear(bool animated)
{
    EditorViewController::viewDidAppear(animated);
    if (_loadLevelViewController != nullptr) CC_SAFE_RELEASE_NULL(_loadLevelViewController);
}

// @ios 10010c82c
void EditorMenuViewController::showLoadLevelVCComplete() {}

// @ios 10010c830
void EditorMenuViewController::loadLevelVCClosed(void* /*notificationObject*/)
{
    uikit::NotificationCenter::removeObserver(this);
    if (popoverController() == nullptr && _loadLevelViewController != nullptr)
    {
        dismissViewControllerAnimated(true, nullptr);
        CC_SAFE_RELEASE_NULL(_loadLevelViewController);
    }
}

// @ios 10010c89c
void EditorMenuViewController::createNewLevelBtnPressed(Ref* /*sender*/)
{
    if (editorLayer()->unsavedChanges())
    {
        showLoseChangesAlertViewWithAlertType(0);
        return;
    }
    confirmNewLevel();
}

// @ios 10010c8dc
void EditorMenuViewController::confirmNewLevel()
{
    LevelSession::getInstance()->clearLevelData();
    editorLayer()->newLevel();
    if (popoverController() != nullptr)
    {
        popoverController()->dismissPopoverAnimated(true);
        return;
    }
    closeMenuBtnPressed(nullptr);
}

// @ios 10010c940
void EditorMenuViewController::exitEditorBtnPressed(Ref* /*sender*/)
{
    if (editorLayer()->unsavedChanges())
    {
        showLoseChangesAlertViewWithAlertType(1);
        return;
    }
    confirmExitEditor();
}

// @ios 10010c980
void EditorMenuViewController::confirmExitEditor()
{
    RefPtr<EditorMenuViewController> keep(this);
    // [self.presentingViewController dismissViewControllerAnimated:YES completion:nil]
    if (presentingViewController() != nullptr)
        presentingViewController()->dismissViewControllerAnimated(true, nullptr);
    else if (isPresentedByRoot())
        EditorViewController::dismissRootPresented(true, nullptr);
    Director::getInstance()->replaceScene(MainMenu::createScene(MenuModeMain, nullptr));
}

// @ios 10010c9cc
void EditorMenuViewController::closeMenuBtnPressed(Ref* /*sender*/)
{
    RefPtr<EditorMenuViewController> keep(this);
    if (_loadLevelViewController != nullptr)
    {
        if (presentingViewController() != nullptr)
            presentingViewController()->dismissViewControllerAnimated(false, nullptr);
        else if (isPresentedByRoot())
            EditorViewController::dismissRootPresented(false, nullptr);
        CC_SAFE_RELEASE_NULL(_loadLevelViewController);
    }
    uikit::NotificationCenter::postNotification(uikit::notification::kMainMenuClosed, nullptr);
}

// @ios 10010ca28
void EditorMenuViewController::lockBtnPressed(Ref* /*sender*/)
{
    _editorLayer->lockSelection();
    closeMenuBtnPressed(nullptr);
}

// @ios 10010ca5c
void EditorMenuViewController::unlockBtnPressed(Ref* /*sender*/)
{
    _editorLayer->unlockAll();
    closeMenuBtnPressed(nullptr);
}

// @ios 10010ca90
void EditorMenuViewController::rotateSwitchValueChanged(Ref* /*sender*/)
{
    editorLayer()->setRotateItemsIndependently(rotateSwitch()->isOn());
}

// @ios 10010cac4
void EditorMenuViewController::pasteInPlaceBtnPressed(Ref* /*sender*/)
{
    editorLayer()->setPasteInPlace(pasteInPlaceSwitch()->isOn());
}

// @ios 10010caf8
void EditorMenuViewController::shareBtnPressed(Ref* /*sender*/)
{
    // [[HWTracker sharedInstance] submitAction:...] - analytics, dropped on PC.
    LevelMO* levelMO = editorLayer()->levelMO();
    ShareAction::shareLevelDataFile(editorLayer()->levelData(), editorLayer()->characterIndex(),
                                    editorLayer()->forceCharacter(), levelMO ? levelMO->name() : std::string(),
                                    std::string(), levelMO ? levelMO->comments() : std::string(), this);
}

// NET (PC addition): sends the level as it is in the editor, with the same data as SHARE LEVEL.
void EditorMenuViewController::sendNearbyBtnPressed(Ref* /*sender*/)
{
    LevelMO* levelMO = editorLayer()->levelMO();
    net::LevelPackage level;
    level.data = editorLayer()->levelData();
    level.playableCharacter = static_cast<int>(editorLayer()->characterIndex());
    level.forceCharacter = editorLayer()->forceCharacter();
    level.name = levelMO ? levelMO->name() : std::string("Untitled");
    level.comments = levelMO ? levelMO->comments() : std::string();
    net::showSendToNearby(level);
}

// @ios 10010cbc4
void EditorMenuViewController::centerToCharBtnPressed(Ref* /*sender*/)
{
    editorLayer()->centerToCharacter();
    closeMenuBtnPressed(nullptr);
}

// @ios 10010cbf0
void EditorMenuViewController::showLoseChangesAlertViewWithAlertType(int alertType)
{
    // UIAlertView "Hey" / "Lose changes?", cancelButtonTitle "Yes" (index 0), otherButtonTitles "No"
    // (hard-coded English on iOS). levelui::showAlert puts it above uikit::window().
    levelui::showAlert(alertType, Localization::get("Hey"), Localization::get("Lose changes?"),
                       Localization::get("Yes"), Localization::get("No"), this);
}

// @ios 10010cc68
void EditorMenuViewController::alertView(HWWindow* alertView, long buttonIndex)
{
    const int tag = alertView->getTag();
    if (tag == 2)
    {
        if (buttonIndex == 0) confirmLoadLevel();
    }
    else if (tag == 1)
    {
        if (buttonIndex == 0) confirmExitEditor();
    }
    else if (tag == 0 && buttonIndex == 0)
    {
        confirmNewLevel();
    }
}

void EditorMenuViewController::hwWindowButtonPressed(int buttonTag, HWWindow* window)
{
    alertView(window, levelui::buttonIndex(buttonTag, window));
}

// ---- properties --------------------------------------------------------------------------------

// @ios 10010cce0
EditorLayer* EditorMenuViewController::editorLayer() const { return _editorLayer; }
// @ios 10010ccf0
void EditorMenuViewController::setEditorLayer(EditorLayer* editorLayer) { _editorLayer = editorLayer; }
// @ios 10010cd00
EditorPopoverController* EditorMenuViewController::popoverController() const { return _popoverController; }
// @ios 10010cd10
void EditorMenuViewController::setPopoverController(EditorPopoverController* popoverController) { _popoverController = popoverController; }
// @ios 10010cd20
ui::Button* EditorMenuViewController::saveBtn() const { return _saveBtn; }
// @ios 10010cd30
void EditorMenuViewController::setSaveBtn(ui::Button* saveBtn) { _saveBtn = saveBtn; }
// @ios 10010cd3c
ui::Button* EditorMenuViewController::loadBtn() const { return _loadBtn; }
// @ios 10010cd4c
void EditorMenuViewController::setLoadBtn(ui::Button* loadBtn) { _loadBtn = loadBtn; }
// @ios 10010cd58
ui::Button* EditorMenuViewController::createNewLevelBtn() const { return _createNewLevelBtn; }
// @ios 10010cd68
void EditorMenuViewController::setCreateNewLevelBtn(ui::Button* createNewLevelBtn) { _createNewLevelBtn = createNewLevelBtn; }
// @ios 10010cd74
ui::Button* EditorMenuViewController::closeBtn() const { return _closeBtn; }
// @ios 10010cd84
void EditorMenuViewController::setCloseBtn(ui::Button* closeBtn) { _closeBtn = closeBtn; }
// @ios 10010cd90
ui::Button* EditorMenuViewController::lockBtn() const { return _lockBtn; }
// @ios 10010cda0
void EditorMenuViewController::setLockBtn(ui::Button* lockBtn) { _lockBtn = lockBtn; }
// @ios 10010cdac
ui::Button* EditorMenuViewController::unlockBtn() const { return _unlockBtn; }
// @ios 10010cdbc
void EditorMenuViewController::setUnlockBtn(ui::Button* unlockBtn) { _unlockBtn = unlockBtn; }
// @ios 10010cdc8
ui::Button* EditorMenuViewController::mainMenuBtn() const { return _mainMenuBtn; }
// @ios 10010cdd8
void EditorMenuViewController::setMainMenuBtn(ui::Button* mainMenuBtn) { _mainMenuBtn = mainMenuBtn; }
// @ios 10010cde4
ui::Button* EditorMenuViewController::shareBtn() const { return _shareBtn; }
// @ios 10010cdf4
void EditorMenuViewController::setShareBtn(ui::Button* shareBtn) { _shareBtn = shareBtn; }
// @ios 10010ce00
uikit::Switch* EditorMenuViewController::rotateSwitch() const { return _rotateSwitch; }
// @ios 10010ce10
void EditorMenuViewController::setRotateSwitch(uikit::Switch* rotateSwitch) { _rotateSwitch = rotateSwitch; }
// @ios 10010ce1c
ui::Text* EditorMenuViewController::rotateSwitchLabel() const { return _rotateSwitchLabel; }
// @ios 10010ce2c
void EditorMenuViewController::setRotateSwitchLabel(ui::Text* rotateSwitchLabel) { _rotateSwitchLabel = rotateSwitchLabel; }
// @ios 10010ce38
uikit::Switch* EditorMenuViewController::pasteInPlaceSwitch() const { return _pasteInPlaceSwitch; }
// @ios 10010ce48
void EditorMenuViewController::setPasteInPlaceSwitch(uikit::Switch* pasteInPlaceSwitch) { _pasteInPlaceSwitch = pasteInPlaceSwitch; }
// @ios 10010ce54
ui::Text* EditorMenuViewController::pasteInPlaceLabel() const { return _pasteInPlaceLabel; }
// @ios 10010ce64
void EditorMenuViewController::setPasteInPlaceLabel(ui::Text* pasteInPlaceLabel) { _pasteInPlaceLabel = pasteInPlaceLabel; }
// @ios 10010ce70
ui::Text* EditorMenuViewController::snapRotLabel() const { return _snapRotLabel; }
// @ios 10010ce80
void EditorMenuViewController::setSnapRotLabel(ui::Text* snapRotLabel) { _snapRotLabel = snapRotLabel; }
// @ios 10010ce8c
uikit::Switch* EditorMenuViewController::snapRotSwitch() const { return _snapRotSwitch; }
// @ios 10010ce9c
void EditorMenuViewController::setSnapRotSwitch(uikit::Switch* snapRotSwitch) { _snapRotSwitch = snapRotSwitch; }
// @ios 10010cea8
ui::Button* EditorMenuViewController::centerToCharBtn() const { return _centerToCharBtn; }
// @ios 10010ceb8
void EditorMenuViewController::setCenterToCharBtn(ui::Button* centerToCharBtn) { _centerToCharBtn = centerToCharBtn; }
// @ios 10010cec4
ui::Text* EditorMenuViewController::menuNameLabel() const { return _menuNameLabel; }
// @ios 10010ced4
void EditorMenuViewController::setMenuNameLabel(ui::Text* menuNameLabel) { _menuNameLabel = menuNameLabel; }
