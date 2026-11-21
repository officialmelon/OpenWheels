#pragma once
// ONLINE (PC addition): the engine-free half of the browser-site protocol (no cocos2d-x): reply
// clean-up, error classification, level lists, record decryption and search matching. HWApi
// (the HTTP side) calls these, and tests/unit/ runs them without the engine, so what the site
// sends can be checked in CI (the "unreadable level list" and "error page" bugs lived here).

#include <string>
#include <vector>

#include "online/OnlineLevel.h"

namespace online {
namespace proto {

// Makes a raw HTTP body usable text: undoes gzip / zlib that the HTTP stack left on, drops a UTF-8
// BOM and leading whitespace, and turns stray non-UTF-8 bytes (the site sends Latin-1 0xA0 in some
// level names) into valid UTF-8 so nothing downstream chokes on them.
std::string normalizeReply(const std::string& raw);

// "failure:<reason>" or an HTML page (Cloudflare's "Just a moment...", a 4xx/5xx page) instead of
// data. true = it is an error and `error` says which, with the HTTP status when known (> 0).
bool classifyReply(const std::string& body, int httpStatus, std::string* error);

// A <lvs><lv .../>...</lvs> reply. Tolerant: stray bytes, bare '&' and control characters are
// repaired, and as a last resort the <lv> tags are read one by one. An empty body is an empty list
// (the site answers some multi-word searches that way). false + `error` (with a short description
// of what arrived) when it is neither a list nor an error classified above.
bool parseLevelList(const std::string& rawBody, int httpStatus, std::vector<OnlineLevelInfo>& levels, int* page,
                    int* perPage, std::string* error);

// Blowfish-CBC (key "eatshit" + author id, IV "abcd1234", PKCS#5) over zlib, as the site serves
// level records. Also accepts a record whose HTTP layer left it gzipped.
bool decryptRecord(const std::string& record, int authorId, std::string& xml, std::string* error);
// The inverse, for tests and tools.
std::string encryptRecord(const std::string& xml, int authorId);

// Every whitespace-separated word of `term` appears in `hay` (any order, ASCII case-insensitive).
bool termMatches(const std::string& hay, const std::string& term);
std::vector<std::string> splitWords(const std::string& term);

}  // namespace proto
}  // namespace online
