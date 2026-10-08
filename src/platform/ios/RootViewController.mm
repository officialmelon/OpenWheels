// iOS root view controller (OpenWheels port), as in the cocos2d-x 3.17.2 template, plus a frame
// update on rotation / resize (Split View, Stage Manager) that re-applies the game's design
// resolution policy, like nativeSurfaceResized on Android.

#import "RootViewController.h"

#include "cocos2d.h"
#import "platform/ios/CCEAGLView-ios.h"

@implementation RootViewController

- (void)loadView
{
    CCEAGLView* eaglView = [CCEAGLView viewWithFrame:[UIScreen mainScreen].bounds
                                         pixelFormat:(__bridge NSString*)cocos2d::GLViewImpl::_pixelFormat
                                         depthFormat:cocos2d::GLViewImpl::_depthFormat
                                  preserveBackbuffer:NO
                                          sharegroup:nil
                                       multiSampling:cocos2d::GLViewImpl::_multisamplingCount > 0 ? YES : NO
                                     numberOfSamples:cocos2d::GLViewImpl::_multisamplingCount];
    // The game's controls are multi-touch (drive and lean at once).
    [eaglView setMultipleTouchEnabled:YES];
    self.view = eaglView;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
    return UIInterfaceOrientationMaskLandscape;
}

- (BOOL)shouldAutorotate
{
    return YES;
}

- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    cocos2d::GLView* glview = cocos2d::Director::getInstance()->getOpenGLView();
    if (!glview) return;
    CCEAGLView* eaglView = (__bridge CCEAGLView*)glview->getEAGLView();
    const float scale = eaglView.contentScaleFactor;
    const cocos2d::Size design = glview->getDesignResolutionSize();
    const ResolutionPolicy policy = glview->getResolutionPolicy();
    glview->setFrameSize(size.width * scale, size.height * scale);
    if (design.width > 0.0f && design.height > 0.0f && policy != ResolutionPolicy::UNKNOWN)
        glview->setDesignResolutionSize(design.width, design.height, policy);
}

- (BOOL)prefersStatusBarHidden
{
    return YES;
}

- (BOOL)prefersHomeIndicatorAutoHidden
{
    return YES;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures
{
    // Thumbs sit on the screen edges while driving: don't let the first swipe open the system UI.
    return UIRectEdgeAll;
}

@end
