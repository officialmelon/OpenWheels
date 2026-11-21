#include "online/Protocol.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <openssl/blowfish.h>
#include <zlib.h>

#include "tinyxml2/tinyxml2.h"

namespace online {
namespace proto {

namespace {

const char* const kKeyPrefix = "eatshit";
const unsigned char kIV[8] = {'a', 'b', 'c', 'd', '1', '2', '3', '4'};

bool inflateStream(const std::string& in, int windowBits, std::string& out) {
    z_stream zs;
    std::memset(&zs, 0, sizeof zs);
    if (inflateInit2(&zs, windowBits) != Z_OK) return false;
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
    zs.avail_in = (uInt)in.size();
    std::string result;
    char buf[65536];
    int rc;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(buf);
        zs.avail_out = sizeof buf;
        rc = inflate(&zs, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END) break;
        result.append(buf, sizeof buf - zs.avail_out);
    } while (rc != Z_STREAM_END);
    inflateEnd(&zs);
    if (rc != Z_STREAM_END) return false;
    out.swap(result);
    return true;
}

// 1f 8b = gzip. (Raw zlib streams are not guessed: a text reply could start with those bytes.)
bool looksGzip(const std::string& s) {
    return s.size() > 18 && (unsigned char)s[0] == 0x1f && (unsigned char)s[1] == 0x8b;
}

// Bytes that are not valid UTF-8 are taken as Latin-1 (what the site stores), valid sequences stay.
std::string toValidUtf8(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = (unsigned char)s[i];
        size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        bool ok = len != 0 && i + len <= s.size();
        for (size_t k = 1; ok && k < len; ++k) ok = ((unsigned char)s[i + k] & 0xC0) == 0x80;
        if (ok && len == 2 && c < 0xC2) ok = false;  // overlong
        if (ok) {
            out.append(s, i, len);
            i += len;
        } else {  // Latin-1 byte -> U+0080..U+00FF
            out += (char)(0xC0 | (c >> 6));
            out += (char)(0x80 | (c & 0x3F));
            ++i;
        }
    }
    return out;
}

std::string lowerAscii(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return s;
}

bool startsWithNoCase(const std::string& s, size_t at, const char* what) {
    const size_t n = std::strlen(what);
    if (at + n > s.size()) return false;
    for (size_t i = 0; i < n; ++i) {
        char a = s[at + i], b = what[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

bool looksHtml(const std::string& body) {
    size_t i = body.find_first_not_of(" \t\r\n");
    if (i == std::string::npos) return false;
    return startsWithNoCase(body, i, "<!doctype") || startsWithNoCase(body, i, "<html") ||
           lowerAscii(body.substr(i, 400)).find("<html") != std::string::npos;
}

// Drops characters XML cannot hold and escapes '&' that does not start an entity (outside CDATA).
std::string repairXml(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool cdata = false;
    for (size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = (unsigned char)s[i];
        if (!cdata && s.compare(i, 9, "<![CDATA[") == 0) {
            cdata = true;
            out += "<![CDATA[";
            i += 8;
            continue;
        }
        if (cdata && s.compare(i, 3, "]]>") == 0) {
            cdata = false;
            out += "]]>";
            i += 2;
            continue;
        }
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') continue;
        if (c == '&' && !cdata) {
            const size_t semi = s.find(';', i);
            const std::string ent = semi == std::string::npos ? std::string() : s.substr(i + 1, semi - i - 1);
            const bool known = !ent.empty() && ent.size() <= 8 &&
                               (ent == "amp" || ent == "lt" || ent == "gt" || ent == "quot" || ent == "apos" ||
                                (ent[0] == '#' && ent.size() > 1));
            if (!known) {
                out += "&amp;";
                continue;
            }
        }
        out += (char)c;
    }
    return out;
}

std::string decodeEntities(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '&') {
            const size_t semi = s.find(';', i);
            if (semi != std::string::npos && semi - i <= 8) {
                const std::string e = s.substr(i + 1, semi - i - 1);
                if (e == "amp") { out += '&'; i = semi; continue; }
                if (e == "lt") { out += '<'; i = semi; continue; }
                if (e == "gt") { out += '>'; i = semi; continue; }
                if (e == "quot") { out += '"'; i = semi; continue; }
                if (e == "apos") { out += '\''; i = semi; continue; }
                if (e.size() > 1 && e[0] == '#') {
                    const long cp = e[1] == 'x' ? std::strtol(e.c_str() + 2, nullptr, 16) : std::strtol(e.c_str() + 1, nullptr, 10);
                    if (cp > 0 && cp < 0x110000) {
                        if (cp < 0x80) out += (char)cp;
                        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
                        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
                        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
                        i = semi;
                        continue;
                    }
                }
            }
        }
        out += s[i];
    }
    return out;
}

std::string attrValue(const std::string& tag, const char* name) {
    const std::string key = std::string(" ") + name + "=\"";
    const size_t at = tag.find(key);
    if (at == std::string::npos) return std::string();
    const size_t from = at + key.size();
    const size_t to = tag.find('"', from);
    if (to == std::string::npos) return std::string();
    return decodeEntities(tag.substr(from, to - from));
}

void fillLevel(OnlineLevelInfo& l, const std::string& id, const std::string& ui, const std::string& rg,
               const std::string& vs, const std::string& ps, const std::string& pc) {
    l.id = std::atoi(id.c_str());
    l.authorId = std::atoi(ui.c_str());
    l.rating = (float)std::atof(rg.c_str());
    l.votes = std::atoi(vs.c_str());
    l.plays = std::atoi(ps.c_str());
    l.character = std::atoi(pc.c_str());
}

// Last resort: read <lv ...> tags one by one, whatever the rest of the document looks like.
bool scanLevels(const std::string& body, std::vector<OnlineLevelInfo>& levels, int* page, int* perPage) {
    const size_t lvs = body.find("<lvs");
    if (lvs == std::string::npos) return false;
    const size_t lvsEnd = body.find('>', lvs);
    if (lvsEnd != std::string::npos) {
        const std::string head = body.substr(lvs, lvsEnd - lvs);
        if (page) *page = std::max(1, std::atoi(attrValue(head, "pg").c_str()));
        if (perPage) *perPage = std::atoi(attrValue(head, "pp").c_str());
    }
    for (size_t at = body.find("<lv ", lvs); at != std::string::npos; at = body.find("<lv ", at + 4)) {
        const size_t end = body.find('>', at);
        if (end == std::string::npos) break;
        const std::string tag = body.substr(at, end - at);
        OnlineLevelInfo l;
        fillLevel(l, attrValue(tag, "id"), attrValue(tag, "ui"), attrValue(tag, "rg"), attrValue(tag, "vs"),
                  attrValue(tag, "ps"), attrValue(tag, "pc"));
        l.name = attrValue(tag, "ln");
        l.authorName = attrValue(tag, "un");
        l.published = attrValue(tag, "dp");
        const size_t next = body.find("<lv ", end);
        const size_t cd = body.find("<![CDATA[", end);
        if (cd != std::string::npos && (next == std::string::npos || cd < next)) {
            const size_t cdEnd = body.find("]]>", cd);
            if (cdEnd != std::string::npos) l.comment = body.substr(cd + 9, cdEnd - cd - 9);
        }
        if (l.id) levels.push_back(l);
    }
    return true;
}

bool readList(const std::string& body, std::vector<OnlineLevelInfo>& levels, int* page, int* perPage) {
    tinyxml2::XMLDocument doc;
    if (doc.Parse(body.c_str(), body.size()) != tinyxml2::XML_SUCCESS) return false;
    tinyxml2::XMLElement* lvs = doc.FirstChildElement("lvs");
    if (!lvs) return false;
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

std::string snippet(const std::string& body) {
    std::string s;
    for (size_t i = 0; i < body.size() && s.size() < 60; ++i) {
        const unsigned char c = (unsigned char)body[i];
        s += (c >= 0x20 && c < 0x7f) ? (char)c : '.';
    }
    return s;
}

}  // namespace

std::string normalizeReply(const std::string& raw) {
    std::string body = raw;
    std::string inflated;
    if (looksGzip(body) && inflateStream(body, 16 + MAX_WBITS, inflated)) body = inflated;
    if (body.size() >= 3 && (unsigned char)body[0] == 0xEF && (unsigned char)body[1] == 0xBB && (unsigned char)body[2] == 0xBF)
        body.erase(0, 3);
    const size_t first = body.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) body.clear();  // only whitespace
    else if (first > 0) body.erase(0, first);
    return toValidUtf8(body);
}

bool classifyReply(const std::string& body, int httpStatus, std::string* error) {
    const std::string status = httpStatus > 0 ? " (HTTP " + std::to_string(httpStatus) + ")" : std::string();
    const std::string head = body.substr(0, 64);
    if (head.find("failure") != std::string::npos) {
        if (error) *error = "server: " + body.substr(0, 80);
        return true;
    }
    if (looksHtml(body)) {
        const bool challenge = body.find("Just a moment") != std::string::npos || body.find("cf-chl") != std::string::npos ||
                               body.find("challenge-platform") != std::string::npos ||
                               body.find("cf-mitigated") != std::string::npos;
        if (error)
            *error = challenge ? "totaljerkface.com's bot protection blocked the request" + status +
                                     ". Try again later or from another network."
                               : "server returned an error page" + status;
        return true;
    }
    return false;
}

bool parseLevelList(const std::string& rawBody, int httpStatus, std::vector<OnlineLevelInfo>& levels, int* page,
                    int* perPage, std::string* error) {
    levels.clear();
    const std::string body = normalizeReply(rawBody);
    if (classifyReply(body, httpStatus, error)) return false;
    if (body.empty()) {  // some multi-word searches come back as a zero-byte 200
        if (page) *page = 1;
        if (perPage) *perPage = 0;
        return true;
    }
    if (readList(body, levels, page, perPage)) return true;
    levels.clear();
    if (readList(repairXml(body), levels, page, perPage)) return true;
    levels.clear();
    if (scanLevels(body, levels, page, perPage)) return true;
    if (error) {
        *error = "unreadable level list";
        if (httpStatus > 0) *error += " (HTTP " + std::to_string(httpStatus) + ")";
        *error += ", " + std::to_string(rawBody.size()) + " bytes, starts: \"" + snippet(body) + "\"";
    }
    return false;
}

bool decryptRecord(const std::string& rawRecord, int authorId, std::string& xml, std::string* error) {
    std::string record = rawRecord;
    std::string inflated;
    if (looksGzip(record) && inflateStream(record, 16 + MAX_WBITS, inflated)) record = inflated;
    std::string err;
    if (classifyReply(record, 0, &err)) {
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
    BF_cbc_encrypt(reinterpret_cast<const unsigned char*>(record.data()), plain.data(), (long)record.size(), &bfKey, iv,
                   BF_DECRYPT);
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
    std::string packed(reinterpret_cast<const char*>(plain.data()), plain.size());
    std::string out;
    if (!inflateStream(packed, MAX_WBITS, out)) {
        if (error) *error = "level data could not be unpacked";
        return false;
    }
    xml.swap(out);
    return true;
}

std::string encryptRecord(const std::string& xml, int authorId) {
    uLongf packedSize = compressBound((uLong)xml.size());
    std::string packed(packedSize, '\0');
    compress(reinterpret_cast<Bytef*>(&packed[0]), &packedSize, reinterpret_cast<const Bytef*>(xml.data()), (uLong)xml.size());
    packed.resize(packedSize);
    const unsigned pad = 8 - (unsigned)(packed.size() % 8);
    packed.append(pad, (char)pad);
    const std::string key = kKeyPrefix + std::to_string(authorId);
    BF_KEY bfKey;
    BF_set_key(&bfKey, (int)key.size(), reinterpret_cast<const unsigned char*>(key.data()));
    std::string out(packed.size(), '\0');
    unsigned char iv[8];
    std::memcpy(iv, kIV, 8);
    BF_cbc_encrypt(reinterpret_cast<const unsigned char*>(packed.data()), reinterpret_cast<unsigned char*>(&out[0]),
                   (long)packed.size(), &bfKey, iv, BF_ENCRYPT);
    return out;
}

std::vector<std::string> splitWords(const std::string& term) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : term) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

bool termMatches(const std::string& hay, const std::string& term) {
    const std::string h = lowerAscii(hay);
    for (const std::string& w : splitWords(term))
        if (h.find(lowerAscii(w)) == std::string::npos) return false;
    return true;
}

}  // namespace proto
}  // namespace online
