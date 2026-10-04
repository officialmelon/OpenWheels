// ONLINE (PC addition): see FlashReplay.h.
#include "online/replays/FlashReplay.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

#include "base/base64.h"
#include "cocos2d.h"
#include "online/HWApi.h"
#include "online/account/TjfAccount.h"
#include "tinyxml2/tinyxml2.h"

USING_NS_CC;

namespace online {
namespace replays {

float ReplayInfo::averageRating() const {
    // ReplayDataObject.getAverageRating: k = 10 prior votes of 2.5.
    const float k = 10.0f, prior = 2.5f;
    if (votes == 0) return 0.0f;
    const float v = (float)votes;
    const float avg = (rating - prior * k / (v + k)) / (v / (v + k));
    return std::min(5.0f, std::max(avg, 0.0f));
}

ReplayInput ReplayInput::fromBytes(const std::string& bytes) {
    ReplayInput in;
    // parseByteArray: the first 0xFF splits keys and mouse data; "if(!_loc2_)" also treats a 0xFF
    // at index 0 as "no mouse data".
    size_t sep = 0;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if ((uint8_t)bytes[i] == 255) {
            sep = i;
            break;
        }
    }
    if (!sep) {
        in.keys.assign(bytes.begin(), bytes.end());
        return in;
    }
    in.keys.assign(bytes.begin(), bytes.begin() + sep);
    for (size_t p = sep + 1; p + 4 <= bytes.size(); p += 4) {
        const int a = ((uint8_t)bytes[p] << 8) | (uint8_t)bytes[p + 1];
        const int b = ((uint8_t)bytes[p + 2] << 8) | (uint8_t)bytes[p + 3];
        MouseEntry e;
        e.rollOut = a > 32767;
        e.iteration = e.rollOut ? a - 32768 : a;
        e.triggerIndex = b;
        in.mouse.push_back(e);
    }
    return in;
}

std::string ReplayInput::toBytes() const {
    std::string out;
    out.reserve(keys.size() + 1 + mouse.size() * 4);
    for (uint8_t k : keys) {
        // A frame with all eight keys down would read back as the separator; drop its eject bit.
        out += (char)(k == 255 ? 254 : k);
    }
    if (!mouse.empty()) {
        out += (char)255;
        for (const MouseEntry& e : mouse) {
            int a = std::min(e.iteration, 32767);
            if (e.rollOut) a += 32768;
            const int b = std::min(e.triggerIndex, 65535);
            out += (char)((a >> 8) & 0xff);
            out += (char)(a & 0xff);
            out += (char)((b >> 8) & 0xff);
            out += (char)(b & 0xff);
        }
    }
    return out;
}

uint8_t flashToMobile(uint8_t f) {
    uint8_t m = 0;
    if (f & 0x80) m |= 0x08;  // left  -> lean back
    if (f & 0x40) m |= 0x04;  // right -> lean forward
    if (f & 0x20) m |= 0x01;  // up    -> forward
    if (f & 0x10) m |= 0x02;  // down  -> back
    if (f & 0x08) m |= 0x10;  // space
    if (f & 0x04) m |= 0x20;  // shift
    if (f & 0x02) m |= 0x40;  // ctrl
    if (f & 0x01) m |= 0x80;  // z (eject)
    return m;
}

uint8_t mobileToFlash(uint8_t m) {
    uint8_t f = 0;
    if (m & 0x08) f |= 0x80;
    if (m & 0x04) f |= 0x40;
    if (m & 0x01) f |= 0x20;
    if (m & 0x02) f |= 0x10;
    if (m & 0x10) f |= 0x08;
    if (m & 0x20) f |= 0x04;
    if (m & 0x40) f |= 0x02;
    if (m & 0x80) f |= 0x01;
    return f;
}

namespace {

void readReplay(tinyxml2::XMLElement* rp, ReplayInfo& r) {
    rp->QueryIntAttribute("id", &r.id);
    rp->QueryIntAttribute("li", &r.levelId);
    rp->QueryIntAttribute("ui", &r.userId);
    rp->QueryFloatAttribute("rg", &r.rating);
    rp->QueryIntAttribute("vs", &r.votes);
    rp->QueryIntAttribute("vw", &r.views);
    rp->QueryIntAttribute("pc", &r.character);
    rp->QueryIntAttribute("ct", &r.frames);
    if (const char* s = rp->Attribute("un")) r.userName = s;
    if (const char* s = rp->Attribute("dc")) r.created = s;
    if (const char* s = rp->Attribute("ar")) r.architecture = s;
    if (const char* s = rp->Attribute("vr")) r.version = s;
    if (tinyxml2::XMLElement* uc = rp->FirstChildElement("uc"))
        if (const char* s = uc->GetText()) r.comment = s;
    // TextUtils.removeSlashes
    std::string c;
    for (size_t i = 0; i < r.comment.size(); ++i) {
        if (r.comment[i] == '\\' && i + 1 < r.comment.size()) ++i;
        c += r.comment[i];
    }
    r.comment = c;
}

}  // namespace

bool parseReplayList(const std::string& body, std::vector<ReplayInfo>& replays, int* page, int* perPage,
                     std::string* error) {
    replays.clear();
    if (HWApi::isServerError(body, error)) return false;
    if (body.empty()) {
        if (error) *error = "empty answer";
        return false;
    }
    tinyxml2::XMLDocument doc;
    if (doc.Parse(body.c_str()) != tinyxml2::XML_SUCCESS || !doc.RootElement()) {
        if (error) *error = "unreadable replay list";
        return false;
    }
    tinyxml2::XMLElement* root = doc.RootElement();
    if (page) {
        *page = 1;
        root->QueryIntAttribute("pg", page);
    }
    if (perPage) {
        *perPage = 0;
        root->QueryIntAttribute("pp", perPage);
    }
    for (tinyxml2::XMLElement* rp = root->FirstChildElement("rp"); rp; rp = rp->NextSiblingElement("rp")) {
        ReplayInfo r;
        readReplay(rp, r);
        if (r.id) replays.push_back(r);
    }
    return true;
}

bool parseCombined(const std::string& body, ReplayInfo& replay, OnlineLevelInfo& level, std::string* error) {
    if (HWApi::isServerError(body, error)) return false;
    tinyxml2::XMLDocument doc;
    if (doc.Parse(body.c_str()) != tinyxml2::XML_SUCCESS || !doc.RootElement()) {
        if (error) *error = "unreadable replay data";
        return false;
    }
    tinyxml2::XMLElement* rp = doc.RootElement()->FirstChildElement("rp");
    tinyxml2::XMLElement* lv = doc.RootElement()->FirstChildElement("lv");
    if (!rp || !lv) {
        if (error) *error = "replay not found";
        return false;
    }
    readReplay(rp, replay);
    tinyxml2::XMLPrinter printer;
    printer.OpenElement("lvs");
    lv->Accept(&printer);
    printer.CloseElement();
    std::vector<OnlineLevelInfo> levels;
    if (!HWApi::parseLevelList(printer.CStr(), levels, nullptr, nullptr, error) || levels.empty()) return false;
    level = levels.front();
    if (!replay.levelId) replay.levelId = level.id;
    return true;
}

bool splitCombinedRecord(const std::string& body, std::string& replayBytes, std::string& levelRecord,
                         std::string* error) {
    // RecordLoader.loadComplete: "<html>" / "failure" in the first 8 bytes are errors.
    const std::string head = body.substr(0, 8);
    if (head.find("<html>") != std::string::npos) {
        if (error) *error = "server returned an error page";
        return false;
    }
    if (head.find("failure") != std::string::npos) {
        if (error) *error = "server: " + body.substr(0, 80);
        return false;
    }
    if (body.size() < 4) {
        if (error) *error = "replay data is damaged";
        return false;
    }
    const uint32_t n = ((uint32_t)(uint8_t)body[0] << 24) | ((uint32_t)(uint8_t)body[1] << 16) |
                       ((uint32_t)(uint8_t)body[2] << 8) | (uint32_t)(uint8_t)body[3];
    if (n > body.size() - 4) {
        if (error) *error = "replay data is damaged";
        return false;
    }
    replayBytes = body.substr(4, n);
    levelRecord = body.substr(4 + n);
    return true;
}

std::string as3Escape(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::u32string u32;
    if (!StringUtils::UTF8ToUTF32(s, u32)) return std::string();
    std::string out;
    for (char32_t c : u32) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '@' || c == '-' ||
            c == '_' || c == '.' || c == '*' || c == '+' || c == '/') {
            out += (char)c;
        } else if (c < 256) {
            out += '%';
            out += hex[(c >> 4) & 15];
            out += hex[c & 15];
        } else {
            char buf[8];
            std::snprintf(buf, sizeof buf, "%%u%04X", (unsigned)(c & 0xffff));
            out += buf;
        }
    }
    return out;
}

