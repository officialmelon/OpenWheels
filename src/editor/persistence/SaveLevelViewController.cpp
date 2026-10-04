// SaveLevelViewController -- port of the iOS save dialog (see SaveLevelViewController.h).
#include "SaveLevelViewController.h"

#include "EditorLayer.h"
#include "HWWindow.h"
#include "LevelMO.h"
#include "LevelSession.h"
#include "LevelStore.h"
#include "LevelUIHelpers.h"
#include "Settings.h"
#include "Tracker.h"
#include "platform/common/Localization.h"

USING_NS_CC;

namespace {

enum
{
    kAlertTagNone = 0,
    kAlertTagCancelSave = 1,
    kAlertTagSaved = 2,
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

SaveLevelViewController* SaveLevelViewController::create(LevelMO* levelMO)
{
    SaveLevelViewController* controller = new (std::nothrow) SaveLevelViewController();
    if (controller && controller->initWithNibName("SaveLevelViewController", levelMO))
    {
        controller->autorelease();
        return controller;
    }
    delete controller;
    return nullptr;
}

SaveLevelViewController::SaveLevelViewController()
    : maxNameChars(0),
      maxDescChars(0),
      setMO(nullptr),
      _levelMO(nullptr),
      editorLayer_(nullptr),
      _nameTextView(nullptr),
      _descriptionTextView(nullptr),
      _nameLabel(nullptr),
      _nameCharsLeftLabel(nullptr),
      _descriptionLabel(nullptr),
      _descriptionCharsLeftLabel(nullptr),
      _cancelBtn(nullptr),
      _saveBtn(nullptr),
      _saveOverBtn(nullptr)
{
}

// @ios 10010d42c  (dealloc)
SaveLevelViewController::~SaveLevelViewController()
{
    CC_SAFE_RELEASE(_levelMO);
}

// @ios 10010cee0
bool SaveLevelViewController::initWithNibName(const std::string& nibName, LevelMO* levelMO)
{
    if (!EditorViewController::initWithNibName(nibName))
    {
        return false;
    }
    maxNameChars = 20;
    maxDescChars = 300;
    charLeftString = capitalizedL("CHARS LEFT");
    setMO = LevelStore::getInstance()->chapterWithIndex(LevelStoreChapterUser);  // "chapterIndex == 5000"
    CC_SAFE_RETAIN(levelMO);
    CC_SAFE_RELEASE(_levelMO);
    _levelMO = levelMO;
    return true;
}

// @ios 10010d36c
void SaveLevelViewController::viewDidLoad()
{
    EditorViewController::viewDidLoad();
    // setContentSizeForViewInPopover:(480, 320): the editor-menu popover is that size.

    // Port: the nib's view hierarchy, autolayout resolved for the window (see the header).
    // Laid out in the controller's own frame: the window, or the editor-menu popover it covers
    // (iPad UIModalPresentationCurrentContext; frames are set before viewDidLoad).
    Size win = frame().size;
    float W = win.width;
    float H = win.height;
    setBackgroundColor(uikit::color(0.706, 0.706, 0.706, 1.0));

    uikit::View* panel = uikit::View::create(Rect(0.0f, 0.0f, W, H - 60.0f));
    panel->setBackgroundColor(uikit::color(0.8, 0.8, 0.8, 1.0));
    addSubview(panel);

    float fieldW = W - 40.0f;
    _nameLabel = levelui::makeLabel("", 17.0f, false, 0, uikit::whiteColor());
    panel->addSubview(_nameLabel, Rect(20.0f, 10.0f, 300.0f, 21.0f));
    _nameCharsLeftLabel = levelui::makeLabel("", 17.0f, false, 2, uikit::whiteColor());
    panel->addSubview(_nameCharsLeftLabel, Rect(W - 20.0f - 200.0f, 10.0f, 200.0f, 21.0f));
    _nameTextView = LevelTextView::create(Rect(20.0f, 39.0f, fieldW, 32.0f), 14.0f, true);
    _nameTextView->setBackgroundColor(uikit::whiteColor());
    panel->addSubview(_nameTextView);

    _descriptionLabel = levelui::makeLabel("", 17.0f, false, 0, uikit::whiteColor());
    panel->addSubview(_descriptionLabel, Rect(20.0f, 103.0f, 300.0f, 21.0f));
    _descriptionCharsLeftLabel = levelui::makeLabel("", 17.0f, false, 2, uikit::whiteColor());
    panel->addSubview(_descriptionCharsLeftLabel, Rect(W - 20.0f - 200.0f, 103.0f, 200.0f, 21.0f));
    _descriptionTextView = LevelTextView::create(Rect(20.0f, 132.0f, fieldW, 77.0f), 14.0f, true);
    _descriptionTextView->setBackgroundColor(uikit::whiteColor());
    panel->addSubview(_descriptionTextView);

    _saveBtn = levelui::makeButton("[save new]", 15.0f, CC_CALLBACK_1(SaveLevelViewController::saveBtnPressed, this));
    addSubview(_saveBtn, Rect(10.0f, H - 11.0f - 39.0f, 120.0f, 39.0f));
    _saveOverBtn = levelui::makeButton("[save overwrite]", 15.0f, [this](Ref* sender) { saveOverBtn(sender); });
    addSubview(_saveOverBtn, Rect(138.0f, H - 11.0f - 39.0f, 120.0f, 39.0f));
    _cancelBtn = levelui::makeButton("[Cancel]", 15.0f, CC_CALLBACK_1(SaveLevelViewController::cancelBtnPressed, this));
    addSubview(_cancelBtn, Rect(W - 9.0f - 120.0f, H - 10.0f - 40.0f, 120.0f, 40.0f));
    // The nib's "Test" button (testBtnPressed:) is not in the view hierarchy.
}

// @ios 10010d024
void SaveLevelViewController::viewWillAppear(bool animated)
{
    EditorViewController::viewWillAppear(animated);
    LevelMO* editorLevel = editorLayer_ ? editorLayer_->levelMO() : nullptr;
    bool hasEditorLevelName = editorLevel != nullptr;   // editorLayer.levelMO.name != nil
    bool newLevel = _levelMO == nullptr && !hasEditorLevelName;

    _descriptionTextView->setDelegate(this);
    _nameTextView->setDelegate(this);
    _nameLabel->setString(capitalizedL("LEVEL NAME"));
    _nameCharsLeftLabel->setString(StringUtils::format("%i %s", maxNameChars, charLeftString.c_str()));
    _descriptionLabel->setString(capitalizedL("LEVEL DESCRIPTION"));
    _descriptionCharsLeftLabel->setString(StringUtils::format("%i %s", maxDescChars, charLeftString.c_str()));
    _nameTextView->setText("");
    _descriptionTextView->setText("");
    skinButton(_cancelBtn, 0);
    levelui::setButtonTitle(_cancelBtn, capitalizedL("CANCEL"));
    std::string saveTitle = capitalizedL(newLevel ? "SAVE" : "SAVE NEW");
    skinButton(_saveBtn, 1);
    levelui::setButtonTitle(_saveBtn, saveTitle);
    if (newLevel)
    {
        if (_saveOverBtn)
        {
            _saveOverBtn->removeFromParent();
        }
        setSaveOverBtn(nullptr);
        // EDITOR (browser features, PC addition): a level opened from XML (online "EDIT",
        // --edit) suggests its own name.
        if (editorLayer_ && !editorLayer_->pendingLevelName().empty())
        {
            _nameTextView->setText(editorLayer_->pendingLevelName().substr(0, maxNameChars));
            textViewDidChange(_nameTextView);
        }
    }
    else
    {
        skinButton(_saveOverBtn, 1);
        levelui::setButtonTitle(_saveOverBtn, capitalizedL("SAVE OVER"));
        // Pre-filled from editorLayer.levelMO (not from _levelMO).
        if (editorLevel)
        {
            _nameTextView->setText(editorLevel->name());
            textViewDidChange(_nameTextView);
            _descriptionTextView->setText(editorLevel->comments());
            textViewDidChange(_descriptionTextView);
        }
    }
}

// @ios 10010d3c4
void SaveLevelViewController::didReceiveMemoryWarning()
{
    EditorViewController::didReceiveMemoryWarning();
}

// @ios 10010d3f8
void SaveLevelViewController::viewWillLayoutSubviews()
{
}

// ---- actions ----------------------------------------------------------------------------------

// @ios 10010d52c
void SaveLevelViewController::saveOverBtn(Ref* sender)
{
    LevelStore* store = LevelStore::getInstance();
    if (LevelMO* level = editorLayer_ ? editorLayer_->levelMO() : nullptr)
    {
        level->setComments(_descriptionTextView->text());
        level->setName(_nameTextView->text());
        level->setData(editorLayer_->levelData());
        level->setForce_character(editorLayer_->forceCharacter());
        level->setPlayable_character(static_cast<int>(editorLayer_->characterIndex()));
        level->removeAllLevelTimes();   // deleteObject: every LevelTimeMO
    }
    std::string error;
    if (!store->save(&error))
    {
        showAlert(kAlertTagNone, "Error", error, "Ok", "", true);
        return;
    }
    showSaveConfirmAlert();
}

// @ios 10010d768
void SaveLevelViewController::saveBtnPressed(Ref* sender)
{
    if (levelui::trimmed(_nameTextView->text()).empty())
    {
        showAlert(kAlertTagNone, L("HEY"), L("ENTER A NAME"), L("OK"), "", false);
        return;
    }
    std::string name = _nameTextView->text();
    std::string comments = _descriptionTextView->text();
    std::string data = editorLayer_ ? editorLayer_->levelData() : std::string();
    for (LevelMO* other : setMO->levels())
    {
        if (other->name() == name)
        {
            showAlert(kAlertTagNone, L("HEY"), L("ENTER UNIQUE NAME"), L("OK"), "", false);
            return;
        }
    }
    LevelStore* store = LevelStore::getInstance();
    int lastId = store->lastLevelId(LevelStoreChapterUser);
    LevelMO* level = store->insertNewLevel();
    level->setData(data);
    level->setId_x(lastId + 1);
    level->setName(name);
    level->setComments(comments);
    level->setForce_character(editorLayer_ && editorLayer_->forceCharacter());
    level->setPlayable_character(editorLayer_ ? static_cast<int>(editorLayer_->characterIndex()) : 0);
    setMO->addLevelsObject(level);
    if (editorLayer_)
    {
        editorLayer_->setLevelMO(level);
    }
    LevelSession::getInstance()->setLevelDataWithManagedObject(level);
    Settings::getInstance()->getTracker()->submitAction(
        "editor_level_menu", "save_level",
        StringUtils::format("count_%i", static_cast<int>(level->chapter()->levels().size())), -1);
    std::string error;
    if (!store->save(&error))
    {
        showAlert(kAlertTagNone, "Error", error, "Ok", "", true);
        return;
    }
    showSaveConfirmAlert();
}

// @ios 10010dc64
void SaveLevelViewController::testBtnPressed(Ref* sender)
{
    for (LevelMO* level : setMO->levels())
    {
        CCLOG("%s", level->name().c_str());
    }
}

// @ios 10010dd84
void SaveLevelViewController::showSaveConfirmAlert()
{
    showAlert(kAlertTagSaved, L("SUCCESS"), L("LEVEL SAVED"), L("OK"), "", true);
}

// @ios 10010de58
void SaveLevelViewController::cancelBtnPressed(Ref* sender)
{
    std::string name = levelui::trimmed(_nameTextView->text());
    std::string description = levelui::trimmed(_descriptionTextView->text());
    if (name.empty() && description.empty())
    {
        if (EditorViewController* presenting = presentingViewController())
        {
            presenting->dismissViewControllerAnimated(true, nullptr);
        }
        else
        {
            dismissViewControllerAnimated(true, nullptr);
        }
        return;
    }
    showAlert(kAlertTagCancelSave, "Do Not Save?", "Cancel the save?", "NO", "YES", true);
}

// @ios 10010df70
void SaveLevelViewController::cancelSave()
{
    if (EditorViewController* presenting = presentingViewController())
    {
        presenting->dismissViewControllerAnimated(true, nullptr);
    }
}

// ---- alerts -----------------------------------------------------------------------------------

void SaveLevelViewController::showAlert(int tag, const std::string& title, const std::string& message,
                                        const std::string& cancelTitle, const std::string& otherTitle,
                                        bool withDelegate)
{
    if (levelui::showAlert(tag, title, message, cancelTitle, otherTitle, withDelegate ? this : nullptr) &&
        withDelegate)
    {
        retain();  // released in hwWindowWasDismissed
    }
}

void SaveLevelViewController::hwWindowButtonPressed(int buttonTag, HWWindow* window)
{
    alertViewClickedButtonAtIndex(window->getTag(), levelui::buttonIndex(buttonTag, window));
}

void SaveLevelViewController::hwWindowWasDismissed(HWWindow* window)
{
    release();
}

// @ios 10010df8c
void SaveLevelViewController::alertViewClickedButtonAtIndex(int alertTag, long buttonIndex)
{
    if (alertTag != kAlertTagSaved && (alertTag != kAlertTagCancelSave || buttonIndex != 1))
    {
        return;
    }
    // EditorLayer::saveLevelComplete (observer) sets hasSaved and dismisses the presented menu.
    RefPtr<EditorViewController> presenting = presentingViewController();
    uikit::NotificationCenter::postNotification(uikit::notification::kSaveLevelDone, nullptr);
    if (presenting && presenting->presentedViewController() == this)
    {
        presenting->dismissViewControllerAnimated(true, nullptr);
    }
}

// ---- UITextViewDelegate -----------------------------------------------------------------------

// @ios 10010e008
void SaveLevelViewController::textViewDidBeginEditing(LevelTextView* textView)
{
}

// @ios 10010e00c
void SaveLevelViewController::textViewDidChange(LevelTextView* textView)
{
    bool isName = textView == _nameTextView;
    ui::Text* counter = isName ? _nameCharsLeftLabel : _descriptionCharsLeftLabel;
    int max = static_cast<int>(isName ? maxNameChars : maxDescChars);
    int left = max - static_cast<int>(textView->length());
    counter->setString(StringUtils::format("%i %s", left, charLeftString.c_str()));
}

// @ios 10010e0b8
void SaveLevelViewController::textViewDidEndEditing(LevelTextView* textView)
{
}

// @ios 10010e0bc
bool SaveLevelViewController::textViewShouldChangeTextInRange(LevelTextView* textView, size_t location,
                                                              size_t length, const std::string& replacement)
{
    if (replacement == "\n")
    {
        if (textView != _nameTextView)
        {
            textView->resignFirstResponder();
            return false;
        }
        _descriptionTextView->becomeFirstResponder();
        return false;
    }
    unsigned int max = textView == _nameTextView ? maxNameChars : maxDescChars;
    size_t newLength = textView->length() - length + LevelTextView::lengthUTF16(replacement);
    return newLength <= max;
}

// ---- properties -------------------------------------------------------------------------------

EditorLayer* SaveLevelViewController::editorLayer() const { return editorLayer_; }                 // @ios 10010e19c
void SaveLevelViewController::setEditorLayer(EditorLayer* editorLayer) { editorLayer_ = editorLayer; }  // @ios 10010e1ac
LevelTextView* SaveLevelViewController::nameTextView() const { return _nameTextView; }            // @ios 10010e1bc
void SaveLevelViewController::setNameTextView(LevelTextView* textView) { _nameTextView = textView; }  // @ios 10010e1cc
LevelTextView* SaveLevelViewController::descriptionTextView() const { return _descriptionTextView; }  // @ios 10010e1d8
void SaveLevelViewController::setDescriptionTextView(LevelTextView* textView) { _descriptionTextView = textView; }  // @ios 10010e1e8
ui::Text* SaveLevelViewController::nameLabel() const { return _nameLabel; }                       // @ios 10010e1f4
void SaveLevelViewController::setNameLabel(ui::Text* label) { _nameLabel = label; }               // @ios 10010e204
ui::Text* SaveLevelViewController::nameCharsLeftLabel() const { return _nameCharsLeftLabel; }     // @ios 10010e210
void SaveLevelViewController::setNameCharsLeftLabel(ui::Text* label) { _nameCharsLeftLabel = label; }  // @ios 10010e220
ui::Text* SaveLevelViewController::descriptionLabel() const { return _descriptionLabel; }         // @ios 10010e22c
void SaveLevelViewController::setDescriptionLabel(ui::Text* label) { _descriptionLabel = label; } // @ios 10010e23c
ui::Text* SaveLevelViewController::descriptionCharsLeftLabel() const { return _descriptionCharsLeftLabel; }  // @ios 10010e248
void SaveLevelViewController::setDescriptionCharsLeftLabel(ui::Text* label) { _descriptionCharsLeftLabel = label; }  // @ios 10010e258
ui::Button* SaveLevelViewController::cancelBtn() const { return _cancelBtn; }                     // @ios 10010e264
void SaveLevelViewController::setCancelBtn(ui::Button* button) { _cancelBtn = button; }           // @ios 10010e274
ui::Button* SaveLevelViewController::saveBtn() const { return _saveBtn; }                         // @ios 10010e280
void SaveLevelViewController::setSaveBtn(ui::Button* button) { _saveBtn = button; }               // @ios 10010e290
ui::Button* SaveLevelViewController::saveOverBtn() const { return _saveOverBtn; }                 // @ios 10010e29c
void SaveLevelViewController::setSaveOverBtn(ui::Button* button) { _saveOverBtn = button; }       // @ios 10010e2ac

std::string SaveLevelViewController::trimmed(const std::string& text)
{
    return levelui::trimmed(text);
}
