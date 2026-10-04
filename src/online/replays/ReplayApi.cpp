// ONLINE (PC addition): see ReplayApi.h.
#include "online/replays/ReplayApi.h"

#include <cstring>

#include <openssl/aes.h>
#include <openssl/rand.h>

#include "cocos2d.h"
#include "online/account/TjfAccount.h"

USING_NS_CC;

namespace online {
namespace replays {

namespace {

// SaveReplayMenu.APPLESAUCE (hex AES-128 key of PostEncryption).
const char* const kPostKeyHex = "7ab7657e5595b5c3486988c90728c6ae";

std::string cacheDir() { return account::storageDir() + "replays/"; }

std::string hexOf(const unsigned char* p, size_t n) {
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < n; ++i) {
        out += hex[p[i] >> 4];
        out += hex[p[i] & 15];
    }
    return out;
}

std::string readFile(const std::string& path) {
    Data d = FileUtils::getInstance()->getDataFromFile(path);
    if (d.isNull()) return std::string();
    return std::string(reinterpret_cast<const char*>(d.getBytes()), (size_t)d.getSize());
}

}  // namespace

const char* sortName(ReplaySort sort) {
    switch (sort) {
        case ReplaySort::Fastest: return "completion_time";
        case ReplaySort::Rating: return "rating";
        case ReplaySort::Oldest: return "oldest";
        default: return "newest";
    }
}

ReplayApi* ReplayApi::get() {
    static ReplayApi* instance = new ReplayApi();
    account::TjfAccount::get();  // cookie hooks
    return instance;
}

RequestId ReplayApi::listByLevel(int levelId, ReplaySort sort, int page, ListCallback done) {
    using account::TjfServices;
    return HWApi::getInstance()->request(
        "replay.hw",
        TjfServices::form({{"action", "get_all_by_level"}, {"page", std::to_string(page)},
                           {"level_id", std::to_string(levelId)}, {"sortby", sortName(sort)}}),
        false, [done](const HWApi::RawResponse& r) {
            std::vector<ReplayInfo> list;
            int pg = 1, pp = 0;
            std::string error = r.error;
            bool ok = r.ok;
            // A level without replays: an empty <rps/> (or nothing at all).
            if (ok && r.body.find_first_not_of(" \r\n\t") == std::string::npos) {
                done(true, std::string(), list, 1, 0);
                return;
            }
            if (ok) ok = parseReplayList(r.body, list, &pg, &pp, &error);
            done(ok, error, list, pg, pp);
        });
}

RequestId ReplayApi::getCombined(int replayId, CombinedCallback done) {
    using account::TjfServices;
    return HWApi::getInstance()->request(
        "replay.hw", TjfServices::form({{"replay_id", std::to_string(replayId)}, {"action", "get_combined"}}), false,
        [done](const HWApi::RawResponse& r) {
            ReplayInfo replay;
            OnlineLevelInfo level;
            std::string error = r.error;
            const bool ok = r.ok && parseCombined(r.body, replay, level, &error);
            done(ok, error, replay, level);
        });
}

bool ReplayApi::isCached(int replayId) const {
    return FileUtils::getInstance()->isFileExist(cacheDir() + std::to_string(replayId) + ".cmb");
}

RequestId ReplayApi::download(const ReplayInfo& replay, const OnlineLevelInfo& level, DownloadCallback done) {
    using account::TjfServices;
    const int authorId = level.authorId;
    const std::string path = cacheDir() + std::to_string(replay.id) + ".cmb";
    auto handle = [done, authorId, path](bool fresh, const std::string& body, const std::string& transportError) {
        ReplayInput input;
        std::string replayBytes, record, xml, error = transportError;
        bool ok = error.empty() && splitCombinedRecord(body, replayBytes, record, &error) &&
                  HWApi::decryptRecord(record, authorId, xml, &error);
        if (ok) {
            input = ReplayInput::fromBytes(replayBytes);
            if (fresh) {
                FileUtils::getInstance()->createDirectory(cacheDir());
                Data d;
                d.copy(reinterpret_cast<const unsigned char*>(body.data()), (ssize_t)body.size());
                FileUtils::getInstance()->writeDataToFile(d, path);
            }
        }
        done(ok, error, input, xml);
    };
    if (FileUtils::getInstance()->isFileExist(path)) {
        const std::string body = readFile(path);
        Director::getInstance()->getScheduler()->performFunctionInCocosThread(
            [handle, body]() { handle(false, body, body.empty() ? "cache unreadable" : std::string()); });
        return 0;
    }
    return HWApi::getInstance()->request(
        "replay.hw",
        TjfServices::form({{"replay_id", std::to_string(replay.id)}, {"level_id", std::to_string(level.id)},
                           {"action", "get_cmb_records"}}),
        false, [handle](const HWApi::RawResponse& r) { handle(true, r.body, r.ok ? std::string() : r.error); });
}

bool ReplayApi::postEncrypt(const std::string& plain, std::string* cipherB64, std::string* ivHex,
                            const unsigned char* fixedIv) {
    unsigned char key[16];
    for (int i = 0; i < 16; ++i) {
        unsigned v = 0;
        std::sscanf(kPostKeyHex + i * 2, "%2x", &v);
        key[i] = (unsigned char)v;
    }
    unsigned char iv[16];
    if (fixedIv) {
        std::memcpy(iv, fixedIv, 16);
    } else if (RAND_bytes(iv, 16) != 1) {
        return false;
    }
    if (ivHex) *ivHex = hexOf(iv, 16);
    // Hex.toArray(Hex.fromString(s)): the UTF-8 bytes; PKCS#5 to the 16-byte AES block.
    std::string data = plain;
    const size_t pad = 16 - data.size() % 16;
    data.append(pad, (char)pad);
    AES_KEY aes;
    if (AES_set_encrypt_key(key, 128, &aes) != 0) return false;
    std::string out(data.size(), '\0');
    unsigned char ivWork[16];
    std::memcpy(ivWork, iv, 16);
    AES_cbc_encrypt(reinterpret_cast<const unsigned char*>(data.data()), reinterpret_cast<unsigned char*>(&out[0]),
                    data.size(), &aes, ivWork, AES_ENCRYPT);
    if (cipherB64) *cipherB64 = account::TjfServices::base64(out);
    return true;
}

std::string ReplayApi::replayQuery(int levelId, int character, int frames, const std::string& comment, int userId) {
    return "id=" + std::to_string(levelId) + "&pc=" + std::to_string(character) + "&ar=" + kArchitecture +
           "&ct=" + std::to_string(frames) + "&vr=" + kFlashVersion + "&uc=" + as3Escape(comment) +
           "&ui=" + std::to_string(userId);
}

RequestId ReplayApi::upload(const SavedRun& run, const std::string& comment, account::ReplyCallback done) {
    using account::TjfServices;
    // SaveReplayMenu.save: ct = length when completed, else maxReplayFrames.
    const int frames = run.completed ? (int)run.input.keys.size() : kMaxReplayFrames;
    const std::string query =
        replayQuery(run.levelId, run.character, frames, comment, account::TjfAccount::get()->userId());
    std::string em, ei;
    if (!postEncrypt(query, &em, &ei)) {
        account::Reply reply;
        reply.reason = "crypto";
        reply.message = "Couldn't prepare the upload.";
        Director::getInstance()->getScheduler()->performFunctionInCocosThread([done, reply]() { done(reply); });
        return 0;
    }
    return HWApi::getInstance()->request(
        "replay.hw",
        TjfServices::form({{"action", "create"}, {"rr", TjfServices::base64(run.input.toBytes())}, {"em", em}, {"ei", ei}}),
        false, [done](const HWApi::RawResponse& r) { done(TjfServices::parseReply(r, "replay")); });
}

RequestId ReplayApi::rate(int replayId, int rating, account::ReplyCallback done) {
    using account::TjfServices;
    return HWApi::getInstance()->request(
        "replay.hw",
        TjfServices::form({{"replay_id", std::to_string(replayId)}, {"rating", std::to_string(rating)},
                           {"action", "rate_replay"}}),
        false, [done](const HWApi::RawResponse& r) { done(TjfServices::parseReply(r, "replay vote")); });
}

}  // namespace replays
}  // namespace online
