// SBSaveLevelViewController -- the unreachable storyboard save stub (see SBSaveLevelViewController.h).
#include "SBSaveLevelViewController.h"

#include "LevelUIHelpers.h"

USING_NS_CC;

SBSaveLevelViewController* SBSaveLevelViewController::create()
{
    SBSaveLevelViewController* controller = new (std::nothrow) SBSaveLevelViewController();
    if (controller && controller->initWithNibName("saveLevelStoryBoard"))
    {
        controller->autorelease();
        return controller;
    }
    delete controller;
    return nullptr;
}

SBSaveLevelViewController::SBSaveLevelViewController() : _edited(false)
{
}

// @ios 1001038f4  (dealloc)
SBSaveLevelViewController::~SBSaveLevelViewController()
{
}

// @ios 10010370c
bool SBSaveLevelViewController::initWithNibName(const std::string& nibName)
{
    if (!EditorViewController::initWithNibName(nibName))
    {
        return false;
    }
    _edited = false;
    return true;
}

// @ios 100103750
void SBSaveLevelViewController::viewWillAppear(bool animated)
{
    EditorViewController::viewWillAppear(animated);
    // navigationItem.leftBarButtonItem "Cancel" -> goBack, rightBarButtonItem "Save" -> saveLevel
    // (UIBarButtonItemStylePlain). Port: two plain buttons in a 44-point bar.
    // Laid out in the controller's own frame: the window, or the editor-menu popover it covers
    // (iPad UIModalPresentationCurrentContext; frames are set before viewDidLoad).
    Size win = frame().size;
    ui::Button* cancel = levelui::makeButton("Cancel", 17.0f, [this](Ref*) { goBack(); });
    addSubview(cancel, Rect(8.0f, 0.0f, 100.0f, 44.0f));
    ui::Button* save = levelui::makeButton("Save", 17.0f, [this](Ref*) { saveLevel(); });
    addSubview(save, Rect(win.width - 108.0f, 0.0f, 100.0f, 44.0f));
}

// @ios 100103814
void SBSaveLevelViewController::viewDidLoad()
{
    EditorViewController::viewDidLoad();
}

// @ios 100103848
void SBSaveLevelViewController::goBack()
{
    if (_edited)
    {
        return;
    }
    confirmGoBack();
}

// @ios 100103860
void SBSaveLevelViewController::saveLevel()
{
    confirmGoBack();
}

// @ios 100103864
void SBSaveLevelViewController::confirmGoBack()
{
    // [navigationController popToViewController:viewControllers[count - 2] animated:YES]
    dismissViewControllerAnimated(true, nullptr);
}

// @ios 1001038bc
void SBSaveLevelViewController::didReceiveMemoryWarning()
{
    EditorViewController::didReceiveMemoryWarning();
}

// @ios 1001038f0  alertView:didDismissWithButtonIndex: (empty)
void SBSaveLevelViewController::hwWindowWasDismissed(HWWindow* window)
{
}