std::string formatTime(int frames) {
    const float s = frames / 30.0f;
    char buf[32];
    if (s >= 60.0f) {
        const int m = (int)(s / 60.0f);
        std::snprintf(buf, sizeof buf, "%d:%05.2f", m, s - m * 60.0f);
    } else {
        std::snprintf(buf, sizeof buf, "%.2f s", s);
    }
    return buf;
}

// ---- saved runs --------------------------------------------------------------------------------

namespace {

std::string localDir() { return account::storageDir() + "replays/local/"; }

std::string b64(const std::string& bytes) {
    char* out = nullptr;
    const int n = base64Encode(reinterpret_cast<const unsigned char*>(bytes.data()), (unsigned)bytes.size(), &out);
    std::string s = out ? std::string(out, n) : std::string();
    free(out);
    return s;
}

std::string unb64(const std::string& text) {
    unsigned char* out = nullptr;
    const int n = base64Decode(reinterpret_cast<const unsigned char*>(text.data()), (unsigned)text.size(), &out);
    std::string s = out ? std::string(reinterpret_cast<char*>(out), n) : std::string();
    free(out);
    return s;
}

}  // namespace

bool saveRun(SavedRun& run, std::string* error) {
    FileUtils* fu = FileUtils::getInstance();
    fu->createDirectory(localDir());
    if (run.file.empty()) {
        const time_t t = time(nullptr);
        char stamp[32];
        std::strftime(stamp, sizeof stamp, "%Y%m%d_%H%M%S", std::localtime(&t));
        run.file = localDir() + std::to_string(run.levelId) + "_" + stamp + ".owreplay";
        if (run.date.empty()) {
            char d[32];
            std::strftime(d, sizeof d, "%Y-%m-%d %H:%M", std::localtime(&t));
            run.date = d;
        }
    }
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement* root = doc.NewElement("owreplay");
    doc.InsertFirstChild(root);
    root->SetAttribute("v", 1);
    root->SetAttribute("level", run.levelId);
    root->SetAttribute("levelName", run.levelName.c_str());
    root->SetAttribute("levelAuthor", run.levelAuthorId);
    root->SetAttribute("pc", run.character);
    root->SetAttribute("ct", run.frames);
    root->SetAttribute("completed", run.completed ? 1 : 0);
    root->SetAttribute("date", run.date.c_str());
    root->SetAttribute("uploaded", run.uploadedId);
    tinyxml2::XMLElement* data = doc.NewElement("rr");
    data->InsertEndChild(doc.NewText(b64(run.input.toBytes()).c_str()));
    root->InsertEndChild(data);
    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    if (!fu->writeStringToFile(printer.CStr(), run.file)) {
        if (error) *error = "couldn't write " + run.file;
        return false;
    }
    return true;
}

