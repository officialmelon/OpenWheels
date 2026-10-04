#pragma once
// Client for the browser game's public level endpoints (totaljerkface.com get_level.hw), the
// same form-encoded POSTs the HTML5 client sends. Requests run one at a time with a minimum
// spacing; callbacks arrive on the cocos thread. Records are decrypted (Blowfish-CBC keyed by the
// author id, then zlib) and cached under <writable>/online/<id>.xml, so a level downloads once.
//
// Offline testing: with the environment variable OW_ONLINE_FIXTURES=<dir> (e.g.
// binary/flash/samples) no network is used: lists come from <dir>/list_*.xml (all of them,
// filtered locally by the query), level metadata from <dir>/<id>.meta.xml and records from
// <dir>/<id>.record.bin.

#include <functional>
#include <string>
#include <vector>

#include "online/OnlineLevel.h"

namespace online {

enum class SortBy { Newest, Oldest, Plays, Rating };
enum class Uploaded { Today, Week, Month, Anytime };
enum class SearchBy { None, Name, Author };

struct LevelQuery {
    SearchBy searchBy = SearchBy::None;
    std::string term;                 // search term for Name/Author
    SortBy sortBy = SortBy::Rating;
    Uploaded uploaded = Uploaded::Anytime;
    int page = 1;                     // 1-based; the server returns up to perPage (500) per page
};

using RequestId = unsigned int;

class HWApi {
public:
    static HWApi* getInstance();

    using ListCallback = std::function<void(bool ok, const std::string& error,
                                            const std::vector<OnlineLevelInfo>& levels, int page,
                                            int perPage)>;
    using InfoCallback = std::function<void(bool ok, const std::string& error, const OnlineLevelInfo& level)>;
    // xml: the level's browser-format XML (<levelXML>...), see FlashLevelConverter for mobile.
    using LevelCallback = std::function<void(bool ok, const std::string& error, const std::string& xml)>;

    RequestId listLevels(const LevelQuery& query, ListCallback callback);
    RequestId listFeatured(ListCallback callback);
    RequestId getLevelInfo(int levelId, InfoCallback callback);
    // Downloads (or reads from the cache) and decrypts the level. countPlay: the server counts
    // the download as a play (what the browser client does when a player starts a level); false
    // for previews.
    RequestId downloadLevel(const OnlineLevelInfo& level, bool countPlay, LevelCallback callback);
    bool isCached(int levelId) const;

    // Drops the callback of a pending request (call from a scene's destructor/onExit).
    void cancel(RequestId id);
    void cancelAll();

    // ONLINE (PC addition): hooks for the account and replay modules (src/online/account,
    // src/online/replays). Every request, these included, goes through the same serial queue.
    //
    // Site root, "https://totaljerkface.com/" unless the environment variable OW_TJF_BASE points
    // somewhere else (e.g. http://127.0.0.1:8765/ for tools/online/mock_tjf.py).
    static std::string siteUrl();
    struct RawResponse {
        bool ok = false;           // transport succeeded (HTTP 2xx)
        std::string error;         // transport error
        std::string body;
        std::string headers;       // raw response headers (Set-Cookie...)
        long status = 0;
    };
    using RawCallback = std::function<void(const RawResponse& response)>;
    // POST (form body) or GET `endpoint` (relative to siteUrl()). The callback is dropped after
    // cancel(id).
    RequestId request(const std::string& endpoint, const std::string& body, bool get, RawCallback callback);
    // The Cookie header sent with every request ("" = none) and an observer that sees every
    // response's headers (the account module keeps the site's session cookie that way).
    void setCookieProvider(std::function<std::string()> provider) { _cookieProvider = std::move(provider); }
    void setHeaderObserver(std::function<void(const std::string& headers)> observer) {
        _headerObserver = std::move(observer);
    }
    // Form helpers shared with the account/replay modules.
    static std::string urlEncode(const std::string& s);
    // "failure:<reason>" or an HTML page instead of data.
    static bool isServerError(const std::string& body, std::string* error);

    // Blowfish-CBC (key "eatshit"+authorId, IV "abcd1234", PKCS#5) + zlib -> UTF-8 XML.
    static bool decryptRecord(const std::string& record, int authorId, std::string& xml, std::string* error);
    // Parses a <lvs> response.
    static bool parseLevelList(const std::string& body, std::vector<OnlineLevelInfo>& levels,
                               int* page, int* perPage, std::string* error);

private:
    HWApi();
    struct Pending;
    void post(const std::string& endpoint, const std::string& body,
              std::function<void(bool ok, const std::string& error, const std::string& response)> done);
    void pump();
    RequestId nextId();

    std::vector<Pending*> _queue;
    bool _busy = false;
    double _lastRequestTime = -1e9;
    RequestId _nextId = 1;
    std::vector<RequestId> _cancelled;
    std::string _fixtures;  // OW_ONLINE_FIXTURES
    std::function<std::string()> _cookieProvider;                 // ONLINE (PC addition)
    std::function<void(const std::string&)> _headerObserver;     // ONLINE (PC addition)
};

}  // namespace online
