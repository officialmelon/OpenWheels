#pragma once

// Results the ad layer (AdController, fed by the Java AdHelper through JNI on Android) reports to its
// delegate. The numeric values are from the binary; the enumerator names are not in it and were chosen
// from the behaviour.
//
// Where each value comes from (Android 1.1.3):
//   0  AdController::interstitialDidFail(); Java jniInterstitialDidEnd(0) ("interstitial is null")
//      and jniInterstitialDidEnd(4) (no network connectivity) are both folded into 0 by the JNI glue.
//   1  AdController::interstitialDidEnd(); Java 1 (ad dismissed by the player).
//   2  AdController::showAd(AdTypeInterstitial) while ads are removed (sent synchronously).
//   3  AdController::interstitialDidExpire(); Java 3.
//   4  never delivered to the delegate (see 0).
//   5  AdController::interstitialStillLoading(); Java 5 (ad not ready yet).
// Gameplay::interstitialDidEnd treats 1..3 as "done, reset the interval timer", 0 as "failed: show
// the internal ad if the interval has elapsed", anything else as "just start playing".
// RE-TODO(@005bbaf4): enumerator names are invented.
enum InterstitialStatus
{
    InterstitialStatusFailed = 0,
    InterstitialStatusEnded = 1,
    InterstitialStatusAdsRemoved = 2,
    InterstitialStatusExpired = 3,
    InterstitialStatusNoConnection = 4,
    InterstitialStatusStillLoading = 5,
};

// Rewarded video ("revive the necromancer" offer of the Mascot).
//   0  AdController::rewardedVideoDidFail(); Java 0 (failure) and 4 (no connectivity) fold into 0.
//   1  AdController::rewardedVideoDidUpdateStatus(); Java 1 (watched to the end, reward granted).
//   3  AdController::rewardedVideoDidExpire(); Java 3.
//   4  tested by Mascot::rewardedVideoComplete (ignored there) but never delivered by the JNI glue.
// Java status 7 ("displayed") is dropped by the JNI glue.
// RE-TODO(@005ebb54): enumerator names are invented; 2 is never used.
enum RewardedVideoStatus
{
    RewardedVideoStatusFailed = 0,
    RewardedVideoStatusRewarded = 1,
    RewardedVideoStatusExpired = 3,
    RewardedVideoStatusNoConnection = 4,
};

// Listener interface of AdController (implemented by Gameplay at +0x320 and Mascot at +0x300).
//
// arm64: a bare vptr (sizeof 8), no virtual destructor. The vtable has exactly these two slots in this
// order (secondary vtables of Gameplay and Mascot):
//   [0] interstitialDidEnd(InterstitialStatus)
//   [1] rewardedVideoDidUpdateStatus(RewardedVideoStatus)
// Both have empty default bodies: Gameplay keeps the default rewardedVideoDidUpdateStatus, Mascot the
// default interstitialDidEnd. The out-of-line copies in the binary are COMDAT instances of these
// inline definitions.
class AdControllerDelegate
{
public:
    // @005ec7ac (COMDAT copy, Mascot TU)
    virtual void interstitialDidEnd(InterstitialStatus status) {}

    // @005bcf34 (COMDAT copy, Gameplay TU)
    virtual void rewardedVideoDidUpdateStatus(RewardedVideoStatus status) {}
};
