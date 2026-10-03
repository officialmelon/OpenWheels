#pragma once

#include "AdControllerDelegate.h"

#include "math/CCGeometry.h"

#include <string>

// What showAd() is asked to show. Values are from the binary; names are not in it.
//   0  banner: Tracker "show banner", JNI AdHelper.showBanner(), marks the banner visible, returns true
//      (only when ads are not removed and no banner is visible yet).
//   1  only marks the banner visible (no JNI call), returns false. No caller in 1.1.3.
//   2  interstitial: when the elapsed gameplay time exceeds the interval: Tracker, JNI
//      AdHelper.showInterstitial(), reset the timer, returns true and the result arrives later
//      through the delegate. Ads removed: delegate->interstitialDidEnd(AdsRemoved), returns false.
//   3  rewarded video: Tracker, JNI AdHelper.showRewardedVideo(), returns false; the result arrives
//      through delegate->rewardedVideoDidUpdateStatus.
// RE-TODO(@00648688): enumerator names are invented.
enum AdType
{
    AdTypeBanner = 0,
    AdTypeBannerFlagOnly = 1,
    AdTypeInterstitial = 2,
    AdTypeRewardedVideo = 3,
};

// Ad network front end (no base class, no RTTI, arm64 sizeof 0x18; created lazily by
// Settings::getAdController()). On Android it drives the Java com.fancyforce.AdHelper (AppLovin MAX)
// through JNI and receives results through the extern "C" JNI entry points
// Java_com_fancyforce_AdHelper_jniInterstitialDidEnd (@00648068),
// Java_com_fancyforce_AdHelper_jniRewardedVideoDidUpdateStatus (@00648190) and
// Java_com_fancyforce_AdHelper_jniBannerShown (@00648284), which live in this TU.
//
// OpenWheels (PC): platform stub. Ads are never shown: showAd() reports "not available" (returns
// false without starting anything), so Gameplay::oneFrameAfterOnEnterTransitionDidFinish clears the
// delegate and calls beginGameplayFollowingInterstitial() immediately. The interval timer and the
// "remove_ads" bookkeeping stay as in the original. See docs/modules/M10.md.
//
// The iOS ancestor is FFAdController (_elapsedGameplayTime, _forceInterstitial, _delegate, ...).
class AdController
{
public:
    // Delegate forwarding used by the JNI glue (interstitial / rewarded video results).
    void interstitialDidEnd();                                                 // @00648128 -> Ended
    void interstitialDidFail();                                                // @00648144 -> Failed
    void interstitialDidExpire();                                              // @00648160 -> Expired
    void interstitialStillLoading();                                           // @0064817c -> StillLoading (no null check)
    void rewardedVideoDidUpdateStatus();                                       // @00648230 -> Rewarded
    void rewardedVideoDidFail();                                               // @0064824c -> Failed
    void rewardedVideoDidExpire();                                             // @00648268 -> Expired
    // Dispatches the custom event "banner_shown" with &bannerHeight as user data.
    void dispatchBannerShown(int bannerHeight);                                // @0064834c

    AdController();                                                            // @00648408
    ~AdController();                                                           // @0064841c

    // _adsRemoved = UserDefault "remove_ads"; interval = firstInterstitialIntervalSeconds;
    // _forceInterstitial overrides both; starts the Java ad helper unless ads are removed.
    bool init();                                                               // @00648420
    // JNI AdHelper.initNativeAds().
    void initJavaAdHelperViaJNI();                                             // @00648484
    // JNI static void com.fancyforce.AdHelper.<methodName>().
    void invokeJNIFunction(std::string methodName);                            // @00648564
    bool showAd(AdType type);                                                  // @00648688
    bool getElapsedTimeHasExceededInterstitialInterval();                      // @00648c34
    // Scheduler::unscheduleUpdate(this).
    void endGameplayTimer();                                                   // @00648c44
    // Elapsed time = 0; after the first interstitial the interval becomes the subsequent one.
    void resetElapsedTime();                                                   // @00648c6c
    // JNI AdHelper.removeBanner(), banner hidden, custom event "banner_removed".
    void removeBannerAd();                                                     // @00648c9c
    // Scheduler::scheduleUpdate(this, 0, false): update(dt) accumulates the elapsed gameplay time.
    void startGameplayTimer();                                                 // @00648de4
    void setAdControllerDelegate(AdControllerDelegate* delegate);              // @00648ee4
    void update(float dt);                                                     // @00648eec
    // Ads removed: (0, 0). Otherwise (-1, 100 / Director::getContentScaleFactor()).
    cocos2d::Size getBannerAdSize();                                           // @00648efc
    bool getAdsRemoved();                                                      // @00648f50
    // Removing also hides the banner (removeBannerAd()).
    void setAdsRemoved(bool adsRemoved);                                       // @00648f58

private:
    // Debug switch: when set, init() forces ads on and the interstitial interval to 0. Only the
    // constructor writes it (false).
    bool _forceInterstitial;                    // +0x00
    bool _adsRemoved;                           // +0x01 UserDefault "remove_ads"
    bool _bannerIsShowing;                      // +0x02
    float _elapsedGameplayTime;                 // +0x04 seconds since the last interstitial
    float _interstitialInterval;                // +0x08 globals::advertising::first/subsequent...
    AdControllerDelegate* _delegate;            // +0x10
};
