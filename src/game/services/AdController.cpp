#include "AdController.h"

#include "Globals.h"

#include "cocos2d.h"

USING_NS_CC;

// ---------------------------------------------------------------------------------------------------
// PC platform policy (see docs/modules/M10.md)
//
// On Android this class drives the Java com.fancyforce.AdHelper (AppLovin MAX) through JNI:
// initJavaAdHelperViaJNI / invokeJNIFunction / showAd / removeBannerAd call static Java methods, and
// the results come back through three extern "C" JNI entry points that lived in this file:
//   Java_com_fancyforce_AdHelper_jniInterstitialDidEnd(status)          @00648068
//       0,4 -> interstitialDidFail(), 1 -> interstitialDidEnd(), 3 -> interstitialDidExpire(),
//       5 -> interstitialStillLoading()
//   Java_com_fancyforce_AdHelper_jniRewardedVideoDidUpdateStatus(status) @00648190
//       0,4 -> rewardedVideoDidFail(), 1 -> rewardedVideoDidUpdateStatus(),
//       3 -> rewardedVideoDidExpire()
//   Java_com_fancyforce_AdHelper_jniBannerShown(height)                  @00648284
//       -> dispatchBannerShown(height) (inlined)
//
// The PC build has no ad network, so no ad is ever shown:
//   * showAd() returns false for every AdType and starts nothing. Gameplay therefore clears its
//     delegate and begins playing at once (Gameplay::oneFrameAfterOnEnterTransitionDidFinish);
//     banner requests are simply not honoured.
//   * A rewarded-video request is waited on by its caller (the Mascot's "revive" offer), so it is
//     answered at once with the result the Java helper reports when it cannot load an ad
//     (showRewardedVideo() without activity/connectivity -> jniRewardedVideoDidUpdateStatus(0/4) ->
//     rewardedVideoDidFail(), i.e. RewardedVideoStatusFailed), delivered synchronously as the Java
//     helper does in that case.
//   * The JNI calls are dropped; everything else (interval timer, "remove_ads" state, the
//     "banner_shown"/"banner_removed" events) behaves as in the original.
// ---------------------------------------------------------------------------------------------------

// @00648128
void AdController::interstitialDidEnd()
{
    if (_delegate)
    {
        _delegate->interstitialDidEnd(InterstitialStatusEnded);
    }
}

// @00648144
void AdController::interstitialDidFail()
{
    if (_delegate)
    {
        _delegate->interstitialDidEnd(InterstitialStatusFailed);
    }
}

// @00648160
void AdController::interstitialDidExpire()
{
    if (_delegate)
    {
        _delegate->interstitialDidEnd(InterstitialStatusExpired);
    }
}

// @0064817c
void AdController::interstitialStillLoading()
{
    // No null check in the original.
    _delegate->interstitialDidEnd(InterstitialStatusStillLoading);
}

// @00648230
void AdController::rewardedVideoDidUpdateStatus()
{
    if (_delegate)
    {
        _delegate->rewardedVideoDidUpdateStatus(RewardedVideoStatusRewarded);
    }
}

// @0064824c
void AdController::rewardedVideoDidFail()
{
    if (_delegate)
    {
        _delegate->rewardedVideoDidUpdateStatus(RewardedVideoStatusFailed);
    }
}

// @00648268
void AdController::rewardedVideoDidExpire()
{
    if (_delegate)
    {
        _delegate->rewardedVideoDidUpdateStatus(RewardedVideoStatusExpired);
    }
}

// @0064834c
void AdController::dispatchBannerShown(int bannerHeight)
{
    Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("banner_shown", &bannerHeight);
}

// @00648408
AdController::AdController()
    : _forceInterstitial(false)
    , _bannerIsShowing(false)
    , _elapsedGameplayTime(0.0f)
    , _delegate(nullptr)
{
}

// @0064841c
AdController::~AdController()
{
}

// @00648420
bool AdController::init()
{
    _adsRemoved = UserDefault::getInstance()->getBoolForKey("remove_ads");
    _interstitialInterval = globals::advertising::firstInterstitialIntervalSeconds;
    if (_forceInterstitial)
    {
        _adsRemoved = false;
        _interstitialInterval = 0.0f;
    }
    else if (_adsRemoved)
    {
        return true;
    }
    initJavaAdHelperViaJNI();
    return true;
}

// @00648484
void AdController::initJavaAdHelperViaJNI()
{
    // Android: JNI static void com.fancyforce.AdHelper.initNativeAds(). PC: no ad network.
}

// @00648564
void AdController::invokeJNIFunction(std::string methodName)
{
    // Android: JNI static void com.fancyforce.AdHelper.<methodName>(). PC: no ad network.
}

// @00648688
bool AdController::showAd(AdType type)
{
    // PC stub (see top of file). The original:
    //   Banner:        if (!_adsRemoved && !_bannerIsShowing) { Tracker "show banner"; JNI showBanner();
    //                  _bannerIsShowing = true; return true; }
    //   BannerFlagOnly:if (!_bannerIsShowing) _bannerIsShowing = true;
    //   Interstitial:  if (!_adsRemoved) { if (_interstitialInterval < _elapsedGameplayTime) {
    //                  endGameplayTimer(); Tracker "show interstitial"; JNI showInterstitial();
    //                  resetElapsedTime(); return true; } } else if (_delegate)
    //                  _delegate->interstitialDidEnd(InterstitialStatusAdsRemoved);
    //   RewardedVideo: Tracker "show rewarded video"; JNI showRewardedVideo(); (result via JNI)
    //   and false otherwise.
    if (type == AdTypeRewardedVideo)
    {
        rewardedVideoDidFail();
    }
    return false;
}

// @00648c34
bool AdController::getElapsedTimeHasExceededInterstitialInterval()
{
    return _interstitialInterval < _elapsedGameplayTime;
}

// @00648c44
void AdController::endGameplayTimer()
{
    Director::getInstance()->getScheduler()->unscheduleUpdate(this);
}

// @00648c6c
void AdController::resetElapsedTime()
{
    _elapsedGameplayTime = 0.0f;
    if (_interstitialInterval == globals::advertising::firstInterstitialIntervalSeconds)
    {
        _interstitialInterval = globals::advertising::subsequentInterstitialIntervalSeconds;
    }
}

// @00648c9c
void AdController::removeBannerAd()
{
    // Android: JNI static void com.fancyforce.AdHelper.removeBanner() first. PC: no ad network.
    _bannerIsShowing = false;
    Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("banner_removed");
}

// @00648de4
void AdController::startGameplayTimer()
{
    Director::getInstance()->getScheduler()->scheduleUpdate(this, 0, false);
}

// @00648ee4
void AdController::setAdControllerDelegate(AdControllerDelegate* delegate)
{
    _delegate = delegate;
}

// @00648eec
void AdController::update(float dt)
{
    _elapsedGameplayTime += dt;
}

// @00648efc
Size AdController::getBannerAdSize()
{
    if (_adsRemoved)
    {
        return Size(0.0f, 0.0f);
    }
    return Size(-1.0f, (1.0f / Director::getInstance()->getContentScaleFactor()) * 100.0f);
}

// @00648f50
bool AdController::getAdsRemoved()
{
    return _adsRemoved;
}

// @00648f58
void AdController::setAdsRemoved(bool adsRemoved)
{
    _adsRemoved = adsRemoved;
    if (adsRemoved)
    {
        removeBannerAd();
    }
}
