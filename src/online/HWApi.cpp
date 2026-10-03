#include "online/HWApi.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>

#include <openssl/blowfish.h>
#include <zlib.h>

#include "cocos2d.h"
#include "network/HttpClient.h"
#include "tinyxml2/tinyxml2.h"

USING_NS_CC;

namespace online {
namespace {

const char* const kSite = "https://totaljerkface.com/";
const char* const kUserAgent =
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/128.0 Safari/537.36";
const double kMinSpacingSeconds = 1.0;  // one request at a time, at most one per second
const char* const kKeyPrefix = "eatshit";
const unsigned char kIV[8] = {'a', 'b', 'c', 'd', '1', '2', '3', '4'};

double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else if (c == ' ') {
            out += '+';
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

std::string form(std::initializer_list<std::pair<const char*, std::string>> fields) {
    std::string body;
    for (const auto& f : fields) {
        if (!body.empty()) body += '&';
        body += f.first;
        body += '=';
        body += urlEncode(f.second);
    }
    return body;
}

const char* sortName(SortBy s) {
    switch (s) {
        case SortBy::Newest: return "newest";
        case SortBy::Oldest: return "oldest";
        case SortBy::Plays: return "plays";
        default: return "rating";
    }
}

const char* uploadedName(Uploaded u) {
    switch (u) {
        case Uploaded::Today: return "today";
        case Uploaded::Week: return "week";
        case Uploaded::Month: return "month";
        default: return "anytime";
    }
}

// TextUtils.randomNumString(4, 8): 4-8 digits; the server counts the download as a play when
// the last digit is odd.
std::string ipTracking(bool countPlay) {
    static std::mt19937 rng((unsigned)time(nullptr));
    std::uniform_int_distribution<int> len(4, 8), d09(0, 9), d19(1, 9), d04(0, 4);
    const int n = len(rng);
    std::string s(1, (char)('0' + d19(rng)));
    for (int i = 1; i < n - 1; ++i) s += (char)('0' + d09(rng));
    s += (char)('0' + d04(rng) * 2 + (countPlay ? 1 : 0));
    return s;
}

// "failure:<reason>" or an HTML error page instead of data.
bool serverError(const std::string& body, std::string* error) {
    const std::string head = body.substr(0, 64);
    if (head.find("failure") != std::string::npos) {
        if (error) *error = "server: " + body.substr(0, 80);
        return true;
    }
    if (head.find("<html") != std::string::npos || head.find("<!DOCTYPE") != std::string::npos) {
        if (error) *error = "server returned an error page";
        return true;
    }
    return false;
}

std::string cacheDir() { return FileUtils::getInstance()->getWritablePath() + "online/"; }

bool matches(const OnlineLevelInfo& l, const LevelQuery& q) {
    if (q.searchBy == SearchBy::None || q.term.empty()) return true;
    std::string hay = q.searchBy == SearchBy::Name ? l.name : l.authorName;
    std::string needle = q.term;
    std::transform(hay.begin(), hay.end(), hay.begin(), ::tolower);
    std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
    return hay.find(needle) != std::string::npos;
}

void sortLevels(std::vector<OnlineLevelInfo>& v, SortBy s) {
    std::stable_sort(v.begin(), v.end(), [s](const OnlineLevelInfo& a, const OnlineLevelInfo& b) {
        switch (s) {
            case SortBy::Newest: return a.published > b.published;
            case SortBy::Oldest: return a.published < b.published;
            case SortBy::Plays: return a.plays > b.plays;
            default: return a.rating > b.rating;
        }
    });
}

}  // namespace

struct HWApi::Pending {
    std::string endpoint;
    std::string body;
    std::function<void(bool, const std::string&, const std::string&)> done;
};

HWApi* HWApi::getInstance() {
    static HWApi* instance = new HWApi();
    return instance;
}

HWApi::HWApi() {
    if (const char* f = std::getenv("OW_ONLINE_FIXTURES")) {
        _fixtures = f;
        if (!_fixtures.empty() && _fixtures.back() != '/' && _fixtures.back() != '\\') _fixtures += '/';
        log("HWApi: offline fixtures from %s", _fixtures.c_str());
    }
}

RequestId HWApi::nextId() { return _nextId++; }

void HWApi::cancel(RequestId id) {
    if (id) _cancelled.push_back(id);
}

void HWApi::cancelAll() {
    for (RequestId id = 1; id < _nextId; ++id) _cancelled.push_back(id);
}

void HWApi::post(const std::string& endpoint, const std::string& body,
                 std::function<void(bool, const std::string&, const std::string&)> done) {
    _queue.push_back(new Pending{endpoint, body, std::move(done)});
    pump();
}

void HWApi::pump() {
    if (_busy || _queue.empty()) return;
    const double wait = kMinSpacingSeconds - (now() - _lastRequestTime);
    if (wait > 0) {
        Director::getInstance()->getScheduler()->schedule([this](float) { pump(); }, this, 0.0f, 0,
                                                          (float)wait, false, "hwapi_pump");
        return;
    }
    Pending* p = _queue.front();
    _queue.erase(_queue.begin());
    _busy = true;

    auto* request = new network::HttpRequest();
    request->setUrl(std::string(kSite) + p->endpoint);
    request->setRequestType(network::HttpRequest::Type::POST);
    request->setHeaders({kUserAgent, "Content-Type: application/x-www-form-urlencoded",
                         std::string("Referer: ") + kSite + "happy_wheels.tjf"});
    request->setRequestData(p->body.data(), p->body.size());
    request->setResponseCallback([this, p](network::HttpClient*, network::HttpResponse* response) {
        _busy = false;
        _lastRequestTime = now();
        std::string data;
        if (response && response->getResponseData())
            data.assign(response->getResponseData()->begin(), response->getResponseData()->end());
        if (!response || !response->isSucceed()) {
            std::string err = "network error";
            if (response && response->getErrorBuffer() && response->getErrorBuffer()[0])
                err += std::string(": ") + response->getErrorBuffer();
            else if (response)
                err += " (HTTP " + std::to_string(response->getResponseCode()) + ")";
            p->done(false, err, data);
        } else {
            p->done(true, std::string(), data);
        }
        delete p;
        pump();
    });
    network::HttpClient::getInstance()->setTimeoutForConnect(15);
    network::HttpClient::getInstance()->setTimeoutForRead(60);
    network::HttpClient::getInstance()->send(request);
    request->release();
}

bool HWApi::parseLevelList(const std::string& body, std::vector<OnlineLevelInfo>& levels, int* page,
                           int* perPage, std::string* error) {
    levels.clear();
    if (serverError(body, error)) return false;
    tinyxml2::XMLDocument doc;
    if (doc.Parse(body.c_str()) != tinyxml2::XML_SUCCESS) {
        if (error) *error = "unreadable level list";
        return false;
    }
    tinyxml2::XMLElement* lvs = doc.FirstChildElement("lvs");
    if (!lvs) {
        if (error) *error = "unreadable level list";
        return false;
    }
    if (page) {
        *page = 1;
        lvs->QueryIntAttribute("pg", page);
    }
    if (perPage) {
        *perPage = 0;
        lvs->QueryIntAttribute("pp", perPage);
    }
    for (tinyxml2::XMLElement* lv = lvs->FirstChildElement("lv"); lv; lv = lv->NextSiblingElement("lv")) {
        OnlineLevelInfo l;
        lv->QueryIntAttribute("id", &l.id);
        lv->QueryIntAttribute("ui", &l.authorId);
        lv->QueryFloatAttribute("rg", &l.rating);
        lv->QueryIntAttribute("vs", &l.votes);
        lv->QueryIntAttribute("ps", &l.plays);
        lv->QueryIntAttribute("pc", &l.character);
        if (const char* s = lv->Attribute("ln")) l.name = s;
        if (const char* s = lv->Attribute("un")) l.authorName = s;
        if (const char* s = lv->Attribute("dp")) l.published = s;
        if (tinyxml2::XMLElement* uc = lv->FirstChildElement("uc"))
            if (const char* s = uc->GetText()) l.comment = s;
        if (l.id) levels.push_back(l);
    }
    return true;
}

bool HWApi::decryptRecord(const std::string& record, int authorId, std::string& xml, std::string* error) {
    std::string err;
    if (serverError(record, &err)) {
        if (error) *error = err;
        return false;
    }
    if (record.empty() || record.size() % 8 != 0) {
        if (error) *error = "level data is damaged";
        return false;
    }
    const std::string key = kKeyPrefix + std::to_string(authorId);
    BF_KEY bfKey;
    BF_set_key(&bfKey, (int)key.size(), reinterpret_cast<const unsigned char*>(key.data()));
    std::vector<unsigned char> plain(record.size());
    unsigned char iv[8];
    std::memcpy(iv, kIV, 8);
    BF_cbc_encrypt(reinterpret_cast<const unsigned char*>(record.data()), plain.data(), (long)record.size(), &bfKey,
                   iv, BF_DECRYPT);
    const unsigned pad = plain.back();
    if (pad < 1 || pad > 8) {
        if (error) *error = "level data could not be decrypted";
        return false;
    }
    for (size_t i = plain.size() - pad; i < plain.size(); ++i) {
        if (plain[i] != pad) {
            if (error) *error = "level data could not be decrypted";
            return false;
        }
    }
    plain.resize(plain.size() - pad);

    z_stream zs;
    std::memset(&zs, 0, sizeof zs);
    if (inflateInit(&zs) != Z_OK) return false;
    zs.next_in = plain.data();
    zs.avail_in = (uInt)plain.size();
    std::string out;
    char buf[65536];
    int rc;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(buf);
        zs.avail_out = sizeof buf;
        rc = inflate(&zs, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END) break;
        out.append(buf, sizeof buf - zs.avail_out);
    } while (rc != Z_STREAM_END);
    inflateEnd(&zs);
    if (rc != Z_STREAM_END) {
        if (error) *error = "level data could not be unpacked";
        return false;
    }
    xml.swap(out);
    return true;
}

RequestId HWApi::listLevels(const LevelQuery& query, ListCallback callback) {
    const RequestId id = nextId();
    auto deliver = [this, id, callback](bool ok, const std::string& err, const std::vector<OnlineLevelInfo>& v,
                                        int pg, int pp) {
        if (std::find(_cancelled.begin(), _cancelled.end(), id) != _cancelled.end()) return;
        callback(ok, err, v, pg, pp);
    };
    if (!_fixtures.empty()) {
        // every list_*.xml fixture, filtered and sorted locally
        std::vector<OnlineLevelInfo> all;
        std::vector<std::string> files;
        FileUtils::getInstance()->listFilesRecursively(_fixtures, &files);
        for (const std::string& f : files) {
            const size_t slash = f.find_last_of("/\\");
            const std::string name = f.substr(slash + 1);
            if (name.compare(0, 5, "list_") != 0) continue;
            std::vector<OnlineLevelInfo> part;
            if (parseLevelList(FileUtils::getInstance()->getStringFromFile(f), part, nullptr, nullptr, nullptr))
                for (auto& l : part)
                    if (std::none_of(all.begin(), all.end(), [&](const OnlineLevelInfo& o) { return o.id == l.id; }))
                        all.push_back(l);
        }
        std::vector<OnlineLevelInfo> hits;
        for (auto& l : all)
            if (matches(l, query)) hits.push_back(l);
        sortLevels(hits, query.sortBy);
        Director::getInstance()->getScheduler()->performFunctionInCocosThread(
            [deliver, hits, query]() { deliver(true, std::string(), hits, query.page, 500); });
        return id;
    }
    std::string body;
    const std::string page = std::to_string(std::max(1, query.page));
    if (query.searchBy == SearchBy::None || query.term.empty()) {
        body = form({{"action", "get_all"}, {"page", page}, {"sortby", sortName(query.sortBy)},
                     {"uploaded", uploadedName(query.uploaded)}});
    } else {
        body = form({{"action", query.searchBy == SearchBy::Name ? "search_by_name" : "search_by_user"},
                     {"sterm", query.term}, {"page", page}, {"sortby", sortName(query.sortBy)},
                     {"uploaded", uploadedName(query.uploaded)}});
    }
    post("get_level.hw", body, [deliver](bool ok, const std::string& err, const std::string& data) {
        std::vector<OnlineLevelInfo> levels;
        int pg = 1, pp = 0;
        std::string perr = err;
        if (ok) ok = parseLevelList(data, levels, &pg, &pp, &perr);
        deliver(ok, perr, levels, pg, pp);
    });
    return id;
}

RequestId HWApi::listFeatured(ListCallback callback) {
    const RequestId id = nextId();
    auto deliver = [this, id, callback](bool ok, const std::string& err, const std::vector<OnlineLevelInfo>& v,
                                        int pg, int pp) {
        if (std::find(_cancelled.begin(), _cancelled.end(), id) != _cancelled.end()) return;
        callback(ok, err, v, pg, pp);
    };
    if (!_fixtures.empty()) {
        LevelQuery q;
        return listLevels(q, callback);
    }
    post("get_level.hw", form({{"action", "get_featured"}}),
         [deliver](bool ok, const std::string& err, const std::string& data) {
             std::vector<OnlineLevelInfo> levels;
             int pg = 1, pp = 0;
             std::string perr = err;
             if (ok) ok = parseLevelList(data, levels, &pg, &pp, &perr);
             deliver(ok, perr, levels, pg, pp);
         });
    return id;
}

RequestId HWApi::getLevelInfo(int levelId, InfoCallback callback) {
    const RequestId id = nextId();
    auto deliver = [this, id, callback](bool ok, const std::string& err, const OnlineLevelInfo& l) {
        if (std::find(_cancelled.begin(), _cancelled.end(), id) != _cancelled.end()) return;
        callback(ok, err, l);
    };
    auto handle = [deliver, levelId](bool ok, const std::string& err, const std::string& data) {
        std::vector<OnlineLevelInfo> levels;
        std::string perr = err;
        if (ok) ok = parseLevelList(data, levels, nullptr, nullptr, &perr);
        if (ok && levels.empty()) {
            ok = false;
            perr = "level " + std::to_string(levelId) + " not found";
        }
        deliver(ok, perr, ok ? levels.front() : OnlineLevelInfo());
    };
    if (!_fixtures.empty()) {
        const std::string path = _fixtures + std::to_string(levelId) + ".meta.xml";
        const std::string data = FileUtils::getInstance()->getStringFromFile(path);
        Director::getInstance()->getScheduler()->performFunctionInCocosThread(
            [handle, data]() { handle(!data.empty(), "not in fixtures", data); });
        return id;
    }
    post("get_level.hw", form({{"level_id", std::to_string(levelId)}, {"action", "get_level"}}), handle);
    return id;
}

bool HWApi::isCached(int levelId) const {
    return FileUtils::getInstance()->isFileExist(cacheDir() + std::to_string(levelId) + ".xml");
}

RequestId HWApi::downloadLevel(const OnlineLevelInfo& level, bool countPlay, LevelCallback callback) {
    const RequestId id = nextId();
    auto deliver = [this, id, callback](bool ok, const std::string& err, const std::string& xml) {
        if (std::find(_cancelled.begin(), _cancelled.end(), id) != _cancelled.end()) return;
        callback(ok, err, xml);
    };
    const std::string cached = cacheDir() + std::to_string(level.id) + ".xml";
    if (FileUtils::getInstance()->isFileExist(cached)) {
        const std::string xml = FileUtils::getInstance()->getStringFromFile(cached);
        Director::getInstance()->getScheduler()->performFunctionInCocosThread(
            [deliver, xml]() { deliver(!xml.empty(), "cache unreadable", xml); });
        return id;
    }
    const int authorId = level.authorId;
    auto handle = [deliver, authorId, cached](bool ok, const std::string& err, const std::string& data) {
        std::string xml, derr = err;
        if (ok) ok = decryptRecord(data, authorId, xml, &derr);
        if (ok) {
            FileUtils::getInstance()->createDirectory(cacheDir());
            FileUtils::getInstance()->writeStringToFile(xml, cached);
        }
        deliver(ok, derr, xml);
    };
    if (!_fixtures.empty()) {
        Data d = FileUtils::getInstance()->getDataFromFile(_fixtures + std::to_string(level.id) + ".record.bin");
        std::string data;
        if (!d.isNull()) data.assign(reinterpret_cast<const char*>(d.getBytes()), (size_t)d.getSize());
        Director::getInstance()->getScheduler()->performFunctionInCocosThread(
            [handle, data]() { handle(!data.empty(), "not in fixtures", data); });
        return id;
    }
    post("get_level.hw",
         form({{"level_id", std::to_string(level.id)}, {"action", "get_record"}, {"ip_tracking", ipTracking(countPlay)}}),
         handle);
    return id;
}

}  // namespace online
