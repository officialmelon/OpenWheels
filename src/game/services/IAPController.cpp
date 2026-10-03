#include "IAPController.h"

#include "AdController.h"
#include "GameText.h"
#include "HWWindow.h"
#include "Settings.h"

#include "cocos2d.h"

#include <sstream>

USING_NS_CC;

// ---------------------------------------------------------------------------------------------------
// PC platform policy (see docs/modules/M10.md)
//
// On Android this class drives the Java com.fancyforce.IAPHelper (Google Play Billing) through JNI:
// init() calls IAPHelper.initIAPStuff(), removeAds() IAPHelper.purchaseRemoveAds(), restorePurchases()
// IAPHelper.restorePurchases(); the results come back through two extern "C" JNI entry points that
// lived in this file and simply forward to onRemoveAdsPurchaseResponse / onRestorePurchasesResponse:
//   Java_com_fancyforce_IAPHelper_jniResponse(code)                 @0064982c
//   Java_com_fancyforce_IAPHelper_jniRestorePurchasesResponse(code) @0064987c
// At start-up the Java helper restores existing purchases silently: an owned "remove ads" arrives as
// restore code 100 ("successful restore, but do not alert the user").
//
// There is no store on PC. The build behaves as if "remove ads" (com.fancyforce.happywheels.removeads)
// had already been bought: init() delivers that silent start-up restore (code 100) through the
// original response handler, which stores UserDefault "remove_ads" and removes the ads. Every other
// store request answers BILLING_UNAVAILABLE (3) through the original failure path, one frame later
// (like the asynchronous Java callbacks; the callers set their "busy" flags after the request).
// ---------------------------------------------------------------------------------------------------

namespace
{
// Google Play BillingResponseCode values / IAPHelper restore codes used by the PC stub.
const int kBillingUnavailable = 3;
const int kRestoreSilentSuccess = 100;
} // namespace

// Product name passed to the delegate (and the UserDefault key read by init()).
static const std::string kRemoveAdsProductId = "remove_ads";  // @00ac6650 (_INIT_20)

// @00649868
void IAPController::onRemoveAdsPurchaseResponse(int responseCode)
{
    _purchaseResponseCode = responseCode;
    if (_inBackground)
    {
        return;
    }
    respondeToGooglePlayStoreResponseCode();
}

// @006498b8
void IAPController::onRestorePurchasesResponse(int responseCode)
{
    _restoreResponseCode = responseCode;
    if (_inBackground)
    {
        return;
    }
    respondeToGooglePlayStoreResponseCode();
}

// @006498cc
IAPController::IAPController()
    : _inBackground(false)
    , _purchaseResponseCode(-1)
    , _restoreResponseCode(-1)
    , _delegate(nullptr)
    , _unk0x18(0)
    , _initialized(false)
    , _unk0x1d(false)
    , _adsRemoved(false)
    , _pendingMessage(0)
{
}

// @006498e8
IAPController::~IAPController()
{
}

// @006498ec
int IAPController::getErrorCodeFromString(std::string str)
{
    char firstChar = str.at(0);
    std::stringstream stream;
    stream << firstChar;
    unsigned int errorCode;
    stream >> errorCode;
    return errorCode;
}

// @00649aac
bool IAPController::init()
{
    _adsRemoved = UserDefault::getInstance()->getBoolForKey(kRemoveAdsProductId.c_str());
    _unk0x18 = 1;
    _purchaseResponseCode = -1;
    // Android: JNI static void com.fancyforce.IAPHelper.initIAPStuff(), whose start-up restore reports
    // an owned "remove ads" as restore code 100. PC: the entitlement is owned (see top of file).
    onRestorePurchasesResponse(kRestoreSilentSuccess);
    return true;
}

// @00649bd0
void IAPController::invokeJNIFunction(std::string methodName)
{
    // Android: JNI static void com.fancyforce.IAPHelper.<methodName>(). PC: no store.
}

// @00649c58
void IAPController::setDelegate(IAPControllerDelegate* delegate)
{
    _delegate = delegate;
}

// @00649c60
bool IAPController::getInitialized()
{
    return _initialized;
}

// @00649c68
void IAPController::restorePurchases()
{
    // Android: JNI static void com.fancyforce.IAPHelper.restorePurchases(); the result arrives later.
    // PC: the store is unavailable.
    Director::getInstance()->getScheduler()->performFunctionInCocosThread([this]() {
        onRestorePurchasesResponse(kBillingUnavailable);
    });
}

// @00649d40
bool IAPController::getAdsRemoved()
{
    return _adsRemoved;
}

// @00649d48
void IAPController::removeAds()
{
    // Android: JNI static void com.fancyforce.IAPHelper.purchaseRemoveAds(); the result arrives later.
    // PC: the store is unavailable.
    Director::getInstance()->getScheduler()->performFunctionInCocosThread([this]() {
        onRemoveAdsPurchaseResponse(kBillingUnavailable);
    });
}

