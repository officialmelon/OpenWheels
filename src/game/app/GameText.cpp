#include "GameText.h"

#include <map>
#include <sstream>

#include "cocos2d.h"

namespace GameText {
namespace {

std::map<std::string, std::string>* g_table = nullptr;

std::string unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const char c = s[++i];
            out += (c == 'n') ? '\n' : (c == 't') ? '\t' : c;
        } else {
            out += s[i];
        }
    }
    return out;
}

void load() {
    g_table = new std::map<std::string, std::string>();
    const std::string data = cocos2d::FileUtils::getInstance()->getStringFromFile("gametext.tsv");
    if (data.empty()) {
        cocos2d::log("GameText: gametext.tsv missing - run the build (tools/re/extract_gametext.py)");
        return;
    }
    std::istringstream in(data);
    std::string line;
    while (std::getline(in, line)) {
        const size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        (*g_table)[line.substr(0, tab)] = unescape(line.substr(tab + 1));
    }
}

}  // namespace

const std::string& get(const char* key) {
    static const std::string empty;
    if (!g_table) load();
    auto it = g_table->find(key);
    if (it == g_table->end()) {
        cocos2d::log("GameText: no entry for '%s'", key);
        return empty;
    }
    return it->second;
}

}  // namespace GameText
