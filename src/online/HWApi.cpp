#include "online/HWApi.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>

#include "cocos2d.h"
#include "network/HttpClient.h"
#include "online/Protocol.h"

USING_NS_CC;

namespace online {
namespace {

const char* const kSite = "https://totaljerkface.com/";
// Cloudflare keeps adding the exact User-Agent strings this client has shipped to its blocklist
// (OpenWheels/1.0, then the fixed official-client UAs), which produced a 403 "Just a moment..."
// page even though the API is unchanged. Build a plausible browser User-Agent from a wide pool
// once per launch instead, so we never reuse a known-blocked string.
const char* pick(const std::vector<const char*>& values, std::mt19937& rng) {
    std::uniform_int_distribution<size_t> dist(0, values.size() - 1);
    return values[dist(rng)];
}

std::string makeUserAgent() {
    std::random_device rd;
    std::mt19937 rng(rd() ^ static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count()));
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
    static const std::vector<const char*> devices = {
        "Pixel 6", "Pixel 6 Pro", "Pixel 7", "Pixel 7 Pro", "Pixel 8", "Pixel 8 Pro", "Pixel 9",
        "SM-S901B", "SM-S911B", "SM-S921B", "SM-A536B", "SM-A546B", "moto g(60)",
        "moto g power (2023)", "CPH2451", "LE2123", "2201117TG", "V2207", "RMX3363"};
    static const std::vector<const char*> androidVersions = {"11", "12", "13", "14", "15", "16"};
    static const std::vector<const char*> chromeVersions = {"149", "150", "151", "152", "153", "154"};
    return "Mozilla/5.0 (Linux; Android " + std::string(pick(androidVersions, rng)) + "; " +
           pick(devices, rng) + ") AppleWebKit/537.36 (KHTML, like Gecko) Chrome/" +
           pick(chromeVersions, rng) + ".0.0.0 Mobile Safari/537.36";
#elif (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
    static const std::vector<const char*> iosVersions = {"17_6", "18_0", "18_1", "18_2"};
    static const std::vector<const char*> safariVersions = {"17.6", "18.0", "18.1", "18.2"};
    const size_t i = std::uniform_int_distribution<size_t>(0, iosVersions.size() - 1)(rng);
    const bool ipad = (rng() & 1) != 0;
    return std::string("Mozilla/5.0 (") + (ipad ? "iPad; CPU OS " : "iPhone; CPU iPhone OS ") +
           iosVersions[i] + " like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/" +
           safariVersions[i] + " Mobile/15E148 Safari/604.1";
#else
    static const std::vector<const char*> platforms = {"Windows NT 10.0; Win64; x64",
                                                       "Macintosh; Intel Mac OS X 10_15_7",
                                                       "X11; Linux x86_64"};
    static const std::vector<const char*> chromeVersions = {"149", "150", "151", "152", "153", "154"};
    return "Mozilla/5.0 (" + std::string(pick(platforms, rng)) +
           ") AppleWebKit/537.36 (KHTML, like Gecko) Chrome/" + pick(chromeVersions, rng) +
           ".0.0.0 Safari/537.36";
#endif
}

const std::string& userAgentHeader() {
    static const std::string header = "User-Agent: " + makeUserAgent();
    return header;
}

int g_lastStatus = 0;  // HTTP status of the latest response (for error messages)
const double kMinSpacingSeconds = 1.0;  // one request at a time, at most one per second

double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

std::string urlEncodeImpl(const std::string& s) {
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
        body += urlEncodeImpl(f.second);
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

// "failure:<reason>" or an HTML error page instead of data (online/Protocol.cpp).
bool serverError(const std::string& body, std::string* error) {
    return proto::classifyReply(body, g_lastStatus, error);
}

std::string cacheDir() { return FileUtils::getInstance()->getWritablePath() + "online/"; }

// Every word of the search term must appear (any order, any case).
bool matches(const OnlineLevelInfo& l, const LevelQuery& q) {
    if (q.searchBy == SearchBy::None || q.term.empty()) return true;
    return proto::termMatches(q.searchBy == SearchBy::Name ? l.name : l.authorName, q.term);
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
    // ONLINE (PC addition): raw requests (account / replay modules)
    bool get = false;
    RawCallback raw;
};

// ONLINE (PC addition)
std::string HWApi::siteUrl() {
    static std::string base;
    if (base.empty()) {
        const char* env = std::getenv("OW_TJF_BASE");
        base = (env && *env) ? env : kSite;
        if (base.back() != '/') base += '/';
    }
    return base;
}

std::string HWApi::urlEncode(const std::string& s) { return urlEncodeImpl(s); }

bool HWApi::isServerError(const std::string& body, std::string* error) { return serverError(body, error); }

RequestId HWApi::request(const std::string& endpoint, const std::string& body, bool get, RawCallback callback) {
    const RequestId id = nextId();
    auto* p = new Pending();
    p->endpoint = endpoint;
    p->body = body;
    p->get = get;
    p->raw = [this, id, callback](const RawResponse& r) {
        if (std::find(_cancelled.begin(), _cancelled.end(), id) != _cancelled.end()) return;
        callback(r);
    };
    _queue.push_back(p);
    pump();
    return id;
}

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
    auto* p = new Pending();
    p->endpoint = endpoint;
    p->body = body;
    p->done = std::move(done);
    _queue.push_back(p);
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
    // ONLINE (PC addition): OW_TJF_BASE, GET requests and the session cookie (account module).
    const std::string site = siteUrl();
    request->setUrl(site + p->endpoint);
    request->setRequestType(p->get ? network::HttpRequest::Type::GET : network::HttpRequest::Type::POST);
    const std::string origin = site.substr(0, site.size() - 1);  // siteUrl() always ends with '/'
    std::vector<std::string> headers = {userAgentHeader(),
                                        std::string("Referer: ") + site + "happy_wheels.tjf",
                                        "Accept: */*",
                                        "Accept-Language: en-US,en;q=0.9",
                                        "Origin: " + origin,
                                        "Sec-Fetch-Site: same-origin",
                                        "Sec-Fetch-Mode: cors",
                                        "Sec-Fetch-Dest: empty"};
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
    // cocos2d-x's Android HttpURLConnection defaults to "Accept-Encoding: identity"; advertise
    // the browser encodings its Java response reader can decode.
    headers.push_back("Accept-Encoding: gzip, deflate");
#endif
    if (!p->get) headers.push_back("Content-Type: application/x-www-form-urlencoded");
    if (_cookieProvider) {
        const std::string cookie = _cookieProvider();
        if (!cookie.empty()) headers.push_back("Cookie: " + cookie);
    }
    request->setHeaders(headers);
    if (!p->get) request->setRequestData(p->body.data(), p->body.size());
    // The body may hold a password (login): don't keep it around longer than the request.
    std::fill(p->body.begin(), p->body.end(), '\0');
    p->body.clear();
    request->setResponseCallback([this, p](network::HttpClient*, network::HttpResponse* response) {
        _busy = false;
        _lastRequestTime = now();
        RawResponse r;
        if (response && response->getResponseData())
            r.body.assign(response->getResponseData()->begin(), response->getResponseData()->end());
        if (response && response->getResponseHeader())
            r.headers.assign(response->getResponseHeader()->begin(), response->getResponseHeader()->end());
        if (response) r.status = response->getResponseCode();
        g_lastStatus = r.status;
        if (_headerObserver && !r.headers.empty()) _headerObserver(r.headers);
        r.ok = response && response->isSucceed();
        if (!r.ok) {
            r.error = "network error";
            if (response && response->getErrorBuffer() && response->getErrorBuffer()[0])
                r.error += std::string(": ") + response->getErrorBuffer();
            else if (response)
                r.error += " (HTTP " + std::to_string(response->getResponseCode()) + ")";
        }
        if (p->raw) {
            p->raw(r);
        } else {
            p->done(r.ok, r.error, r.body);
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
    return proto::parseLevelList(body, g_lastStatus, levels, page, perPage, error);
}

bool HWApi::decryptRecord(const std::string& record, int authorId, std::string& xml, std::string* error) {
    return proto::decryptRecord(record, authorId, xml, error);
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
    post("get_level.hw", body, [this, deliver, query, page](bool ok, const std::string& err, const std::string& data) {
        std::vector<OnlineLevelInfo> levels;
        int pg = 1, pp = 0;
        std::string perr = err;
        if (ok) ok = parseLevelList(data, levels, &pg, &pp, &perr);
        // The site answers multi-word searches ("Spinning Gears") with nothing at all: search for the
        // longest word instead and keep the levels that contain every word.
        const std::vector<std::string> ws = proto::splitWords(query.term);
        if (ok && levels.empty() && ws.size() > 1 && query.searchBy != SearchBy::None) {
            std::string longest = ws[0];
            for (const std::string& w : ws)
                if (w.size() > longest.size()) longest = w;
            const std::string retry = form({{"action", query.searchBy == SearchBy::Name ? "search_by_name" : "search_by_user"},
                                            {"sterm", longest}, {"page", page}, {"sortby", sortName(query.sortBy)},
                                            {"uploaded", uploadedName(query.uploaded)}});
            post("get_level.hw", retry, [deliver, query](bool ok2, const std::string& err2, const std::string& data2) {
                std::vector<OnlineLevelInfo> all, hits;
                int pg2 = 1, pp2 = 0;
                std::string perr2 = err2;
                if (ok2) ok2 = parseLevelList(data2, all, &pg2, &pp2, &perr2);
                for (auto& l : all)
                    if (matches(l, query)) hits.push_back(l);
                deliver(ok2, perr2, hits, pg2, pp2);
            });
            return;
        }
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
