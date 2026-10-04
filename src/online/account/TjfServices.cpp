// ONLINE (PC addition): see TjfServices.h.
#include "online/account/TjfServices.h"

#include <cstring>

#include <openssl/blowfish.h>
#include <zlib.h>

#include "base/base64.h"
#include "cocos2d.h"
#include "online/account/TjfAccount.h"
#include "tinyxml2/tinyxml2.h"

USING_NS_CC;

namespace online {
namespace account {

namespace {

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<OnlineLevelInfo> levelsUnder(tinyxml2::XMLElement* parent) {
    std::vector<OnlineLevelInfo> out;
    if (!parent) return out;
    tinyxml2::XMLElement* lvs = parent->FirstChildElement("lvs");
    if (!lvs) return out;
    tinyxml2::XMLPrinter printer;
    lvs->Accept(&printer);
    HWApi::parseLevelList(printer.CStr(), out, nullptr, nullptr, nullptr);
    return out;
}

}  // namespace

TjfServices* TjfServices::get() {
    static TjfServices* instance = new TjfServices();
    TjfAccount::get();  // installs the cookie hooks
    return instance;
}

std::string TjfServices::form(std::initializer_list<std::pair<const char*, std::string>> fields) {
    std::string body;
    for (const auto& f : fields) {
        if (!body.empty()) body += '&';
        body += f.first;
        body += '=';
        body += HWApi::urlEncode(f.second);
    }
    return body;
}

std::string TjfServices::base64(const std::string& bytes) {
    char* out = nullptr;
    const int n = cocos2d::base64Encode(reinterpret_cast<const unsigned char*>(bytes.data()), (unsigned)bytes.size(), &out);
    std::string s = out ? std::string(out, n) : std::string();
    free(out);
    return s;
}

Reply TjfServices::parseReply(const HWApi::RawResponse& r, const std::string& what) {
    Reply reply;
    if (!r.ok) {
        reply.reason = "network";
        reply.message = "Couldn't reach totaljerkface.com. Check your internet connection and try again.";
        return reply;
    }
    const std::string body = trim(r.body);
    const std::string head8 = body.substr(0, 8);
    if (head8.find("<html") != std::string::npos || head8.find("<!DOC") != std::string::npos) {
        reply.reason = "system_error";
        reply.message = "There was an unexpected system error.";
        return reply;
    }
    const size_t colon = body.find(':');
    const std::string head = body.substr(0, colon);
    const std::string arg = colon == std::string::npos ? std::string() : trim(body.substr(colon + 1));
    if (head == "success") {
        reply.ok = true;
        reply.value = arg;
        return reply;
    }
    if (head != "failure") {
        reply.reason = "unknown";
        reply.message = "Error: something dreadful has happened.";
        return reply;
    }
    reply.reason = arg;
    if (arg == "not_logged_in") {
        TjfAccount::get()->sessionExpired();
        reply.message = "You are not logged in any more. Please log in again.";
    } else if (arg == "duplicate") {
        reply.message = "This level is already one of your favorite levels.";
    } else if (arg == "duplicate_rating") {
        reply.message = what == "replay vote" ? "You've already voted on this replay." : "You've already voted on this level.";
    } else if (arg == "illegal_argument") {
        reply.message = "Rating must be between 0 and 5.";
    } else if (arg == "time_lockout") {
        if (what == "publish") reply.message = "You may only publish one level per day.";
        else if (what == "replay") reply.message = "You saved a replay too recently. Please wait a moment and try again.";
        else reply.message = "You saved a level too recently. Please wait a moment and try again.";
    } else if (arg == "hi_comp_time") {
        reply.message = "Your replay is too long.";
    } else if (arg == "app_error") {
        reply.message = "Sorry, there was an application error. Please try again in a moment.";
    } else if (arg == "bad_param" || arg == "invalid_action") {
        reply.message = "The server rejected the request (" + arg + ").";
    } else {
        reply.message = "An unknown error has occurred (" + arg + ").";
    }
    return reply;
}

RequestId TjfServices::listFavorites(ListCallback done) {
    // LevelBrowser.loadData with _favorites: a fresh URLVariables with action only.
    return HWApi::getInstance()->request(
        "user.hw", form({{"action", "get_favorites"}}), false, [this, done](const HWApi::RawResponse& r) {
            std::vector<OnlineLevelInfo> levels;
            std::string error = r.error;
            bool ok = r.ok;
            if (ok && trim(r.body).compare(0, 8, "failure:") == 0) {
                const Reply reply = parseReply(r, "favorites");
                ok = false;
                error = reply.message;
            } else if (ok) {
                ok = HWApi::parseLevelList(r.body, levels, nullptr, nullptr, &error);
            }
            if (ok) {
                _favorites.clear();
                for (const auto& l : levels) _favorites.insert(l.id);
                _favoritesKnown = true;
            }
            if (done) done(ok, error, levels);
        });
}

RequestId TjfServices::setFavorite(int levelId, bool favorite, ReplyCallback done) {
    return HWApi::getInstance()->request(
        "user.hw", form({{"level_id", std::to_string(levelId)}, {"action", favorite ? "set_favorite" : "delete_favorite"}}),
        false, [this, levelId, favorite, done](const HWApi::RawResponse& r) {
            Reply reply = parseReply(r, "favorites");
            if (reply.ok || reply.reason == "duplicate") {
                if (favorite) _favorites.insert(levelId);
                else _favorites.erase(levelId);
            }
            if (done) done(reply);
        });
}

RequestId TjfServices::rateLevel(int levelId, int rating, ReplyCallback done) {
    return HWApi::getInstance()->request(
        "set_level.hw",
        form({{"level_id", std::to_string(levelId)}, {"rating", std::to_string(rating)}, {"action", "rate_level"}}),
        false, [done](const HWApi::RawResponse& r) {
            if (done) done(parseReply(r, "level vote"));
        });
}

RequestId TjfServices::listMyLevels(MyLevelsCallback done) {
    return HWApi::getInstance()->request(
        "get_level.hw", form({{"action", "get_cmb_by_user"}}), false, [done](const HWApi::RawResponse& r) {
            std::vector<OnlineLevelInfo> priv, pub;
            if (!r.ok) {
                if (done) done(false, r.error, priv, pub);
                return;
            }
            if (trim(r.body).compare(0, 8, "failure:") == 0 || HWApi::isServerError(r.body, nullptr)) {
                const Reply reply = parseReply(r, "my levels");
                if (done) done(false, reply.message, priv, pub);
                return;
            }
            tinyxml2::XMLDocument doc;
            if (doc.Parse(r.body.c_str()) != tinyxml2::XML_SUCCESS || !doc.RootElement()) {
                if (done) done(false, "unreadable level list", priv, pub);
                return;
            }
            priv = levelsUnder(doc.RootElement()->FirstChildElement("private"));
            pub = levelsUnder(doc.RootElement()->FirstChildElement("published"));
            if (done) done(true, std::string(), priv, pub);
        });
}

RequestId TjfServices::listPublishedBy(int userId, int page, ListCallback done) {
    return HWApi::getInstance()->request(
        "get_level.hw",
        form({{"action", "get_pub_by_user"}, {"user_id", std::to_string(userId)}, {"page", std::to_string(page)},
              {"sortby", "newest"}, {"uploaded", "anytime"}}),
        false, [done](const HWApi::RawResponse& r) {
            std::vector<OnlineLevelInfo> levels;
            std::string error = r.error;
            bool ok = r.ok && HWApi::parseLevelList(r.body, levels, nullptr, nullptr, &error);
            if (done) done(ok, error, levels);
        });
}

std::string TjfServices::encryptLevelRecord(const std::string& xml, int authorId) {
    // ByteArray.compress(): zlib stream, default level.
    uLongf zlen = compressBound((uLong)xml.size());
    std::string z(zlen, '\0');
    if (compress2(reinterpret_cast<Bytef*>(&z[0]), &zlen, reinterpret_cast<const Bytef*>(xml.data()), (uLong)xml.size(),
                  Z_DEFAULT_COMPRESSION) != Z_OK)
        return std::string();
    z.resize(zlen);
    // PKCS#5 to the 8-byte Blowfish block.
    const size_t pad = 8 - z.size() % 8;
    z.append(pad, (char)pad);
    const std::string key = "eatshit" + std::to_string(authorId);
    BF_KEY bfKey;
    BF_set_key(&bfKey, (int)key.size(), reinterpret_cast<const unsigned char*>(key.data()));
    unsigned char iv[8] = {'a', 'b', 'c', 'd', '1', '2', '3', '4'};
    std::string out(z.size(), '\0');
    BF_cbc_encrypt(reinterpret_cast<const unsigned char*>(z.data()), reinterpret_cast<unsigned char*>(&out[0]),
                   (long)z.size(), &bfKey, iv, BF_ENCRYPT);
    return base64(out);
}

RequestId TjfServices::createLevel(const std::string& name, const std::string& comment, int playableCharacter,
                                   const std::string& xml, ReplyCallback done) {
    const int author = TjfAccount::get()->userId();
    return HWApi::getInstance()->request(
        "set_level.hw",
        form({{"action", "create"}, {"level_name", name}, {"user_comment", comment},
              {"playable_character", std::to_string(playableCharacter)}, {"level_record", encryptLevelRecord(xml, author)}}),
        false, [done](const HWApi::RawResponse& r) {
            if (done) done(parseReply(r, "save"));
        });
}

RequestId TjfServices::updateLevel(int levelId, const std::string& name, const std::string& comment,
                                   int playableCharacter, const std::string& xml, ReplyCallback done) {
    const int author = TjfAccount::get()->userId();
    return HWApi::getInstance()->request(
        "set_level.hw",
        form({{"action", "update"}, {"level_id", std::to_string(levelId)}, {"level_name", name},
              {"user_comment", comment}, {"playable_character", std::to_string(playableCharacter)},
              {"level_record", encryptLevelRecord(xml, author)}}),
        false, [done](const HWApi::RawResponse& r) {
            if (done) done(parseReply(r, "save"));
        });
}

RequestId TjfServices::publishLevel(int levelId, ReplyCallback done) {
    return HWApi::getInstance()->request(
        "set_level.hw", form({{"action", "publish"}, {"level_id", std::to_string(levelId)}}), false,
        [done](const HWApi::RawResponse& r) {
            if (done) done(parseReply(r, "publish"));
        });
}

}  // namespace account
}  // namespace online