// @00649e24
void IAPController::purchase(std::string productId)
{
}

// @00649e28
void IAPController::respondeToGooglePlayStoreResponseCode()
{
    HWWindow* window;

    switch (_purchaseResponseCode)
    {
    case 0: // OK
        removeAdsSuccessfullyPurchased();
        break;
    case 1: // USER_CANCELED
        window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        window->showAlertMessage("purchase cancelled", "'remove ads' purchased was cancelled.", "ok", "", true);
        if (_delegate)
        {
            _delegate->onStoreResponse(IAPStoreActionPurchaseCancelled, kRemoveAdsProductId);
        }
        break;
    case 2: // SERVICE_UNAVAILABLE
        window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        window->showAlertMessage("could not connect", OW_GAMETEXT(iap_network_down, 0x003f6c9c), "ok", "", true);
        if (_delegate)
        {
            _delegate->onStoreResponse(IAPStoreActionPurchaseFailed, kRemoveAdsProductId);
        }
        break;
    case 3: // BILLING_UNAVAILABLE
        window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        window->showAlertMessage("error", OW_GAMETEXT(iap_billing_unsupported_item, 0x003f6cc7), "ok", "", true);
        if (_delegate)
        {
            _delegate->onStoreResponse(IAPStoreActionPurchaseFailed, kRemoveAdsProductId);
        }
        break;
    case 4: // ITEM_UNAVAILABLE
        window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        window->showAlertMessage("hmmm", "it seems this item is not available.", "ok", "", true);
        if (_delegate)
        {
            _delegate->onStoreResponse(IAPStoreActionPurchaseFailed, kRemoveAdsProductId);
        }
        break;
    case 5: // DEVELOPER_ERROR
        window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        window->showAlertMessage("developer error", OW_GAMETEXT(iap_developer_error, 0x003f844a), "ok", "", true);
        if (_delegate)
        {
            _delegate->onStoreResponse(IAPStoreActionPurchaseFailed, kRemoveAdsProductId);
        }
        break;
    case 6: // ERROR
        window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        window->showAlertMessage("error", OW_GAMETEXT(iap_purchase_error, 0x003feec4), "ok", "", true);
        if (_delegate)
        {
            _delegate->onStoreResponse(IAPStoreActionPurchaseFailed, kRemoveAdsProductId);
        }
        break;
    case 7: // ITEM_ALREADY_OWNED
    {
        window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        window->showAlertMessage("already owned", OW_GAMETEXT(iap_already_owned, 0x003feef5), "ok", "", true);
        UserDefault* userDefault = UserDefault::getInstance();
        userDefault->setBoolForKey("remove_ads", true);
        userDefault->flush();
        Settings::getInstance()->getAdController()->setAdsRemoved(true);
        if (_delegate)
        {
            _delegate->onStoreResponse(IAPStoreActionAlreadyOwned, kRemoveAdsProductId);
        }
        break;
    }
    case 8: // ITEM_NOT_OWNED
        window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        window->showAlertMessage("weird error", OW_GAMETEXT(iap_error_code_8, 0x0040a331), "ok", "", true);
        if (_delegate)
        {
            _delegate->onStoreResponse(IAPStoreActionPurchaseFailed, kRemoveAdsProductId);
        }
        break;
    }

    // Restore results are cached in Settings and shown by OptionsMenu / PauseLayer.
    switch (_restoreResponseCode)
    {
    case 0: // OK
        Settings::getInstance()->cacheAlertMessage("restore purchases success",
                                                   OW_GAMETEXT(iap_restore_success, 0x003fe25b), "ok", "", true);
        if (!_delegate)
        {
            break;
        }
        _delegate->onStoreResponse(IAPStoreActionRestoreSucceeded, kRemoveAdsProductId);
        break;
    case 1: // USER_CANCELED
        Settings::getInstance()->cacheAlertMessage("restore purchases cancelled", "restore purchases was cancelled.",
                                                   "ok", "", true);
        if (!_delegate)
        {
            break;
        }
        _delegate->onStoreResponse(IAPStoreActionRestoreCancelled, kRemoveAdsProductId);
        break;
    case 2: // SERVICE_UNAVAILABLE
        Settings::getInstance()->cacheAlertMessage("could not connect", OW_GAMETEXT(iap_network_down, 0x003f6c9c),
                                                   "ok", "", true);
        if (!_delegate)
        {
            break;
        }
        _delegate->onStoreResponse(IAPStoreActionRestoreFailed, kRemoveAdsProductId);
        break;
    case 3: // BILLING_UNAVAILABLE
        Settings::getInstance()->cacheAlertMessage("error", OW_GAMETEXT(iap_billing_unsupported_restore, 0x003fb218),
                                                   "ok", "", true);
        if (!_delegate)
        {
            break;
        }
        _delegate->onStoreResponse(IAPStoreActionRestoreFailed, kRemoveAdsProductId);
        break;
    case 4: // ITEM_UNAVAILABLE
        Settings::getInstance()->cacheAlertMessage("hmmm", OW_GAMETEXT(iap_restore_unavailable, 0x00414d20), "ok", "",
                                                   true);
        if (!_delegate)
        {
            break;
        }
        _delegate->onStoreResponse(IAPStoreActionRestoreFailed, kRemoveAdsProductId);
        break;
    case 5: // DEVELOPER_ERROR
        Settings::getInstance()->cacheAlertMessage("developer error", OW_GAMETEXT(iap_developer_error, 0x003f844a),
                                                   "ok", "", true);
        if (!_delegate)
        {
            break;
        }
        _delegate->onStoreResponse(IAPStoreActionRestoreFailed, kRemoveAdsProductId);
        break;
    case 6: // ERROR
        Settings::getInstance()->cacheAlertMessage("error", OW_GAMETEXT(iap_restore_error, 0x0040bae0), "ok", "",
                                                   true);
        if (!_delegate)
        {
            break;
        }
        _delegate->onStoreResponse(IAPStoreActionRestoreFailed, kRemoveAdsProductId);
        break;
    case 7: // ITEM_ALREADY_OWNED
    {
        Settings::getInstance()->cacheAlertMessage("weird error", OW_GAMETEXT(iap_restore_weird_error_2, 0x003f8480),
                                                   "ok", "", true);
        UserDefault* userDefault = UserDefault::getInstance();
        userDefault->setBoolForKey("remove_ads", true);
        userDefault->flush();
        Settings::getInstance()->getAdController()->setAdsRemoved(true);
        if (!_delegate)
        {
            break;
        }
        _delegate->onStoreResponse(IAPStoreActionRestoreSucceeded, kRemoveAdsProductId);
        break;
    }
    case 8: // ITEM_NOT_OWNED
        Settings::getInstance()->cacheAlertMessage("weird error", OW_GAMETEXT(iap_error_code_8, 0x0040a331), "ok", "",
                                                   true);
        if (!_delegate)
        {
            break;
        }
        _delegate->onStoreResponse(IAPStoreActionRestoreFailed, kRemoveAdsProductId);
        break;
    case 100: // IAPHelper: purchase found, restore silently
    {
        UserDefault* userDefault = UserDefault::getInstance();
        userDefault->setBoolForKey("remove_ads", true);
        userDefault->flush();
        Settings::getInstance()->getAdController()->setAdsRemoved(true);
        break;
    }
    case 101: // IAPHelper: no purchases
        Settings::getInstance()->cacheAlertMessage("no purchases made", OW_GAMETEXT(iap_no_purchases, 0x003f6d03),
                                                   "ok", "", true);
        if (!_delegate)
        {
            break;
        }
        _delegate->onStoreResponse(IAPStoreActionRestoreSucceeded, kRemoveAdsProductId);
        break;
    case 102: // IAPHelper: query failed
        Settings::getInstance()->cacheAlertMessage("we're sorry", OW_GAMETEXT(iap_restore_went_wrong, 0x00412459),
                                                   "ok", "", true);
        if (!_delegate)
        {
            break;
        }
        _delegate->onStoreResponse(IAPStoreActionRestoreFailed, kRemoveAdsProductId);
        break;
    }

    _purchaseResponseCode = -1;
    _restoreResponseCode = -1;
}

