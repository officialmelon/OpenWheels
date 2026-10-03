#pragma once
// SBSaveLevelViewController (iOS 1.2.7): the "Save Level" scene of EditorMenuStoryboard
// (storyboard id "saveLevelStoryBoard"), pushed by -[EditorMenuTableViewController
// tableView:didSelectRowAtIndexPath:] row 0. Nothing in the app instantiates
// EditorMenuTableViewController or the storyboard's entry point, so this screen is unreachable on
// iOS: a navigation-bar stub with "Cancel" (goBack) and "Save" (saveLevel) that both just pop back
// (saveLevel saves nothing). Ported for completeness; no port code presents it.
//
// Port: UIViewController + UINavigationController -> EditorViewController; "pop to the previous
// view controller" -> dismissViewControllerAnimated.

#include "EditorViewController.h"
#include "HWWindowDelegate.h"

#include <string>

class SBSaveLevelViewController : public EditorViewController, public HWWindowDelegate
{
public:
    static SBSaveLevelViewController* create();
    bool initWithNibName(const std::string& nibName) override;  // @ios 10010370c  _edited = NO
    // Adds the bar buttons "Cancel" -> goBack and "Save" -> saveLevel.
    void viewWillAppear(bool animated) override;                // @ios 100103750
    void viewDidLoad() override;                                // @ios 100103814
    void goBack();                                              // @ios 100103848  !_edited -> confirmGoBack
    void saveLevel();                                           // @ios 100103860  -> confirmGoBack
    // popToViewController:(viewControllers[count - 2])
    void confirmGoBack();                                       // @ios 100103864
    void didReceiveMemoryWarning() override;                    // @ios 1001038bc
    // alertView:didDismissWithButtonIndex: -- empty
    void hwWindowWasDismissed(HWWindow* window) override;       // @ios 1001038f0

protected:
    SBSaveLevelViewController();
    ~SBSaveLevelViewController() override;                      // @ios 1001038f4  dealloc
    bool _edited;                                               // +0x08
};
