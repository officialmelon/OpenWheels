// Level lists, replies and records as the site really sends them (and as it has broken before).
#include <cstring>
#include <string>
#include <vector>

#include <zlib.h>

#include "check.h"
#include "online/Protocol.h"

using namespace online;
using online::proto::classifyReply;
using online::proto::decryptRecord;
using online::proto::encryptRecord;
using online::proto::normalizeReply;
using online::proto::parseLevelList;
using online::proto::termMatches;

namespace {

std::string lv(int id, const std::string& name, const std::string& author = "someone", const std::string& comment = "hi") {
    return "<lv dp=\"2026-11-16\" id=\"" + std::to_string(id) + "\" ln=\"" + name + "\" pc=\"9\" ps=\"18688\" rg=\"4.8101\" ui=\"10350857\" un=\"" +
           author + "\" vs=\"685\"><uc><![CDATA[" + comment + "]]></uc></lv>";
}

std::string list(const std::string& inner, const std::string& head = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>") {
    return head + "<lvs pg=\"1\" pp=\"500\">" + inner + "</lvs>";
}

std::string gzip(const std::string& in) {
    z_stream zs;
    std::memset(&zs, 0, sizeof zs);
    deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    std::string out(deflateBound(&zs, (uLong)in.size()) + 32, '\0');
    zs.next_in = (Bytef*)in.data();
    zs.avail_in = (uInt)in.size();
    zs.next_out = (Bytef*)&out[0];
    zs.avail_out = (uInt)out.size();
    deflate(&zs, Z_FINISH);
    out.resize(zs.total_out);
    deflateEnd(&zs);
    return out;
}

bool parse(const std::string& body, std::vector<OnlineLevelInfo>& levels, std::string* error = nullptr, int status = 200) {
    int page = 0, perPage = -1;
    std::string err;
    const bool ok = parseLevelList(body, status, levels, &page, &perPage, &err);
    if (error) *error = err;
    return ok;
}

}  // namespace

TEST(list_parses_fields) {
    std::vector<OnlineLevelInfo> v;
    int page = 0, perPage = 0;
    std::string err;
    CHECK(parseLevelList(list(lv(11139233, "Doom 1993 (WORKING)", "amcommite") + lv(5, "second")), 200, v, &page, &perPage, &err));
    CHECK_EQ(v.size(), (size_t)2);
    CHECK_EQ(page, 1);
    CHECK_EQ(perPage, 500);
    CHECK_EQ(v[0].id, 11139233);
    CHECK_EQ(v[0].name, std::string("Doom 1993 (WORKING)"));
    CHECK_EQ(v[0].authorName, std::string("amcommite"));
    CHECK_EQ(v[0].authorId, 10350857);
    CHECK_EQ(v[0].plays, 18688);
    CHECK_EQ(v[0].votes, 685);
    CHECK_EQ(v[0].character, 9);
    CHECK_EQ(v[0].published, std::string("2026-11-16"));
    CHECK_EQ(v[0].comment, std::string("hi"));
    CHECK(v[0].rating > 4.8f && v[0].rating < 4.82f);
}

// The server really sent ln="hi\xA0\xA0" (a Latin-1 byte in a UTF-8 document, 2026-11-20): the
// list must still load and the name must be valid UTF-8.
TEST(list_survives_latin1_bytes) {
    std::vector<OnlineLevelInfo> v;
    std::string err;
    CHECK(parse(list(lv(1, "ok") + lv(11118408, std::string("hi\xA0\xA0")) + lv(3, "after")), v, &err));
    CHECK_EQ(v.size(), (size_t)3);
    CHECK_EQ(v[1].name, std::string("hi\xC2\xA0\xC2\xA0"));
}

TEST(list_survives_bom_and_whitespace) {
    std::vector<OnlineLevelInfo> v;
    CHECK(parse(std::string("\xEF\xBB\xBF\r\n  ") + list(lv(1, "a")), v));
    CHECK_EQ(v.size(), (size_t)1);
}

TEST(list_gzip_left_on_by_http_stack) {
    std::vector<OnlineLevelInfo> v;
    std::string err;
    CHECK(parse(gzip(list(lv(1, "a") + lv(2, "b"))), v, &err));
    CHECK_EQ(v.size(), (size_t)2);
}

TEST(list_bare_ampersand_and_control_chars) {
    std::vector<OnlineLevelInfo> v;
    std::string err;
    CHECK(parse(list(lv(1, "Tom & Jerry") + lv(2, std::string("ctrl") + char(1) + "x")), v, &err));
    CHECK_EQ(v.size(), (size_t)2);
    CHECK_EQ(v[0].name, std::string("Tom & Jerry"));
}

TEST(list_truncated_keeps_what_arrived) {
    std::vector<OnlineLevelInfo> v;
    const std::string whole = list(lv(1, "a") + lv(2, "b") + lv(3, "c"));
    CHECK(parse(whole.substr(0, whole.size() - 60), v));  // cut inside the last <lv>
    CHECK(v.size() >= 2);
}

