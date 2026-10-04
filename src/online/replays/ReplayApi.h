#pragma once
// ONLINE (PC addition): replay.hw requests (decompiled Flash v1.87 menus/ReplayBrowser,
// ReplayLoader, RecordLoader, menus/SaveReplayMenu, menus/SessionReplayMenu):
//
//   get_all_by_level  level_id, page, sortby newest|oldest|rating|completion_time
//                     -> <rps pg pp><rp id li ui un rg vs vw dc pc ct ar vr><uc/></rp>...</rps>
//   get_combined      replay_id -> <combined_data><rp/><lv/></combined_data>
//   get_cmb_records   replay_id, level_id -> int32 BE n, n replay bytes, the level record
//                     (counts a view, as watching in the browser does)
//   create            rr = base64(replay bytes), em = base64(AES-128-CBC(PKCS#5) of
//                     "id=<level>&pc=<char>&ar=<arch>&ct=<frames|6000>&vr=<version>&uc=<escape(comment)>&ui=<user>"
//                     with the client's key 7ab7657e..., random IV), ei = hex(IV)   [login]
//                     -> success:<new replay id> | failure:time_lockout|hi_comp_time|not_logged_in...
//   rate_replay       replay_id, rating                                          [login]
//
// There is no "replays by user" action (probed: get_all_by_user returns nothing); "My replays"
// are the runs saved / uploaded from this PC.
//
// Downloads are cached under <writable>/online/replays/<replay id>.cmb (never in the repo).

#include <functional>
#include <string>
#include <vector>

#include "online/HWApi.h"
#include "online/account/TjfServices.h"
#include "online/replays/FlashReplay.h"

namespace online {
namespace replays {

enum class ReplaySort { Fastest, Rating, Newest, Oldest };
const char* sortName(ReplaySort sort);

class ReplayApi {
public:
    static ReplayApi* get();

    using ListCallback = std::function<void(bool ok, const std::string& error, const std::vector<ReplayInfo>& replays,
                                            int page, int perPage)>;
    RequestId listByLevel(int levelId, ReplaySort sort, int page, ListCallback done);

    using CombinedCallback = std::function<void(bool ok, const std::string& error, const ReplayInfo& replay,
                                                const OnlineLevelInfo& level)>;
    RequestId getCombined(int replayId, CombinedCallback done);

    // Replay input + decrypted browser-format level XML (cached after the first download).
    using DownloadCallback = std::function<void(bool ok, const std::string& error, const ReplayInput& input,
                                                const std::string& levelXml)>;
    RequestId download(const ReplayInfo& replay, const OnlineLevelInfo& level, DownloadCallback done);
    bool isCached(int replayId) const;

    // Upload one of the player's runs (needs login). reply.value = new replay id.
    RequestId upload(const SavedRun& run, const std::string& comment, account::ReplyCallback done);
    RequestId rate(int replayId, int rating, account::ReplyCallback done);

    // PostEncryption (AES-128-CBC, PKCS#5, random IV): base64 ciphertext + hex IV. Exposed for tests.
    static bool postEncrypt(const std::string& plain, std::string* cipherB64, std::string* ivHex,
                            const unsigned char* fixedIv = nullptr);
    // The query string SaveReplayMenu.createReplayQueryString builds.
    static std::string replayQuery(int levelId, int character, int frames, const std::string& comment, int userId);

private:
    ReplayApi() = default;
};

}  // namespace replays
}  // namespace online