bool loadRun(const std::string& file, SavedRun& run) {
    const std::string text = FileUtils::getInstance()->getStringFromFile(file);
    tinyxml2::XMLDocument doc;
    if (text.empty() || doc.Parse(text.c_str()) != tinyxml2::XML_SUCCESS) return false;
    tinyxml2::XMLElement* root = doc.FirstChildElement("owreplay");
    if (!root) return false;
    run = SavedRun();
    run.file = file;
    root->QueryIntAttribute("level", &run.levelId);
    root->QueryIntAttribute("levelAuthor", &run.levelAuthorId);
    root->QueryIntAttribute("pc", &run.character);
    root->QueryIntAttribute("ct", &run.frames);
    int completed = 0;
    root->QueryIntAttribute("completed", &completed);
    run.completed = completed != 0;
    root->QueryIntAttribute("uploaded", &run.uploadedId);
    if (const char* s = root->Attribute("levelName")) run.levelName = s;
    if (const char* s = root->Attribute("date")) run.date = s;
    if (tinyxml2::XMLElement* rr = root->FirstChildElement("rr"))
        if (const char* s = rr->GetText()) run.input = ReplayInput::fromBytes(unb64(s));
    return true;
}

bool deleteRun(const SavedRun& run) { return !run.file.empty() && FileUtils::getInstance()->removeFile(run.file); }

std::vector<SavedRun> listSavedRuns(int levelId) {
    std::vector<SavedRun> runs;
    std::vector<std::string> files;
    FileUtils* fu = FileUtils::getInstance();
    if (!fu->isDirectoryExist(localDir())) return runs;
    fu->listFilesRecursively(localDir(), &files);
    for (const std::string& f : files) {
        if (f.size() < 9 || f.compare(f.size() - 9, 9, ".owreplay") != 0) continue;
        SavedRun run;
        if (!loadRun(f, run)) continue;
        if (levelId && run.levelId != levelId) continue;
        runs.push_back(run);
    }
    std::sort(runs.begin(), runs.end(), [](const SavedRun& a, const SavedRun& b) {
        return a.date != b.date ? a.date > b.date : a.file > b.file;
    });
    return runs;
}

}  // namespace replays
}  // namespace online
