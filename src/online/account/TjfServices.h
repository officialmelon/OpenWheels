#pragma once
// ONLINE (PC addition): the logged-in player's actions on totaljerkface.com, request for request
// what the decompiled Flash v1.87 client sends (menus/LevelBrowser, menus/SessionMenu,
// editor/LoadMenu, editor/SaverLoader). All need the session cookie (TjfAccount); the server
// answers "success[:<value>]" or "failure:<reason>".
//
//   user.hw       get_favorites                      -> <lvs> like get_level.hw lists
//                 set_favorite / delete_favorite      level_id
//   set_level.hw  rate_level                          level_id, rating (1..5)
//                 create                              level_name, user_comment, playable_character,
//                                                     level_record -> success:<new level id>
//                 update                              level_id + the create fields
//                 publish                             level_id (one per day: time_lockout)
//   get_level.hw  get_cmb_by_user                     -> <x><private><lvs/></private><published><lvs/></published></x>
//                 get_pub_by_user                     user_id + get_all fields (public, no login)
//
// level_record = base64( Blowfish-CBC( key "eatshit"+<author id>, IV "abcd1234", PKCS#5 )
//                        over zlib( UTF-8 level XML ) ) - LevelEncryptor / SaverLoader.saveNewLevel.

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "online/HWApi.h"
#include "online/OnlineLevel.h"

namespace online {
namespace account {

// A "success:..." / "failure:..." reply.
struct Reply {
    bool ok = false;
    std::string value;     // after "success:"
    std::string reason;    // after "failure:" (or "network", "system_error")
    std::string message;   // player-facing text (the Flash client's wording)
};

using ReplyCallback = std::function<void(const Reply& reply)>;
using ListCallback = std::function<void(bool ok, const std::string& error, const std::vector<OnlineLevelInfo>& levels)>;

class TjfServices {
public:
    static TjfServices* get();

    // Favorites. favorites() is the last get_favorites result (ids), kept in sync by set/delete.
    RequestId listFavorites(ListCallback done);
    RequestId setFavorite(int levelId, bool favorite, ReplyCallback done);
    bool isFavorite(int levelId) const { return _favorites.count(levelId) != 0; }
    bool favoritesKnown() const { return _favoritesKnown; }

    // Level rating (1..5 stars; the Flash VotingStars).
    RequestId rateLevel(int levelId, int rating, ReplyCallback done);

    // The player's own levels: private (saved, unpublished) and published.
    using MyLevelsCallback = std::function<void(bool ok, const std::string& error,
                                                const std::vector<OnlineLevelInfo>& privateLevels,
                                                const std::vector<OnlineLevelInfo>& published)>;
    RequestId listMyLevels(MyLevelsCallback done);
    // Anyone's published levels (get_pub_by_user, public).
    RequestId listPublishedBy(int userId, int page, ListCallback done);

    // Editor upload. xml: browser-format level XML (<levelXML>...). create -> reply.value = new id.
    RequestId createLevel(const std::string& name, const std::string& comment, int playableCharacter,
                          const std::string& xml, ReplyCallback done);
    RequestId updateLevel(int levelId, const std::string& name, const std::string& comment, int playableCharacter,
                          const std::string& xml, ReplyCallback done);
    RequestId publishLevel(int levelId, ReplyCallback done);

    // LevelEncryptor.encryptByteArray(compress(xml), "eatshit" + authorId), base64.
    static std::string encryptLevelRecord(const std::string& xml, int authorId);
    // Shared reply parsing (also used by the replay module): maps reasons to the Flash texts and
    // ends the local session on not_logged_in.
    static Reply parseReply(const HWApi::RawResponse& r, const std::string& what);
    static std::string base64(const std::string& bytes);
    static std::string form(std::initializer_list<std::pair<const char*, std::string>> fields);

private:
    TjfServices() = default;
    std::set<int> _favorites;
    bool _favoritesKnown = false;
};

}  // namespace account
}  // namespace online
