#pragma once
// PC stand-in for the sdkbox Google Analytics plugin used by the Android build. Analytics are
// not collected on PC: events are dropped.

#include <string>

namespace sdkbox {

class PluginGoogleAnalytics {
public:
    static void logEvent(const std::string& eventCategory, const std::string& eventAction,
                         const std::string& eventLabel, int value) {
        (void)eventCategory; (void)eventAction; (void)eventLabel; (void)value;
    }
};

}  // namespace sdkbox