// @0064c1bc
void IAPController::displayMessageStupidAssWorkaroundFuckThis()
{
    HWWindow* window;
    if (_pendingMessage == 2)
    {
        window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        window->showAlertMessage("remove ads purchased success.", "thank you for your support!", "yeah, whatever", "",
                                 true);
    }
    else if (_pendingMessage == 3)
    {
        window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        window->showAlertMessage("purchase cancelled", OW_GAMETEXT(iap_purchase_cancelled_unknown, 0x003fb1b8), "ok",
                                 "", true);
    }
    else
    {
        return;
    }
    _pendingMessage = 0;
}

// @0064c564
void IAPController::applicationDidEnterBackground()
{
    _inBackground = true;
}

// @0064c570
void IAPController::applicationWillEnterForeground()
{
    _inBackground = false;
    respondeToGooglePlayStoreResponseCode();
}

// @0064c578
void IAPController::removeAdsSuccessfullyPurchased()
{
    UserDefault* userDefault = UserDefault::getInstance();
    userDefault->setBoolForKey("remove_ads", true);
    userDefault->flush();
    Settings::getInstance()->getAdController()->setAdsRemoved(true);
    HWWindow* window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
    window->showAlertMessage("remove ads purchased", "thank you for your support!", "yeah, whatever", "", true);
    if (_delegate)
    {
        _delegate->onStoreResponse(IAPStoreActionPurchaseSucceeded, kRemoveAdsProductId);
    }
}

// @0064c804
void IAPController::updateAdRelatedComponents()
{
    UserDefault* userDefault = UserDefault::getInstance();
    userDefault->setBoolForKey("remove_ads", true);
    userDefault->flush();
    Settings::getInstance()->getAdController()->setAdsRemoved(true);
}
