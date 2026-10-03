#include "platform/common/Localization.h"

#include <unordered_map>

#include "cocos2d.h"
#include "platform/common/BinaryPlist.h"
#include "platform/common/IOSBundle.h"

namespace Localization {
namespace {

std::unordered_map<std::string, std::string>* g_table = nullptr;

void load() {
    g_table = new std::unordered_map<std::string, std::string>();
    if (!openwheels::hasIOSBundle()) {
        cocos2d::log("Localization: no iOS bundle; UI text falls back to keys");
        return;
    }
    const std::string path = openwheels::iosBundlePath() + "Localizable.strings";
    cocos2d::Data data = cocos2d::FileUtils::getInstance()->getDataFromFile(path);
    if (data.isNull()) {
        cocos2d::log("Localization: %s not found", path.c_str());
        return;
    }
    const cocos2d::Value table = openwheels::parseBinaryPlist(data);
    if (table.getType() != cocos2d::Value::Type::MAP) {
        cocos2d::log("Localization: %s is not a binary-plist string table", path.c_str());
        return;
    }
    for (const auto& kv : table.asValueMap())
        if (kv.second.getType() == cocos2d::Value::Type::STRING) (*g_table)[kv.first] = kv.second.asString();
}

}  // namespace

const std::string& get(const std::string& key) {
    if (!g_table) load();
    auto it = g_table->find(key);
    return it == g_table->end() ? key : it->second;
}

size_t size() {
    if (!g_table) load();
    return g_table->size();
}

}  // namespace Localization