TEST(list_keeps_carriage_returns_in_comments) {
    std::vector<OnlineLevelInfo> v;
    CHECK(parse(list(lv(1, "a", "x", "line1\rline2\r\nline3 & more")), v));
    CHECK_EQ(v.size(), (size_t)1);
    CHECK_CONTAINS(v[0].comment, "line2");
}

TEST(list_entities_in_names) {
    std::vector<OnlineLevelInfo> v;
    CHECK(parse(list(lv(1, "a &amp; b &lt;3 &#233;")), v));
    CHECK_EQ(v[0].name, std::string("a & b <3 \xC3\xA9"));
}

TEST(empty_reply_is_an_empty_list) {
    std::vector<OnlineLevelInfo> v;
    CHECK(parse("", v));
    CHECK(v.empty());
    CHECK(parse(" \r\n", v));
}

TEST(cloudflare_challenge_is_reported_as_blocked) {
    const char* pages[] = {
        "<!DOCTYPE html><html lang=\"en-US\"><head><title>Just a moment...</title>",
        "<!doctype html><html><head><title>Just a moment...</title>",
        "\r\n\r\n   <!DOCTYPE html>\n<html><title>Just a moment...</title>",
        "<html><head><script src=\"/cdn-cgi/challenge-platform/h/b\"></script>",
    };
    for (const char* page : pages) {
        std::vector<OnlineLevelInfo> v;
        std::string err;
        CHECK(!parse(page, v, &err, 403));
        CHECK_CONTAINS(err, "bot protection");
        CHECK_CONTAINS(err, "HTTP 403");
    }
}

TEST(other_error_pages_and_failures) {
    std::vector<OnlineLevelInfo> v;
    std::string err;
    CHECK(!parse("<html><body>502 Bad Gateway</body></html>", v, &err, 502));
    CHECK_CONTAINS(err, "error page");
    CHECK_CONTAINS(err, "HTTP 502");
    CHECK(!parse("failure:app_error", v, &err));
    CHECK_CONTAINS(err, "failure:app_error");
}

TEST(garbage_reports_what_arrived) {
    std::vector<OnlineLevelInfo> v;
    std::string err;
    CHECK(!parse("{\"levels\":[]}", v, &err));
    CHECK_CONTAINS(err, "unreadable level list");
    CHECK_CONTAINS(err, "starts:");
    CHECK_CONTAINS(err, "{\"levels\"");
    CHECK(!parse("just some words", v, &err, 200));
    CHECK_CONTAINS(err, "15 bytes");
}

TEST(normalize_reply_basics) {
    CHECK_EQ(normalizeReply("\xEF\xBB\xBF  hello"), std::string("hello"));
    CHECK_EQ(normalizeReply(gzip(std::string("payload payload payload payload"))), std::string("payload payload payload payload"));
    CHECK_EQ(normalizeReply("caf\xE9"), std::string("caf\xC3\xA9"));
    CHECK_EQ(normalizeReply("caf\xC3\xA9"), std::string("caf\xC3\xA9"));  // valid UTF-8 untouched
}

TEST(classify_reply_leaves_data_alone) {
    std::string err;
    CHECK(!classifyReply(list(lv(1, "a")), 200, &err));
    CHECK(!classifyReply("1234|5678", 200, &err));
}

TEST(record_roundtrip_and_failures) {
    const std::string xml = "<levelXML><info v=\"1.95\" x=\"NaN\" y=\"5100\" c=\"11\"/></levelXML>";
    const std::string rec = encryptRecord(xml, 11115790);
    std::string out, err;
    CHECK(decryptRecord(rec, 11115790, out, &err));
    CHECK_EQ(out, xml);
    // wrong author id (the key suffix), truncated, empty, an HTML page, gzip left on
    CHECK(!decryptRecord(rec, 11115791, out, &err));
    CHECK(!decryptRecord(rec.substr(0, rec.size() - 3), 11115790, out, &err));
    CHECK_CONTAINS(err, "damaged");
    CHECK(!decryptRecord("", 1, out, &err));
    CHECK(!decryptRecord("<!DOCTYPE html><html><title>Just a moment...</title></html>", 1, out, &err));
    CHECK_CONTAINS(err, "bot protection");
    CHECK(decryptRecord(gzip(rec), 11115790, out, &err));
    CHECK_EQ(out, xml);
}

TEST(record_big_level) {
    std::string xml = "<levelXML>";
    for (int i = 0; i < 40000; ++i) xml += "<sh t=\"0\" p0=\"" + std::to_string(i) + "\" p1=\"5\"/>";
    xml += "</levelXML>";
    std::string out, err;
    CHECK(decryptRecord(encryptRecord(xml, 42), 42, out, &err));
    CHECK_EQ(out.size(), xml.size());
}

TEST(search_matching) {
    CHECK(termMatches("Spinning Gears", "spinning gears"));
    CHECK(termMatches("Spinning Gears", "GEARS spinning"));
    CHECK(termMatches("The Combine 2.1", "combine"));
    CHECK(!termMatches("Spinning Gears", "spinning wheels"));
    CHECK(termMatches("anything", ""));
    CHECK_EQ(proto::splitWords("  a  b\tc ").size(), (size_t)3);
}
