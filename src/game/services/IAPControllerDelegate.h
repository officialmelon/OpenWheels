#pragma once

#include <string>

// What IAPController::respondeToGooglePlayStoreResponseCode() / removeAdsSuccessfullyPurchased() report
// to the delegate. The only product is "remove_ads" (Google Play SKU
// "com.fancyforce.happywheels.removeads"); the product string passed along is always "remove_ads".
// Values are from the binary; the names are not in it.
// Google Play BillingResponseCode -> action:
//   purchase:  0 OK -> 2, 1 USER_CANCELED -> 3, 7 ITEM_ALREADY_OWNED -> 4 (ads get removed),
//              2..6 and 8 -> 1
//   restore:   0 OK -> 5, 7 -> 5 (ads get removed), 101 ("no purchases made") -> 5, 1 -> 7,
//              2..6, 8 and 102 -> 6; 100 (purchase found) removes the ads without notifying.
// Both implementers (OptionsMenu, PauseLayer) only clear their "waiting for the store" flag, so the
// value itself is never inspected in 1.1.3.
// RE-TODO(@00649e28): enumerator names are invented; 0 is never sent.
enum IAPStoreAction
{
    IAPStoreActionPurchaseFailed = 1,
    IAPStoreActionPurchaseSucceeded = 2,
    IAPStoreActionPurchaseCancelled = 3,
    IAPStoreActionAlreadyOwned = 4,
    IAPStoreActionRestoreSucceeded = 5,
    IAPStoreActionRestoreFailed = 6,
    IAPStoreActionRestoreCancelled = 7,
};

// Listener interface of IAPController (implemented by OptionsMenu at +0x358 and PauseLayer at +0x320).
//
// arm64: a bare vptr (sizeof 8), no virtual destructor; the vtable has this single slot (secondary
// vtables of OptionsMenu and PauseLayer). No base implementation exists in the binary.
class IAPControllerDelegate
{
public:
    virtual void onStoreResponse(IAPStoreAction action, std::string productId) = 0;
};
