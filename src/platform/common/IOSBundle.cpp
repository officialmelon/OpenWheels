#include "platform/common/IOSBundle.h"

namespace openwheels {
namespace {
std::string g_iosBundle;
}
void setIOSBundlePath(const std::string& dirWithSlash) { g_iosBundle = dirWithSlash; }
const std::string& iosBundlePath() { return g_iosBundle; }
}  // namespace openwheels
