#pragma once
// PC stand-in for the sdkbox "Review" plugin (store rating prompt) that the Android build links.
// Only the calls the game makes exist; on PC there is no store, so they do nothing.

namespace sdkbox {

class PluginReview {
public:
    static bool init(const char* jsonconfig = nullptr) { (void)jsonconfig; return true; }
    static void rate() {}
    static void rateInAppstore(bool showDialog) { (void)showDialog; }
};

}  // namespace sdkbox
