#pragma once

#include "IAPControllerDelegate.h"

#include <string>

// In-app purchase front end (no base class, no RTTI, arm64 sizeof 0x28; created lazily by
// Settings::getIAPController(), init() is called by AppDelegate). On Android it drives the Java
// com.fancyforce.IAPHelper (Google Play Billing) through JNI and receives results through the
// extern "C" entry points Java_com_fancyforce_IAPHelper_jniResponse (@0064982c) and
// Java_com_fancyforce_IAPHelper_jniRestorePurchasesResponse (@0064987c), which live in this TU.
//
// The only product is "remove ads" (C++ name "remove_ads", Play SKU
// "com.fancyforce.happywheels.removeads"). Buying or restoring it sets UserDefault "remove_ads" =
// true and AdController::setAdsRemoved(true); nothing else in the game is gated by a purchase.
//
// Response codes are stored first (_purchaseResponseCode / _restoreResponseCode); while the app is in
// the background they are only stored and are handled by applicationWillEnterForeground().
//
// OpenWheels (PC): platform stub. The store is unavailable: purchase/restore requests do nothing and
// no response ever arrives. See docs/modules/M10.md.
class IAPController
{
public:
    // _purchaseResponseCode = responseCode; handled now unless in the background.
    void onRemoveAdsPurchaseResponse(int responseCode);                        // @00649868
    void onRestorePurchasesResponse(int responseCode);                         // @006498b8

    IAPController();                                                           // @006498cc
    ~IAPController();                                                          // @006498e8

    // Parses the first character of str as an int (stringstream).
    int getErrorCodeFromString(std::string str);                               // @006498ec
    // _adsRemoved = UserDefault "remove_ads"; JNI IAPHelper.initIAPStuff().
    bool init();                                                               // @00649aac
    // JNI static void com.fancyforce.IAPHelper.<methodName>().
    void invokeJNIFunction(std::string methodName);                            // @00649bd0
    void setDelegate(IAPControllerDelegate* delegate);                         // @00649c58
    bool getInitialized();                                                     // @00649c60
    // JNI IAPHelper.restorePurchases().
    void restorePurchases();                                                   // @00649c68
    bool getAdsRemoved();                                                      // @00649d40
    // JNI IAPHelper.purchaseRemoveAds().
    void removeAds();                                                          // @00649d48
    void purchase(std::string productId);                                      // @00649e24 (empty)
    // Shows the result alert (purchase) or caches it in Settings (restore), notifies the delegate
    // (IAPStoreAction) and resets both response codes to -1. (Name sic.)
    void respondeToGooglePlayStoreResponseCode();                              // @00649e28
    // Shows the alert for _pendingMessage 2 (purchased) / 3 (cancelled) and clears it. Nothing sets
    // _pendingMessage in 1.1.3, so this never shows anything. Called by OptionsMenu::update. (Name sic.)
    void displayMessageStupidAssWorkaroundFuckThis();                          // @0064c1bc
    void applicationDidEnterBackground();                                      // @0064c564
    void applicationWillEnterForeground();                                     // @0064c570
    // UserDefault "remove_ads" = true + flush, AdController::setAdsRemoved(true), thank-you alert,
    // delegate->onStoreResponse(PurchaseSucceeded, "remove_ads").
    void removeAdsSuccessfullyPurchased();                                     // @0064c578
    // UserDefault "remove_ads" = true + flush, AdController::setAdsRemoved(true).
    void updateAdRelatedComponents();                                          // @0064c804

private:
    bool _inBackground;                         // +0x00
    int _purchaseResponseCode;                  // +0x04 Google Play BillingResponseCode, -1 = none
    int _restoreResponseCode;                   // +0x08 BillingResponseCode or 100..102, -1 = none
    IAPControllerDelegate* _delegate;           // +0x10
    // RE-TODO(@00649aac): set to 1 by init(), never read.
    int _unk0x18;                               // +0x18
    bool _initialized;                          // +0x1c never set to true in 1.1.3
    // RE-TODO: zeroed by the constructor, never accessed (layout placeholder).
    bool _unk0x1d;                              // +0x1d
    bool _adsRemoved;                           // +0x1e UserDefault "remove_ads" at init()
    int _pendingMessage;                        // +0x20 see displayMessageStupidAssWorkaroundFuckThis
};
