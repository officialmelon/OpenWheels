#include "online/FlashLevelConverter.h"

// Browser level XML -> mobile level XML (see docs/FLASH_LEVELS.md section 5/6).
//
// The two formats share element names, type ids and units, but the mobile loader (LevelB2D, a 1:1
// reconstruction that must not change) trusts its input completely: fixed-size vertex arrays,
// unchecked item indices in joints and triggers, null bodies passed to Box2D, a 1600-entry draw
// table per shape layer, sprite tables indexed by item parameters. So instead of patching the
// browser XML in place, the converter re-emits every element from a whitelist of the attributes
// the mobile code reads, with each value checked against what that code can take:
//
// * <info>: the Flash units (ptm/sw/sh/r/cw), a character the mobile game has, defaults the mobile
//   loader lacks (bgc, opacity...).
// * shapes: polygon/art vertex lists resolved (shared "<v id>" lists), art bezier handles
//   flattened, outlines simplified to the loader's limits (8 physics vertices, 100 art vertices)
//   while still triangulating the way FFDrawNode does, Flash's vertex scaling baked in.
// * specials: supported ids sanitised to the ranges their classes index with, unsupported ones
//   dropped or replaced by placeholder shapes of their footprint, chains rebuilt from shapes.
// * groups: shapes forced onto the group body, specials the mobile group code ignores replaced.
// * joints and trigger targets: every reference re-resolved to an item the mobile loader really
//   creates (and that has a body where a body is needed); indices remapped after removals.
// * triggers: multi-action targets (v >= 1.87) flattened, missing action properties filled in,
//   action combinations that crash the mobile runtime removed (see the notes in convertTriggers).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "online/FlashGeometry.h"
#include "tinyxml2/tinyxml2.h"

namespace online {

namespace {

using geom::Pt;
using tinyxml2::XMLElement;

const double kPi = 3.14159265358979323846;
const double kMaxCoord = 200000.0;     // px; Flash's canvas is 20000 x 10000
const double kMaxSize = 100000.0;      // px
const double kMinPhysicalSize = 0.5;   // px; zero-area dynamic fixtures give NaN mass centres
const size_t kMaxPhysicsVerts = 8;     // b2_maxPolygonVertices in the game's Box2D
const size_t kMaxArtVerts = 100;       // LevelB2D::addShape Vec2 verts[100]
const int kDrawDelegateLimit = 1590;   // FFDrawNode::_artDelegates[1600] per shape layer
const int kMaxTargetIndex = 10000;     // LevelB2D::_targetActions keys: shapes < 10000 <= joints...
const int kSoundCount = 326;           // SoundList::_sfxArray

// ---------------------------------------------------------------------------------------------
// Output tree, written as text at the end.

struct Node {
    std::string name;
    std::vector<std::pair<std::string, std::string>> attrs;
    std::vector<Node> children;

    explicit Node(const std::string& n = std::string()) : name(n) {}

    void set(const std::string& key, const std::string& value) {
        for (auto& a : attrs) {
            if (a.first == key) {
                a.second = value;
                return;
            }
        }
        attrs.emplace_back(key, value);
    }
    void setNum(const std::string& key, double value);
    void setInt(const std::string& key, long long value) { set(key, std::to_string(value)); }
    void setBool(const std::string& key, bool value) { set(key, value ? "t" : "f"); }
    const std::string* get(const std::string& key) const {
        for (const auto& a : attrs) {
            if (a.first == key) return &a.second;
        }
        return nullptr;
    }
};

std::string formatNumber(double value) {
    if (!std::isfinite(value)) value = 0.0;
    if (value == 0.0) return "0";
    if (std::fabs(value) < 1e15 && value == std::floor(value)) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%lld", (long long)value);
        return buffer;
    }
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "%.9g", value);
    return buffer;
}

void Node::setNum(const std::string& key, double value) { set(key, formatNumber(value)); }

void appendEscaped(std::string& out, const std::string& text) {
    for (unsigned char c : text) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default:
            if (c < 0x20) {
                out += ' ';
            } else {
                out += (char)c;
            }
        }
    }
}

void writeNode(std::string& out, const Node& node, int depth) {
    out.append((size_t)depth, ' ');
    out += '<';
    out += node.name;
    for (const auto& a : node.attrs) {
        out += ' ';
        out += a.first;
        out += "=\"";
        appendEscaped(out, a.second);
        out += '"';
    }
    if (node.children.empty()) {
        out += "/>\n";
        return;
    }
    out += ">\n";
    for (const Node& child : node.children) writeNode(out, child, depth + 1);
    out.append((size_t)depth, ' ');
    out += "</";
    out += node.name;
    out += ">\n";
}

// ---------------------------------------------------------------------------------------------
// Input helpers. Flash reads attributes with Number()/int() conversions; anything that is not a
// finite number falls back to the item's default here.

const char* attr(const XMLElement* e, const char* key) {
    const char* value = e ? e->Attribute(key) : nullptr;
    return value;
}

bool readNum(const XMLElement* e, const char* key, double* out) {
    const char* text = attr(e, key);
    if (!text) return false;
    while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') text++;
    if (!*text) return false;
    char* end = nullptr;
    double value = strtod(text, &end);
    if (end == text || !std::isfinite(value)) return false;
    *out = value;
    return true;
}

double num(const XMLElement* e, const char* key, double def) {
    double value;
    return readNum(e, key, &value) ? value : def;
}

double numClamped(const XMLElement* e, const char* key, double def, double low, double high) {
    return std::max(low, std::min(high, num(e, key, def)));
}

// AS3 int(): truncation toward zero.
int inum(const XMLElement* e, const char* key, int def) {
    double value;
    if (!readNum(e, key, &value)) return def;
    if (value > 2.0e9 || value < -2.0e9) return def;
    return (int)std::trunc(value);
}

int inumClamped(const XMLElement* e, const char* key, int def, int low, int high) {
    return std::max(low, std::min(high, inum(e, key, def)));
}

bool flag(const XMLElement* e, const char* key, bool def) {
    const char* text = attr(e, key);
    if (!text) return def;
    if (!strcmp(text, "t") || !strcmp(text, "1") || !strcmp(text, "true")) return true;
    if (!strcmp(text, "f") || !strcmp(text, "0") || !strcmp(text, "false")) return false;
    return def;
}

double coord(double value) { return std::max(-kMaxCoord, std::min(kMaxCoord, value)); }

double angleDeg(double value) {
    if (!std::isfinite(value)) return 0.0;
    return std::fmod(value, 360.0);
}

// ---------------------------------------------------------------------------------------------
// Converted items.

enum class BodyKind {
    None,     // no body a joint could use
    Level,    // the static level body
    Own,      // a body of its own
};

struct ShapeRec {
    Node node{"sh"};
    bool valid = false;
    bool interactive = false;
    BodyKind body = BodyKind::None;  // for joints (top-level shapes only)
    bool visible = true;
    double area = 0.0;               // px^2, decides which decoration goes first when over budget
    bool referenced = false;         // by a joint or a trigger
    bool dropped = false;
    int outIndex = -1;
};

struct ChainInfo {
    std::vector<int> linkShapes;     // indices into the extra shapes
    std::vector<Pt> linkCenters;     // world px
};

enum class SpecialFate { Keep, Placeholder, Chain, Drop };

struct SpecialRec {
    int type = -1;
    SpecialFate fate = SpecialFate::Drop;
    Node node{"sp"};
    bool jointable = false;          // getJointBody returns a body
    BodyKind jointBody = BodyKind::None;
    int placeholder = -1;            // index into extra shapes
    ChainInfo chain;
    int outIndex = -1;
};

struct GroupRec {
    Node node{"g"};
    bool hasBody = false;            // ends up with fixtures (addGroup keeps the body)
    bool foreground = false;
    std::vector<ShapeRec> shapes;    // group shapes (kept in node only at the end)
    std::vector<Node> specials;
    int outIndex = -1;
};

struct Ref {
    enum Kind { Level, Shape, Special, Group, Invalid } kind = Invalid;
    int index = -1;
};

struct JointRec {
    Node node{"j"};
    bool keep = false;
    bool prismatic = false;
    Ref b1;
    Ref b2;
    int outIndex = -1;
};

struct Action {
    int a = -1;                      // -1: no action attribute (activation-only target)
    std::vector<double> props;
};

struct TargetRec {
    std::string kind;                // t sh j g sp
    int index = -1;                  // old index
    std::vector<Action> actions;
};

struct TriggerRec {
    Node node{"t"};
    int type = 0;
    int triggeredBy = 0;
    int repeat = 1;
    double delay = 0.0;
    std::vector<TargetRec> targets;
};

const char* const kCharacterNames[] = {
    "", "Wheelchair Guy", "Segway Guy", "Irresponsible Dad", "Effective Shopper", "Moped Couple",
    "Lawnmower Man", "Explorer Guy", "Santa Claus", "Pogostick Man", "Irresponsible Mom",
    "Helicopter Man"};

const char* const kSpecialNames[] = {
    "van", "table", "mine", "I-beam", "log", "spring box", "spikes", "wrecking ball", "fan",
    "finish line", "soccer ball", "meteor", "boost panel", "building", "building", "harpoon gun",
    "text box", "NPC character", "glass pane", "chair", "bottle", "TV", "boombox", "sign",
    "toilet", "homing mine", "trash can", "rail", "jet", "arrow gun", "chain", "token",
    "food item", "cannon", "blade weapon", "paddle"};

std::string plural(int count, const char* singular, const char* pluralForm = nullptr) {
    std::string word = count == 1 ? singular : (pluralForm ? pluralForm : std::string(singular) + "s");
    return std::to_string(count) + " " + word;
}

class Converter {
public:
    explicit Converter(ConversionReport& report) : _report(report) {}
    std::string run(const std::string& flashXml);

private:
    // info
    bool convertInfo(const XMLElement* info);
    // shapes
    bool convertShape(const XMLElement* e, bool inGroup, ShapeRec& out);
    bool buildPolygon(const XMLElement* e, int type, bool interactive, double width, bool hasWidth,
                      double height, bool hasHeight, ShapeRec& out, bool* becameArt);
    void emitVerts(Node& shapeNode, const std::vector<Pt>& points);
    ShapeRec makeBox(double x, double y, double w, double h, double angle, bool interactive,
                     bool immovable, bool sleeping, double density, int color, double opacity,
                     int collision, bool inGroup);
    ShapeRec makeCircle(double x, double y, double diameter, bool interactive, bool immovable,
                        bool sleeping, double density, int color, double opacity, bool inGroup);
    // specials
    void convertSpecial(const XMLElement* e, int index);
    bool sanitizeSupportedSpecial(const XMLElement* e, int type, bool inGroup, Node& node,
                                  bool* jointable, BodyKind* jointBody, bool* addsGroupFixture);
    bool placeholderFor(const XMLElement* e, int type, bool inGroup, ShapeRec* shape);
    void buildChain(const XMLElement* e, SpecialRec& rec);
    // groups
    void convertGroup(const XMLElement* e);
    // joints / triggers
    Ref parseBodyRef(const char* text);
    void convertJoints(const XMLElement* joints);
    void parseTriggers(const XMLElement* triggers);
    void markReferences();
    void enforceDrawBudget();
    void assignIndices();
    bool resolveJointBody(const Ref& ref, const Pt& anchor, std::string* out, int* identity);
    std::vector<Node> emitTargets(const TriggerRec& trigger, int triggerIndex);
    void finishTriggers();
    std::string write();
    void warn(const std::string& text) { _report.warnings.push_back(text); }
    void count(const std::string& key, int n = 1) { _counts[key] += n; }

    ConversionReport& _report;
    double _version = 1.87;
    std::string _versionText = "1.87";
    bool _dotVerts = false;           // browser vertices: '.' separated (v < 1.84)
    bool _mobileUnderscore = true;    // mobile stringToVec separator for this version
    Node _info{"info"};

    std::map<int, std::vector<std::string>> _polyCache;
    std::map<int, std::vector<std::string>> _artCache;

    std::vector<ShapeRec> _shapes;    // top-level <sh>, old order
    std::vector<ShapeRec> _extra;     // placeholders and chain links (appended after _shapes)
    std::vector<SpecialRec> _specials;
    std::vector<GroupRec> _groups;
    std::vector<JointRec> _joints;
    std::vector<JointRec> _extraJoints;  // chain links
    std::vector<TriggerRec> _triggers;
    std::vector<Node> _triggerOut;

    std::map<std::string, int> _counts;
    std::set<int> _unknownSpecialIds;
    int _jointCount = 0;
};

// ---------------------------------------------------------------------------------------------
// <info>

bool Converter::convertInfo(const XMLElement* info) {
    double version;
    if (readNum(info, "v", &version) && version > 0.0) {
        _version = version;
        _versionText = attr(info, "v");
    }
    // Flash: split('.') below 1.84; mobile LevelB2D::stringToVec: (float)v >= 1.84 -> '_'.
    _dotVerts = _version < 1.84;
    _mobileUnderscore = (double)(float)_version >= 1.84;

    _info.set("v", formatNumber(_version));
    _info.setNum("x", coord(num(info, "x", 0.0)));
    _info.setNum("y", coord(num(info, "y", 0.0)));

    int character = inum(info, "c", 1);
    bool forced = flag(info, "f", false);
    int mobile = character;
    switch (character) {
    case 1: case 2: case 3: case 4: case 5: case 9: break;
    case 6: mobile = 4; break;   // lawnmower -> motor cart
    case 7: mobile = 4; break;   // explorer's mine cart -> motor cart
    case 8: mobile = 5; break;   // santa's sleigh with elves -> moped couple
    case 10: mobile = 3; break;  // irresponsible mom -> irresponsible dad
    case 11: mobile = 2; break;  // helicopter man -> segway guy
    default: mobile = 1; break;
    }
    if (mobile != character) {
        if (forced && character >= 1 && character <= 11) {
            warn(std::string(kCharacterNames[character]) + " isn't in this version; pick any character");
        }
        forced = false;
    }
    _info.setInt("c", mobile);
    _info.setBool("f", forced);
    _info.setBool("h", flag(info, "h", false));
    int background = inum(info, "bg", 0);
    if (background < 0 || background > 2) background = 0;
    // Android 1.1.3 ships no city gradient (BackgroundLayer would crash on the missing file):
    // the dusky night horizon is the closest backdrop it has.
    if (background == 2) background = 4000;
    _info.setInt("bg", background);
    _info.setInt("bgc", inum(info, "bgc", 16777215) & 0xffffff);
    if (const char* e = attr(info, "e")) _info.set("e", e);
    // Flash units: 62.5 px/m, a 20000 x 10000 px stage, y down, rotations clockwise.
    _info.set("ptm", "62.5");
    _info.set("sw", "20000");
    _info.set("sh", "10000");
    _info.set("r", "1");
    _info.set("cw", "1");
    _report.character = mobile;
    _report.forceCharacter = forced;
    return true;
}

// ---------------------------------------------------------------------------------------------
// Shapes

void Converter::emitVerts(Node& shapeNode, const std::vector<Pt>& points) {
    // Integers (stringToVec uses atoi) scaled by k for sub-pixel precision; p2/p3 are set so
    // that LevelB2D's stretch (p2 / extent incl. the origin) divides k out again.
    double maxAbs = 1.0;
    for (const Pt& p : points) maxAbs = std::max(maxAbs, std::max(std::fabs(p.x), std::fabs(p.y)));
    int k = (int)std::max(1.0, std::min(10.0, std::floor(1.0e7 / maxAbs)));
    std::vector<long long> xs, ys;
    long long minX = 0, maxX = 0, minY = 0, maxY = 0;
    for (const Pt& p : points) {
        long long x = std::llround(p.x * k);
        long long y = std::llround(p.y * k);
        xs.push_back(x);
        ys.push_back(y);
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }
    shapeNode.setNum("p2", maxX - minX > 0 ? (double)(maxX - minX) / k : 0.0);
    shapeNode.setNum("p3", maxY - minY > 0 ? (double)(maxY - minY) / k : 0.0);
    Node v("v");
    v.setInt("id", -1);
    v.setInt("n", (long long)points.size());
    const char separator = _mobileUnderscore ? '_' : '.';
    for (size_t i = 0; i < points.size(); i++) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%lld%c%lld", xs[i], separator, ys[i]);
        v.set("v" + std::to_string(i), buffer);
    }
    shapeNode.children.clear();
    shapeNode.children.push_back(v);
}

namespace {

std::vector<Pt> quantized(const std::vector<Pt>& points) {
    // What the game will see (up to the k scale): integers after emitVerts' rounding, in 1/10 px.
    std::vector<Pt> out;
    out.reserve(points.size());
    for (const Pt& p : points) out.push_back(Pt(std::round(p.x * 10.0) / 10.0, std::round(p.y * 10.0) / 10.0));
    return out;
}

std::vector<Pt> cleanRing(std::vector<Pt> ring) {
    ring = quantized(ring);
    geom::removeDuplicates(ring, 1e-9);
    geom::removeCollinear(ring, 1e-6);
    return ring;
}

// Simplifies a closed art outline to the vertex limit, preferring a result FFDrawNode can fill.
std::vector<Pt> fitArtRing(const std::vector<Pt>& input, size_t maxCount) {
    std::vector<Pt> ring = cleanRing(input);
    if (ring.size() <= maxCount && geom::earClipSucceeds(ring)) return ring;
    std::vector<std::vector<Pt>> candidates;
    candidates.push_back(cleanRing(geom::simplifyClosedVW(ring, maxCount)));
    candidates.push_back(cleanRing(geom::simplifyClosedRDP(ring, maxCount)));
    for (size_t target : {maxCount * 3 / 4, maxCount / 2, maxCount / 4, (size_t)12, (size_t)8}) {
        if (target < 3 || target >= ring.size()) continue;
        candidates.push_back(cleanRing(geom::simplifyClosedVW(ring, target)));
        candidates.push_back(cleanRing(geom::simplifyClosedRDP(ring, target)));
    }
    for (const auto& candidate : candidates) {
        if (candidate.size() >= 3 && candidate.size() <= maxCount && geom::earClipSucceeds(candidate)) {
            return candidate;
        }
    }
    // Nothing the game can fill (self-intersecting outline): keep the closest one; FFDrawNode
    // just draws nothing for it, as it does for such outlines in shipped levels.
    if (ring.size() <= maxCount) return ring;
    return candidates.front().size() <= maxCount ? candidates.front()
                                                 : geom::simplifyClosedVW(ring, maxCount);
}

}  // namespace

bool Converter::buildPolygon(const XMLElement* e, int type, bool interactive, double width,
                             bool hasWidth, double height, bool hasHeight, ShapeRec& out,
                             bool* becameArt) {
    *becameArt = false;
    const XMLElement* v = e->FirstChildElement("v");
    if (!v) return false;
    int id = inum(v, "id", 0);
    std::map<int, std::vector<std::string>>& cache = type == 3 ? _polyCache : _artCache;
    auto cached = cache.find(id);
    std::vector<std::string> strings;
    if (cached != cache.end()) {
        strings = cached->second;
    } else {
        int n = inumClamped(v, "n", 0, 0, 20000);
        for (int i = 0; i < n; i++) {
            const char* s = v->Attribute(("v" + std::to_string(i)).c_str());
            strings.push_back(s ? s : "");
        }
        cache[id] = strings;
    }
    const bool closed = !attr(v, "f") || flag(v, "f", true);

    std::vector<Pt> points, handlesIn, handlesOut;
    for (const std::string& s : strings) {
        Pt p, hi, ho;
        if (!geom::parseVertex(s.c_str(), _dotVerts, &p, &hi, &ho)) continue;
        points.push_back(p);
        handlesIn.push_back(hi);
        handlesOut.push_back(ho);
    }
    if (points.size() <= 1) return false;  // Flash puts an empty dummy shape here

    // Flash EdgeShape: scale = shapeWidth / defaultWidth (unless 0), defaults from the vertices.
    double defaultWidth = geom::flashDefaultExtent(points, true);
    double defaultHeight = geom::flashDefaultExtent(points, false);
    // (PolygonShape/ArtShape clamp the resulting scale to 0.1..10.)
    double sx = (hasWidth && width != 0.0 && defaultWidth > 0.0) ? width / defaultWidth : 1.0;
    double sy = (hasHeight && height != 0.0 && defaultHeight > 0.0) ? height / defaultHeight : 1.0;
    if (!std::isfinite(sx)) sx = 1.0;
    if (!std::isfinite(sy)) sy = 1.0;
    sx = std::max(0.1, std::min(10.0, sx));
    sy = std::max(0.1, std::min(10.0, sy));
    for (size_t i = 0; i < points.size(); i++) {
        points[i] = Pt(coord(points[i].x * sx), coord(points[i].y * sy));
        handlesIn[i] = Pt(handlesIn[i].x * sx, handlesIn[i].y * sy);
        handlesOut[i] = Pt(handlesOut[i].x * sx, handlesOut[i].y * sy);
    }

    if (type == 3 && interactive) {
        // Physics polygon: Box2D takes at most 8 vertices (and their convex hull).
        std::vector<Pt> ring = cleanRing(points);
        if (ring.size() > kMaxPhysicsVerts) {
            ring = geom::convexHull(ring);
            if (ring.size() > kMaxPhysicsVerts) ring = cleanRing(geom::simplifyClosedVW(ring, kMaxPhysicsVerts));
        }
        std::vector<Pt> hull = geom::convexHull(ring);
        if (ring.size() >= 3 && hull.size() >= 3 && std::fabs(geom::signedArea(hull)) >= 1.0) {
            emitVerts(out.node, ring);
            out.area = std::fabs(geom::signedArea(ring));
            return true;
        }
        // Degenerate (a line or a dot): keep it as art without physics.
        *becameArt = true;
        type = 4;
    }

    std::vector<Pt> ring;
    if (closed) {
        ring = fitArtRing(geom::flattenArt(points, handlesIn, handlesOut, true), kMaxArtVerts);
    } else {
        // Open path: Flash strokes it with a hairline. Draw it as a thin filled ribbon.
        std::vector<Pt> line = geom::flattenArt(points, handlesIn, handlesOut, false);
        geom::removeDuplicates(line, 0.05);
        if (line.size() > kMaxArtVerts / 2) line = geom::simplifyOpenVW(line, kMaxArtVerts / 2);
        ring = geom::strokeOutline(line, 2.0);
        if (!geom::earClipSucceeds(cleanRing(ring))) {
            for (size_t target : {(size_t)24, (size_t)12, (size_t)6, (size_t)2}) {
                if (target >= line.size()) continue;
                std::vector<Pt> candidate = geom::strokeOutline(geom::simplifyOpenVW(line, target), 2.0);
                if (geom::earClipSucceeds(cleanRing(candidate))) {
                    ring = candidate;
                    break;
                }
            }
        }
        ring = cleanRing(ring);
        if (ring.size() > kMaxArtVerts) ring = geom::simplifyClosedVW(ring, kMaxArtVerts);
    }
    if (ring.size() < 3 || std::fabs(geom::signedArea(ring)) < 0.01) return false;  // nothing to draw
    emitVerts(out.node, ring);
    out.area = std::fabs(geom::signedArea(ring));
    return true;
}

bool Converter::convertShape(const XMLElement* e, bool inGroup, ShapeRec& out) {
    int type = inum(e, "t", 0);
    if (type < 0 || type > 4) {
        count("unknownShape");
        return false;
    }
    bool interactive = !(attr(e, "i") && !strcmp(attr(e, "i"), "f"));
    if (type == 3 && !interactive) type = 4;  // Flash loads these as art shapes
    if (type == 4) interactive = false;

    double x = coord(num(e, "p0", 0.0));
    double y = coord(num(e, "p1", 0.0));
    double width = 0.0, height = 0.0;
    bool hasWidth = readNum(e, "p2", &width);
    bool hasHeight = readNum(e, "p3", &height);
    double rotation = angleDeg(num(e, "p4", 0.0));
    bool immovable = flag(e, "p5", false);
    bool sleeping = flag(e, "p6", false);
    double density = numClamped(e, "p7", 1.0, 0.1, 100.0);  // RefShape.density clamp
    long long fill = inum(e, "p8", 4032711);
    long long outline = inum(e, "p9", -1);
    double opacity = numClamped(e, "p10", 100.0, 0.0, 100.0);
    int collision = inumClamped(e, "p11", 1, 0, 7);
    if (inGroup) immovable = false;  // group shapes live on the group body (Flash ignores p5 there)

    Node& n = out.node;
    n.setInt("t", type);
    n.setBool("i", interactive);
    n.setNum("p0", x);
    n.setNum("p1", y);
    switch (type) {
    case 0:
    case 1:
    case 2: {
        // Flash sizes are scales of a 100 px shape (triangles: 100 x 300), clamped by
        // RefShape.scaleX/Y to 0.05..50 (triangles ..15); so never negative or zero.
        const double baseHeight = type == 2 ? 300.0 : 100.0;
        const double maxScale = type == 2 ? 15.0 : 50.0;
        if (!hasWidth) width = 100.0;
        if (!hasHeight) height = baseHeight;
        if (type == 1) width = height = hasHeight ? height : width;  // setShapeHeight sets both, last
        width = std::max(100.0 * 0.05, std::min(100.0 * maxScale, width));
        height = std::max(baseHeight * 0.05, std::min(baseHeight * maxScale, height));
        if (type == 1) height = width;
        n.setNum("p2", width);
        n.setNum("p3", height);
        out.area = type == 1 ? kPi * width * width / 4.0 : std::fabs(width * height) / (type == 2 ? 2.0 : 1.0);
        break;
    }
    default: {
        bool becameArt = false;
        if (!buildPolygon(e, type, interactive, width, hasWidth, height, hasHeight, out, &becameArt)) {
            count("brokenShape");
            return false;
        }
        if (becameArt) {
            type = 4;
            interactive = false;
            n.setInt("t", 4);
            n.setBool("i", false);
        }
        break;
    }
    }
    bool visible = opacity > 0.0 && fill >= 0;
    n.setNum("p4", rotation);
    n.setBool("p5", immovable);
    n.setBool("p6", sleeping);
    n.setNum("p7", density);
    n.setInt("p8", fill >= 0 ? (fill & 0xffffff) : 0);
    n.setInt("p9", outline >= 0 ? (outline & 0xffffff) : -1);
    n.setNum("p10", visible ? opacity : 0.0);  // no fill in Flash: the mobile game draws no outlines
    n.setInt("p11", collision);
    // Keep the attribute order the shipped levels use (t i p0..p11, then the vertex list).
    std::vector<std::pair<std::string, std::string>> ordered;
    for (const char* key : {"t", "i", "p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9", "p10", "p11"}) {
        if (const std::string* value = n.get(key)) ordered.emplace_back(key, *value);
    }
    n.attrs.swap(ordered);

    out.valid = true;
    out.interactive = interactive;
    out.visible = visible;
    if (!interactive) {
        out.body = BodyKind::None;
    } else {
        out.body = immovable ? BodyKind::Level : BodyKind::Own;
    }
    return true;
}

ShapeRec Converter::makeBox(double x, double y, double w, double h, double angle, bool interactive,
                            bool immovable, bool sleeping, double density, int color,
                            double opacity, int collision, bool inGroup) {
    ShapeRec s;
    Node& n = s.node;
    n.setInt("t", 0);
    n.setBool("i", interactive);
    n.setNum("p0", coord(x));
    n.setNum("p1", coord(y));
    n.setNum("p2", std::max(kMinPhysicalSize, std::min(kMaxSize, std::fabs(w))));
    n.setNum("p3", std::max(kMinPhysicalSize, std::min(kMaxSize, std::fabs(h))));
    n.setNum("p4", angleDeg(angle));
    n.setBool("p5", immovable && !inGroup);
    n.setBool("p6", sleeping);
    n.setNum("p7", density);
    n.setInt("p8", color);
    n.setInt("p9", -1);
    n.setNum("p10", opacity);
    n.setInt("p11", collision);
    s.valid = true;
    s.interactive = interactive;
    s.visible = opacity > 0.0;
    s.area = std::fabs(w * h);
    s.body = !interactive ? BodyKind::None : (immovable && !inGroup ? BodyKind::Level : BodyKind::Own);
    return s;
}

ShapeRec Converter::makeCircle(double x, double y, double diameter, bool interactive, bool immovable,
                               bool sleeping, double density, int color, double opacity, bool inGroup) {
    ShapeRec s = makeBox(x, y, diameter, diameter, 0.0, interactive, immovable, sleeping, density,
                         color, opacity, 1, inGroup);
    s.node.setInt("t", 1);
    s.area = kPi * diameter * diameter / 4.0;
    return s;
}

// ---------------------------------------------------------------------------------------------
// Specials

// Supported ids: re-emits the parameters the mobile class reads, clamped like Flash's editor
// setters clamp them (and to the tables the mobile classes index). Returns false to drop.
bool Converter::sanitizeSupportedSpecial(const XMLElement* e, int type, bool inGroup, Node& node,
                                         bool* jointable, BodyKind* jointBody,
                                         bool* addsGroupFixture) {
    node = Node("sp");
    node.setInt("t", type);
    node.setNum("p0", coord(num(e, "p0", 0.0)));
    node.setNum("p1", coord(num(e, "p1", 0.0)));
    *jointable = false;
    *jointBody = BodyKind::Own;
    *addsGroupFixture = false;
    auto angle = [&](const char* key) { node.setNum(key, angleDeg(num(e, key, 0.0))); };
    switch (type) {
    case 0: {  // Van: p2 angle, p3 sleeping, p4 interactive
        angle("p2");
        node.setBool("p3", flag(e, "p3", false));
        bool interactive = flag(e, "p4", true);
        node.setBool("p4", interactive);
        *jointable = interactive;
        break;
    }
    case 2:  // Mine
    case 8:  // Fan
        angle("p2");
        break;
    case 3:    // IBeam: p2 w, p3 h, p4 angle, p5 fixed, p6 sleeping
    case 4: {  // Log: same
        double w = std::fabs(num(e, "p2", type == 3 ? 300.0 : 200.0));
        double h = std::fabs(num(e, "p3", type == 3 ? 30.0 : 40.0));
        node.setNum("p2", std::max(2.0, std::min(kMaxSize, w)));
        node.setNum("p3", std::max(2.0, std::min(kMaxSize, h)));
        angle("p4");
        bool fixed = flag(e, "p5", false) && !inGroup;  // Flash: grouped beams can't be fixed
        node.setBool("p5", fixed);
        node.setBool("p6", flag(e, "p6", false));
        *jointable = !fixed;
        *addsGroupFixture = type == 3;
        break;
    }
    case 5:  // SpringBox: p3 delay (s)
        angle("p2");
        node.setNum("p3", numClamped(e, "p3", 0.0, 0.0, 2.0));
        break;
    case 6: {  // Spikes: p3 fixed, p4 count, p5 sleeping
        angle("p2");
        bool fixed = flag(e, "p3", false) && !inGroup;
        node.setBool("p3", fixed);
        node.setInt("p4", inumClamped(e, "p4", 20, 20, 150));
        node.setBool("p5", flag(e, "p5", false));
        *jointable = true;
        *jointBody = fixed ? BodyKind::Level : BodyKind::Own;
        *addsGroupFixture = true;
        break;
    }
    case 7:  // WreckingBall: p2 rope length (px)
        node.setNum("p2", numClamped(e, "p2", 350.0, 200.0, 1000.0));
        break;
    case 9:   // FinishLine
    case 10:  // SoccerBall
        break;
    case 12:  // BoostPanel: p3 panels, p4 power
        angle("p2");
        node.setInt("p3", inumClamped(e, "p3", 2, 1, 6));
        node.setInt("p4", inumClamped(e, "p4", 20, 10, 100));
        break;
    case 15:  // HarpoonGun: p3 anchor, p4 fixed turret, p5 turret angle, p6 trigger firing, p7 off
        angle("p2");
        node.setBool("p3", flag(e, "p3", true));
        node.setBool("p4", flag(e, "p4", false));
        node.setNum("p5", numClamped(e, "p5", 0.0, -110.0, 110.0));
        node.setBool("p6", flag(e, "p6", false));
        node.setBool("p7", flag(e, "p7", false));
        break;
    case 20: {  // Bottle: p3 colour 1..4, p4 sleeping, p5 interactive
        angle("p2");
        node.setInt("p3", inumClamped(e, "p3", 1, 1, 4));
        node.setBool("p4", flag(e, "p4", false));
        bool interactive = flag(e, "p5", true);
        node.setBool("p5", interactive);
        *jointable = interactive;
        break;
    }
    case 23:  // Sign: p3 type 1..13, p4 post
        angle("p2");
        node.setInt("p3", inumClamped(e, "p3", 1, 1, 13));
        node.setBool("p4", flag(e, "p4", true));
        break;
    case 25:  // HomingMine: p2 seek speed 1..10, p3 delay 0..5
        node.setInt("p2", inumClamped(e, "p2", 1, 1, 10));
        node.setInt("p3", inumClamped(e, "p3", 0, 0, 5));
        break;
    case 28:  // Jet: p3 sleeping, p4 power 1..10, p5 fire time 0..50, p6 accel 0..5, p7 fixed rot
        angle("p2");
        node.setBool("p3", flag(e, "p3", false));
        node.setInt("p4", inumClamped(e, "p4", 1, 1, 10));
        node.setInt("p5", inumClamped(e, "p5", 0, 0, 50));
        node.setInt("p6", inumClamped(e, "p6", 0, 0, 5));
        node.setBool("p7", flag(e, "p7", false));
        *jointable = true;
        break;
    case 29: {  // ArrowGun: p3 fixed, p4 rate of fire, p5 don't shoot player
        angle("p2");
        bool fixed = flag(e, "p3", true) && !inGroup;  // fixed guns in groups corrupt Box2D
        node.setBool("p3", fixed);
        node.setInt("p4", inumClamped(e, "p4", 5, 1, 10));
        node.setBool("p5", flag(e, "p5", false));
        *jointable = !fixed;
        *addsGroupFixture = true;
        break;
    }
    case 31:  // Token (a stub on Android 1.1.3)
        angle("p2");
        break;
    case 34: {  // BladeWeapon: p3 flipped, p4 sleeping, p5 interactive, p6 type 1..12
        angle("p2");
        node.setBool("p3", flag(e, "p3", false));
        node.setBool("p4", flag(e, "p4", false));
        bool interactive = flag(e, "p5", true);
        node.setBool("p5", interactive);
        node.setInt("p6", inumClamped(e, "p6", 1, 1, 12));
        *jointable = interactive;
        *addsGroupFixture = interactive;
        break;
    }
    default:
        return false;
    }
    if (!*jointable) *jointBody = BodyKind::None;
    return true;
}

// Placeholder shape with the item's footprint for unsupported specials that shape the level.
// Positions are the special's (local in groups, as group shapes are). Returns false: drop.
bool Converter::placeholderFor(const XMLElement* e, int type, bool inGroup, ShapeRec* shape) {
    const double x = coord(num(e, "p0", 0.0));
    const double y = coord(num(e, "p1", 0.0));
    auto offsetBox = [&](double angle, double ox, double oy, double w, double h, bool interactive,
                         bool immovable, bool sleeping, double density, int color, double opacity) {
        double r = angle * kPi / 180.0;
        double cx = x + ox * std::cos(r) - oy * std::sin(r);
        double cy = y + ox * std::sin(r) + oy * std::cos(r);
        *shape = makeBox(cx, cy, w, h, angle, interactive, immovable, sleeping, density, color,
                         opacity, 1, inGroup);
    };
    switch (type) {
    case 0: {  // Van inside a group (the mobile van ignores groups)
        double a = angleDeg(num(e, "p2", 0.0));
        offsetBox(a, 0, 0, 132, 116, flag(e, "p4", true), false, flag(e, "p3", false), 3.0, 0xd8d8d0, 100);
        return true;
    }
    case 20: {  // Bottle inside a group
        double a = angleDeg(num(e, "p2", 0.0));
        offsetBox(a, 0, 0, 10, 29, flag(e, "p5", true), false, flag(e, "p4", false), 1.0, 0x3f7f3f, 80);
        return true;
    }
    case 1: {  // Table: p2 angle, p3 sleeping, p4 interactive
        double a = angleDeg(num(e, "p2", 0.0));
        bool interactive = flag(e, "p4", true) || _version < 1.67;
        offsetBox(a, 0, 22, 156, 56, interactive, false, flag(e, "p3", false), 5.0, 0x8b5a2b, 100);
        return true;
    }
    case 11: {  // Meteor: p2 diameter, p4 fixed, p5 sleeping
        double d = numClamped(e, "p2", 400.0, 10.0, 2000.0);
        *shape = makeCircle(x, y, d, true, flag(e, "p4", false), flag(e, "p5", false), 75.0, 0x5a4636, 100, inGroup);
        return true;
    }
    case 13:
    case 14: {  // Buildings: p2 floors wide (x300 px), p3 floors (x165 px + 100 roof), top-left x/y
        double w = inumClamped(e, "p2", 1, 1, 20) * 300.0;
        double h = inumClamped(e, "p3", 3, 1, 60) * 165.0 + 100.0;
        *shape = makeBox(x + w * 0.5, y + h * 0.5, w, h, 0.0, true, true, false, 1.0,
                         type == 13 ? 0x6f6f78 : 0x8c7b6b, 100, 1, inGroup);
        return true;
    }
    case 18: {  // Glass: p2 w, p3 h, p4 angle -- no physics: the player would smash through it
        double w = numClamped(e, "p2", 10.0, 1.0, kMaxSize);
        double h = numClamped(e, "p3", 100.0, 1.0, kMaxSize);
        *shape = makeBox(x, y, w, h, angleDeg(num(e, "p4", 0.0)), false, false, false, 1.0, 0xbfe6ff, 45, 1, inGroup);
        return true;
    }
    case 19: {  // Chair: p2 angle, p3 reverse, p4 sleeping, p5 interactive
        double a = angleDeg(num(e, "p2", 0.0));
        offsetBox(a, 0, -12, 42, 88, flag(e, "p5", true), false, flag(e, "p4", false), 3.0, 0x8b5a2b, 100);
        return true;
    }
    case 21:    // TV: p2 angle, p3 sleeping, p4 interactive
    case 22: {  // Boombox
        double a = angleDeg(num(e, "p2", 0.0));
        offsetBox(a, 0, 0, type == 21 ? 55 : 48, type == 21 ? 40 : 28, flag(e, "p4", true), false,
                  flag(e, "p3", false), 3.0, 0x303030, 100);
        return true;
    }
    case 24: {  // Toilet: p2 angle, p3 reverse, p4 sleeping, p5 interactive
        double a = angleDeg(num(e, "p2", 0.0));
        offsetBox(a, 0, 0, 62, 78, flag(e, "p5", true), false, flag(e, "p4", false), 3.0, 0xf0f0f0, 100);
        return true;
    }
    case 26: {  // TrashCan: p2 angle, p3 sleeping, p4 interactive
        double a = angleDeg(num(e, "p2", 0.0));
        offsetBox(a, 0, 0, 46, 64, flag(e, "p4", true), false, flag(e, "p3", false), 3.0, 0x56645a, 100);
        return true;
    }
    case 27: {  // Rail: p2 length, p4 angle (static)
        double w = numClamped(e, "p2", 250.0, 10.0, kMaxSize);
        *shape = makeBox(x, y, w, 16, angleDeg(num(e, "p4", 0.0)), true, true, false, 1.0, 0x9a9a9a, 100, 1, inGroup);
        return true;
    }
    case 32: {  // FoodItem: p3 sleeping, p4 interactive
        *shape = makeCircle(x, y, 50, flag(e, "p4", true), false, flag(e, "p3", false), 2.0, 0x6aa84f, 100, inGroup);
        return true;
    }
    case 35: {  // Paddle: its static base
        double a = angleDeg(num(e, "p2", 0.0));
        offsetBox(a, 0, 0, 350, 40, true, true, false, 1.0, 0x707070, 100);
        return true;
    }
    default:
        return false;
    }
}

// Flash Chain (userspecials/Chain): linkCount boxes 9*s px apart along an arc, pinned to each
// other. The Android build only has a stub, so the chain is rebuilt from shapes and joints.
void Converter::buildChain(const XMLElement* e, SpecialRec& rec) {
    const double x = coord(num(e, "p0", 0.0));
    const double y = coord(num(e, "p1", 0.0));
    const double rotation = angleDeg(num(e, "p2", 0.0)) * kPi / 180.0;
    const bool sleeping = flag(e, "p3", false);
    const bool interactive = flag(e, "p4", true);
    const int links = inumClamped(e, "p5", 20, 2, 40);
    const double linkScale = numClamped(e, "p6", 1.0, 1.0, 10.0);
    const double linkAngle = numClamped(e, "p7", 0.0, -10.0, 10.0);
    const double s = 1.0 + (linkScale - 1.0) / 9.0 * 2.0;
    const double step = (15.0 - 3.0 * 2.0) * s;
    const double turn = -(linkAngle / 10.0 * (360.0 / (links * 2))) * kPi / 180.0;
    const double c = std::cos(rotation), sn = std::sin(rotation);
    auto world = [&](double lx, double ly) { return Pt(x + lx * c - ly * sn, y + lx * sn + ly * c); };

    Pt point(0, 0);
    std::vector<Pt> joints;
    for (int i = 0; i < links; i++) {
        double a = kPi / 2.0 + ((i + 1) * turn - turn * 0.5);
        double dx = std::cos(a) * step;
        double dy = std::sin(a) * step;
        Pt center = world(point.x + dx * 0.5, point.y + dy * 0.5);
        double bodyAngle = (rotation + a - kPi / 2.0) * 180.0 / kPi;
        ShapeRec link = makeBox(center.x, center.y, 6.0 * s, 15.0 * s, bodyAngle, interactive, false,
                                sleeping, 17.0, 0x4a4a4a, 100, 1, false);
        rec.chain.linkShapes.push_back((int)_extra.size());
        rec.chain.linkCenters.push_back(center);
        _extra.push_back(link);
        point = Pt(point.x + dx, point.y + dy);
        if (i + 1 < links) joints.push_back(world(point.x, point.y));
    }
    if (!interactive) return;
    for (size_t i = 0; i < joints.size(); i++) {
        JointRec j;
        j.keep = true;
        j.node.setInt("t", 0);
        j.node.setNum("x", joints[i].x);
        j.node.setNum("y", joints[i].y);
        j.b1.kind = Ref::Shape;
        j.b1.index = -2 - rec.chain.linkShapes[i];  // extra shape (resolved in assignIndices)
        j.b2.kind = Ref::Shape;
        j.b2.index = -2 - rec.chain.linkShapes[i + 1];
        _extraJoints.push_back(j);
    }
}

void Converter::convertSpecial(const XMLElement* e, int index) {
    SpecialRec rec;
    int type = inum(e, "t", -1);
    rec.type = type;
    bool addsGroupFixture = false;
    if (sanitizeSupportedSpecial(e, type, false, rec.node, &rec.jointable, &rec.jointBody, &addsGroupFixture)) {
        rec.fate = SpecialFate::Keep;
    } else if (type == 30) {
        rec.fate = SpecialFate::Chain;
        buildChain(e, rec);
        count("chain");
    } else {
        ShapeRec shape;
        if (type != 0 && type != 20 && placeholderFor(e, type, false, &shape)) {
            rec.fate = SpecialFate::Placeholder;
            rec.placeholder = (int)_extra.size();
            _extra.push_back(shape);
            count("placeholder:" + std::to_string(type));
        } else {
            rec.fate = SpecialFate::Drop;
            if (type >= 0 && type <= 35) {
                count("dropped:" + std::to_string(type));
            } else {
                _unknownSpecialIds.insert(type);
                count("unknownSpecial");
            }
        }
    }
    (void)index;
    _specials.push_back(rec);
}

// ---------------------------------------------------------------------------------------------
// Groups

void Converter::convertGroup(const XMLElement* e) {
    GroupRec g;
    Node& n = g.node;
    n.setNum("x", coord(num(e, "x", 0.0)));
    n.setNum("y", coord(num(e, "y", 0.0)));
    n.setNum("r", angleDeg(num(e, "r", 0.0)));
    n.setNum("ox", coord(num(e, "ox", 0.0)));
    n.setNum("oy", coord(num(e, "oy", 0.0)));
    n.setBool("s", flag(e, "s", false));
    g.foreground = flag(e, "f", false);
    n.setBool("f", g.foreground);
    n.setNum("o", numClamped(e, "o", 100.0, 0.0, 100.0));  // mobile default would be 0 (invisible)
    n.setBool("im", flag(e, "im", false));
    n.setBool("fr", flag(e, "fr", false));
    if (attr(e, "v") && flag(e, "v", false)) {
        _report.hasUserVehicle = true;
        count("vehicle");
    }

    for (const XMLElement* s = e->FirstChildElement("sh"); s; s = s->NextSiblingElement("sh")) {
        ShapeRec shape;
        if (!convertShape(s, true, shape)) continue;
        if (shape.interactive) g.hasBody = true;
        g.shapes.push_back(shape);
    }
    for (const XMLElement* s = e->FirstChildElement("sp"); s; s = s->NextSiblingElement("sp")) {
        int type = inum(s, "t", -1);
        Node special;
        bool jointable = false, addsFixture = false;
        BodyKind body;
        // Group-aware mobile specials; the others are drawn at their raw (local) position.
        bool groupAware = type == 3 || type == 6 || type == 23 || type == 29 || type == 34;
        if (groupAware && sanitizeSupportedSpecial(s, type, true, special, &jointable, &body, &addsFixture)) {
            if (addsFixture) g.hasBody = true;
            g.specials.push_back(special);
            continue;
        }
        ShapeRec shape;
        if (placeholderFor(s, type, true, &shape)) {
            if (shape.interactive) g.hasBody = true;
            g.shapes.push_back(shape);
            count("placeholder:" + std::to_string(type));
            continue;
        }
        if (type >= 0 && type <= 35) {
            count("dropped:" + std::to_string(type));
        } else {
            _unknownSpecialIds.insert(type);
            count("unknownSpecial");
        }
    }
    _groups.push_back(g);
}

// ---------------------------------------------------------------------------------------------
// Joints

Ref Converter::parseBodyRef(const char* text) {
    Ref ref;
    if (!text || !*text) {
        ref.kind = Ref::Level;  // Flash: an absent body is the level
        return ref;
    }
    char kind = text[0];
    const char* digits = (kind == 's' || kind == 'g') ? text + 1 : text;
    char* end = nullptr;
    long value = strtol(digits, &end, 10);
    if (end == digits || *end != '\0') return ref;  // garbage: Invalid
    if (kind == 's') {
        ref.kind = value >= 0 ? Ref::Special : Ref::Invalid;
    } else if (kind == 'g') {
        ref.kind = value >= 0 ? Ref::Group : Ref::Invalid;
    } else {
        ref.kind = value < 0 ? Ref::Level : Ref::Shape;
    }
    ref.index = (int)value;
    return ref;
}

void Converter::convertJoints(const XMLElement* joints) {
    if (!joints) return;
    for (const XMLElement* e = joints->FirstChildElement("j"); e; e = e->NextSiblingElement("j")) {
        JointRec j;
        int type = inum(e, "t", -1);
        if (type != 0 && type != 1) {
            _joints.push_back(j);  // Flash skips these too (keep the slot for index mapping)
            count("droppedJoint");
            continue;
        }
        j.keep = true;
        j.prismatic = type == 1;
        if (flag(e, "v", false)) _report.hasUserVehicle = true;
        Node& n = j.node;
        n.setInt("t", type);
        n.setNum("x", coord(num(e, "x", 0.0)));
        n.setNum("y", coord(num(e, "y", 0.0)));
        j.b1 = parseBodyRef(attr(e, "b1"));
        j.b2 = parseBodyRef(attr(e, "b2"));
        n.setBool("l", flag(e, "l", false));
        n.setBool("m", flag(e, "m", false));
        n.setBool("c", flag(e, "c", false));
        n.setNum("sp", numClamped(e, "sp", 0.0, -1.0e6, 1.0e6));
        if (type == 0) {
            double upper = numClamped(e, "ua", 0.0, -1.0e5, 1.0e5);
            double lower = numClamped(e, "la", 0.0, -1.0e5, 1.0e5);
            if (lower > upper) std::swap(lower, upper);
            n.setNum("ua", upper);
            n.setNum("la", lower);
            n.setNum("tq", numClamped(e, "tq", 0.0, 0.0, 1.0e9));
        } else {
            n.setNum("a", angleDeg(num(e, "a", 0.0)));
            double upper = numClamped(e, "ul", 0.0, -kMaxSize, kMaxSize);
            double lower = numClamped(e, "ll", 0.0, -kMaxSize, kMaxSize);
            if (lower > upper) std::swap(lower, upper);
            n.setNum("ul", upper);
            n.setNum("ll", lower);
            n.setNum("fo", numClamped(e, "fo", 0.0, 0.0, 1.0e9));
        }
        _joints.push_back(j);
    }
}

// ---------------------------------------------------------------------------------------------
// Triggers (parsed first, emitted once all indices are final)

void Converter::parseTriggers(const XMLElement* triggers) {
    if (!triggers) return;
    for (const XMLElement* e = triggers->FirstChildElement("t"); e; e = e->NextSiblingElement("t")) {
        TriggerRec t;
        Node& n = t.node;
        n.setNum("x", coord(num(e, "x", 0.0)));
        n.setNum("y", coord(num(e, "y", 0.0)));
        n.setNum("w", std::min(kMaxSize, std::fabs(num(e, "w", 100.0))));
        n.setNum("h", std::min(kMaxSize, std::fabs(num(e, "h", 100.0))));
        n.setNum("a", angleDeg(num(e, "a", 0.0)));
        t.triggeredBy = inum(e, "b", 1);
        if (t.triggeredBy < 1 || t.triggeredBy > 5) t.triggeredBy = 5;  // only fired by triggers
        t.type = inum(e, "t", 1);
        t.repeat = inum(e, "r", 1);
        if (t.repeat < 1 || t.repeat > 4) t.repeat = 1;
        n.setInt("b", t.triggeredBy);
        n.setInt("r", t.repeat);
        n.setBool("sd", flag(e, "sd", false));
        if (t.repeat > 2) n.setNum("i", numClamped(e, "i", 1.0, 0.0, 1.0e6));
        t.delay = numClamped(e, "d", 0.0, 0.0, 1.0e6);
        if (t.type == 2) {
            int sound = inum(e, "s", -1);
            if (sound < 0 || sound >= kSoundCount) {
                t.type = 0;  // a sound this game doesn't have: the trigger does nothing
                count("badSound");
            } else {
                n.setInt("s", sound);
                n.setInt("l", inum(e, "l", 1));
                n.setNum("v", numClamped(e, "v", 1.0, 0.0, 100.0));
                n.setNum("p", numClamped(e, "p", 0.0, -100.0, 100.0));
            }
        } else if (t.type != 1 && t.type != 3) {
            t.type = 0;
            count("badTrigger");
        }
        n.setInt("t", t.type);

        const bool hasTargets = t.type == 1 || t.triggeredBy == 4;
        if (hasTargets) {
            for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement()) {
                TargetRec target;
                std::string name = c->Value() ? c->Value() : "";
                if (name != "sh" && name != "sp" && name != "g" && name != "j") name = "t";  // as Flash
                target.kind = name;
                target.index = inum(c, "i", -1);
                if (t.type == 1) {
                    auto readProps = [](const XMLElement* from) {
                        std::vector<double> props;
                        for (int p = 0; p < 8; p++) {
                            double value;
                            props.push_back(readNum(from, ("p" + std::to_string(p)).c_str(), &value) ? value : NAN);
                        }
                        return props;
                    };
                    const XMLElement* firstAction = c->FirstChildElement("a");
                    if (firstAction || _version >= 1.87) {
                        for (const XMLElement* a = firstAction; a; a = a->NextSiblingElement("a")) {
                            Action action;
                            action.a = inum(a, "i", 0);
                            action.props = readProps(a);
                            target.actions.push_back(action);
                        }
                        if (!firstAction && attr(c, "a")) {  // old-style inline action anyway
                            Action action;
                            action.a = inum(c, "a", 0);
                            action.props = readProps(c);
                            target.actions.push_back(action);
                        }
                    } else {
                        Action action;
                        action.a = inum(c, "a", 0);  // Flash: int(@a), 0 when absent
                        action.props = readProps(c);
                        target.actions.push_back(action);
                    }
                }
                t.targets.push_back(target);
            }
        }
        _triggers.push_back(t);
    }
}

void Converter::markReferences() {
    auto markShape = [&](int index) {
        if (index >= 0 && index < (int)_shapes.size()) _shapes[index].referenced = true;
    };
    for (const JointRec& j : _joints) {
        if (!j.keep) continue;
        if (j.b1.kind == Ref::Shape) markShape(j.b1.index);
        if (j.b2.kind == Ref::Shape) markShape(j.b2.index);
    }
    for (const TriggerRec& t : _triggers) {
        for (const TargetRec& target : t.targets) {
            if (target.kind == "sh") markShape(target.index);
        }
    }
}

// FFDrawNode keeps at most 1600 shapes per layer (top-level shapes and background groups share
// one, foreground groups have the other). Drop decoration nobody references, the least visible
// first; only if that is not enough, physical shapes too.
void Converter::enforceDrawBudget() {
    struct Candidate {
        double score;
        int group;   // -1: top-level shape
        int index;
        bool physical;
    };
    for (int layer = 0; layer < 2; layer++) {
        const bool foreground = layer == 1;
        int total = 0;
        std::vector<Candidate> candidates;
        if (!foreground) {
            for (size_t i = 0; i < _shapes.size(); i++) {
                ShapeRec& s = _shapes[i];
                if (!s.valid || s.dropped) continue;
                if (!s.visible && !s.interactive && !s.referenced) {
                    s.dropped = true;  // invisible and inert: costs a draw slot, does nothing
                    count("invisible");
                    continue;
                }
                total++;
                if (!s.referenced) candidates.push_back({s.area, -1, (int)i, s.interactive});
            }
            total += (int)_extra.size();
        }
        for (size_t gi = 0; gi < _groups.size(); gi++) {
            GroupRec& g = _groups[gi];
            if (g.foreground != foreground) continue;
            for (size_t i = 0; i < g.shapes.size(); i++) {
                ShapeRec& s = g.shapes[i];
                if (s.dropped) continue;
                if (!s.visible && !s.interactive) {
                    s.dropped = true;
                    count("invisible");
                    continue;
                }
                total++;
                candidates.push_back({s.area, (int)gi, (int)i, s.interactive});
            }
        }
        if (total <= kDrawDelegateLimit) continue;
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            if (a.physical != b.physical) return !a.physical;
            return a.score < b.score;
        });
        int removedArt = 0, removedPhysical = 0;
        for (const Candidate& c : candidates) {
            if (total <= kDrawDelegateLimit) break;
            ShapeRec& s = c.group < 0 ? _shapes[c.index] : _groups[c.group].shapes[c.index];
            s.dropped = true;
            total--;
            (c.physical ? removedPhysical : removedArt)++;
        }
        if (removedArt) warn(plural(removedArt, "small decoration shape") + " left out (too many shapes)");
        if (removedPhysical) warn(plural(removedPhysical, "solid shape") + " left out (too many shapes)");
    }
    for (GroupRec& g : _groups) {
        bool body = false;
        for (const ShapeRec& s : g.shapes) {
            if (!s.dropped && s.interactive) body = true;
        }
        // Specials that put fixtures on the group body were accounted for in convertGroup;
        // only shapes can be dropped here.
        if (!body) {
            for (const Node& sp : g.specials) {
                const std::string* t = sp.get("t");
                int type = t ? atoi(t->c_str()) : -1;
                const std::string* p5 = sp.get("p5");
                if (type == 3 || type == 6 || type == 29 || (type == 34 && p5 && *p5 == "t")) body = true;
            }
        }
        g.hasBody = body;
    }
}

void Converter::assignIndices() {
    int next = 0;
    for (ShapeRec& s : _shapes) {
        if (s.valid && !s.dropped) s.outIndex = next++;
    }
    for (ShapeRec& s : _extra) s.outIndex = next++;
    next = 0;
    for (SpecialRec& s : _specials) {
        if (s.fate == SpecialFate::Keep) s.outIndex = next++;
    }
    next = 0;
    for (GroupRec& g : _groups) g.outIndex = next++;
}

// Mobile body reference for a joint end. identity: something that tells two ends on the same
// body apart (-1 level, shapes >= 0, specials 1e6+, groups 2e6+).
bool Converter::resolveJointBody(const Ref& ref, const Pt& anchor, std::string* out, int* identity) {
    switch (ref.kind) {
    case Ref::Level:
        *out = "-1";
        *identity = -1;
        return true;
    case Ref::Shape: {
        const ShapeRec* s = nullptr;
        if (ref.index <= -2) {
            int extra = -2 - ref.index;
            if (extra < (int)_extra.size()) s = &_extra[extra];
        } else if (ref.index >= 0 && ref.index < (int)_shapes.size()) {
            s = &_shapes[ref.index];
        }
        if (!s || !s->valid || s->dropped || s->outIndex < 0 || s->body == BodyKind::None) return false;
        if (s->body == BodyKind::Level) {
            *out = std::to_string(s->outIndex);
            *identity = -1;
            return true;
        }
        *out = std::to_string(s->outIndex);
        *identity = s->outIndex;
        return true;
    }
    case Ref::Special: {
        if (ref.index < 0 || ref.index >= (int)_specials.size()) return false;
        const SpecialRec& sp = _specials[ref.index];
        if (sp.fate == SpecialFate::Keep) {
            if (!sp.jointable || sp.jointBody == BodyKind::None) return false;
            *out = "s" + std::to_string(sp.outIndex);
            *identity = sp.jointBody == BodyKind::Level ? -1 : 1000000 + sp.outIndex;
            return true;
        }
        if (sp.fate == SpecialFate::Placeholder) {
            Ref shape;
            shape.kind = Ref::Shape;
            shape.index = -2 - sp.placeholder;
            return resolveJointBody(shape, anchor, out, identity);
        }
        if (sp.fate == SpecialFate::Chain && !sp.chain.linkShapes.empty()) {
            size_t best = 0;
            double bestDistance = 1e300;
            for (size_t i = 0; i < sp.chain.linkCenters.size(); i++) {
                double d = std::hypot(sp.chain.linkCenters[i].x - anchor.x, sp.chain.linkCenters[i].y - anchor.y);
                if (d < bestDistance) {
                    bestDistance = d;
                    best = i;
                }
            }
            Ref shape;
            shape.kind = Ref::Shape;
            shape.index = -2 - sp.chain.linkShapes[best];
            return resolveJointBody(shape, anchor, out, identity);
        }
        return false;
    }
    case Ref::Group: {
        if (ref.index < 0 || ref.index >= (int)_groups.size()) return false;
        const GroupRec& g = _groups[ref.index];
        if (!g.hasBody) return false;
        *out = "g" + std::to_string(g.outIndex);
        *identity = 2000000 + g.outIndex;
        return true;
    }
    default:
        return false;
    }
}

// ---------------------------------------------------------------------------------------------
// Trigger targets

namespace {

int requiredProps(const std::string& kind, int action) {
    if (kind == "sh") return action == 3 ? 2 : action == 4 ? 3 : action == 7 ? 1 : 0;
    if (kind == "g") return action == 1 ? 2 : action == 2 ? 3 : action == 7 ? 1 : 0;
    if (kind == "j") return (action == 1 || action == 4) ? 2 : 0;
    if (kind == "sp") return action == 1 ? 3 : 0;
    return 0;
}

double propOr(const Action& action, size_t i, double def) {
    return i < action.props.size() && std::isfinite(action.props[i]) ? action.props[i] : def;
}

}  // namespace

std::vector<Node> Converter::emitTargets(const TriggerRec& trigger, int triggerIndex) {
    (void)triggerIndex;
    std::vector<Node> out;
    const bool runs = trigger.type == 1;
    const bool bodies = trigger.triggeredBy == 4;
    for (const TargetRec& target : trigger.targets) {
        std::string kind = target.kind;
        int index = -1;
        // Shape-like special stand-ins accept the shape actions that match the Flash ones.
        std::vector<Action> actions = target.actions;
        int specialType = -1;
        if (kind == "sh") {
            if (target.index < 0 || target.index >= (int)_shapes.size()) continue;
            const ShapeRec& s = _shapes[target.index];
            if (!s.valid || s.dropped) continue;
            index = s.outIndex;
        } else if (kind == "g") {
            if (target.index < 0 || target.index >= (int)_groups.size()) continue;
            index = _groups[target.index].outIndex;
        } else if (kind == "j") {
            if (target.index < 0 || target.index >= (int)_joints.size()) continue;
            const JointRec& j = _joints[target.index];
            if (!j.keep || j.outIndex < 0) continue;
            index = j.outIndex;
        } else if (kind == "t") {
            if (target.index < 0 || target.index >= (int)_triggers.size()) continue;
            index = target.index;
        } else if (kind == "sp") {
            if (target.index < 0 || target.index >= (int)_specials.size()) continue;
            const SpecialRec& sp = _specials[target.index];
            if (sp.fate == SpecialFate::Keep) {
                index = sp.outIndex;
                specialType = sp.type;
            } else if (sp.fate == SpecialFate::Placeholder || sp.fate == SpecialFate::Chain) {
                // Map Flash's special actions onto shape actions of the stand-in shape(s).
                std::vector<int> shapes;
                if (sp.fate == SpecialFate::Placeholder) {
                    shapes.push_back(sp.placeholder);
                } else {
                    shapes = sp.chain.linkShapes;
                }
                std::vector<Action> mapped;
                for (const Action& a : actions) {
                    Action m;
                    m.a = -2;
                    if (sp.type == 18) {  // glass: shatter, wake, impulse
                        if (a.a == 0) m.a = 6;
                        else if (a.a == 1) m.a = 0;
                        else if (a.a == 2) { m.a = 4; m.props = a.props; }
                    } else if (a.a == 0) {
                        m.a = 0;
                    } else if (a.a == 1) {
                        m.a = 4;
                        m.props = a.props;
                        if (sp.fate == SpecialFate::Chain) m.props.insert(m.props.begin() + std::min<size_t>(2, m.props.size()), 0.0);
                    }
                    if (m.a != -2) mapped.push_back(m);
                }
                if (runs && mapped.empty() && !bodies) continue;
                for (size_t k = 0; k < shapes.size(); k++) {
                    const ShapeRec& s = _extra[shapes[k]];
                    std::vector<Action> per = mapped;
                    // An impulse goes to one link of a chain, not to every link.
                    if (sp.fate == SpecialFate::Chain && k != shapes.size() / 2) {
                        per.erase(std::remove_if(per.begin(), per.end(), [](const Action& a) { return a.a == 4; }), per.end());
                    }
                    if (per.empty() && !bodies) continue;
                    if (per.empty()) per.push_back(Action());
                    for (const Action& a : per) {
                        Node node("sh");
                        node.setInt("i", s.outIndex);
                        if (a.a >= 0) node.setInt("a", a.a);
                        int need = requiredProps("sh", a.a);
                        for (int p = 0; p < need; p++) node.setNum("p" + std::to_string(p), propOr(a, p, 0.0));
                        out.push_back(node);
                    }
                }
                continue;
            } else {
                continue;
            }
        } else {
            continue;
        }
        if (index < 0) continue;
        if ((kind == "sh" || kind == "j" || kind == "g") && index >= kMaxTargetIndex) continue;

        if (!runs) {
            // Only activation bodies (b=4 with a sound/finish trigger): actions never run.
            if (!bodies) continue;
            if (kind == "sp" && (specialType == 8 || specialType == 12)) continue;  // would switch them off
            Node node(kind);
            node.setInt("i", index);
            out.push_back(node);
            continue;
        }
        if (actions.empty()) {
            if (!bodies || kind == "t" || kind == "j") continue;
            // Activation body without an action: give it one the mobile code ignores.
            if (kind == "sp") {
                if (specialType != 0 && specialType != 3 && specialType != 4 && specialType != 20 && specialType != 34) continue;
                Node node(kind);
                node.setInt("i", index);
                node.setInt("a", 2);
                out.push_back(node);
            } else {
                Node node(kind);
                node.setInt("i", index);
                node.setInt("a", -1);
                out.push_back(node);
            }
            continue;
        }
        for (const Action& action : actions) {
            int a = action.a;
            if (a < 0) continue;
            bool supported;
            if (kind == "sh" || kind == "g") supported = a <= 7;
            else if (kind == "j") supported = a <= 4;
            else if (kind == "t") supported = a <= 2;
            else supported = a <= 2;  // specials: 0 wake/fire/..., 1 impulse/disable, 2 enable
            if (!supported) {
                count("badAction");
                continue;
            }
            if (kind == "sp" && a == 3) continue;
            Node node(kind);
            node.setInt("i", index);
            node.setInt("a", a);
            int need = requiredProps(kind, a);
            for (int p = 0; p < need; p++) {
                double value = propOr(action, p, 0.0);
                if ((kind == "sh" && a == 3) || (kind == "g" && a == 1)) {
                    if (p == 0) value = std::max(0.0, std::min(100.0, propOr(action, 0, 100.0)));
                    if (p == 1) value = std::max(0.0, std::min(1.0e4, propOr(action, 1, 0.0)));
                }
                if (a == 7 && (kind == "sh" || kind == "g")) value = std::max(1, std::min(7, (int)propOr(action, 0, 1.0)));
                if (kind == "j" && a == 1 && p == 1) value = std::max(0.0, std::min(1.0e4, value));
                if (std::fabs(value) > 1.0e9) value = value < 0 ? -1.0e9 : 1.0e9;
                node.setNum("p" + std::to_string(p), value);
            }
            if (kind == "j" && a == 4) {
                // Both joint kinds read p0 = upper, p1 = lower (Flash order); keep upper >= lower.
                double upper = propOr(action, 0, 0.0), lower = propOr(action, 1, 0.0);
                if (upper < lower) std::swap(upper, lower);
                node.setNum("p0", std::max(-1.0e5, std::min(1.0e5, upper)));
                node.setNum("p1", std::max(-1.0e5, std::min(1.0e5, lower)));
            }
            out.push_back(node);
        }
    }
    return out;
}

// Runtime hazards of the mobile trigger code (src/game/triggers, reproduced from the original),
// removed here because the browser game allows these combinations:
//  * shape "delete shape" (5) on a dynamic shape deletes the ShapeItem while it is still used:
//    replaced by "set to fixed" + "change collision: none", which looks the same;
//  * "change collision" (7) on a shape that some trigger deletes (6) dereferences null;
//  * group "set to fixed" (3) on a group without a body (or one whose shapes a trigger deletes);
//  * prismatic joint actions once a body of the joint can be destroyed;
//  * trigger loops that recurse forever or modify a vector while iterating it;
//  * a trigger disabling a trigger from inside the same step (d = 0).
void Converter::finishTriggers() {
    std::vector<std::vector<Node>> targets(_triggers.size());
    for (size_t i = 0; i < _triggers.size(); i++) targets[i] = emitTargets(_triggers[i], (int)i);

    // Shape index (output) -> shape record, for the body kind.
    std::map<int, const ShapeRec*> shapeByOut;
    for (const ShapeRec& s : _shapes) {
        if (s.outIndex >= 0) shapeByOut[s.outIndex] = &s;
    }
    for (const ShapeRec& s : _extra) shapeByOut[s.outIndex] = &s;

    // 1. Release (5) on dynamic shapes -> fixed + no collision.
    for (auto& list : targets) {
        std::vector<Node> rewritten;
        for (const Node& n : list) {
            const std::string* a = n.get("a");
            if (n.name == "sh" && a && *a == "5") {
                const ShapeRec* s = shapeByOut[atoi(n.get("i")->c_str())];
                if (s && s->body == BodyKind::Own) {
                    Node fix("sh");
                    fix.set("i", *n.get("i"));
                    fix.setInt("a", 1);
                    Node ghost("sh");
                    ghost.set("i", *n.get("i"));
                    ghost.setInt("a", 7);
                    ghost.setInt("p0", 3);
                    rewritten.push_back(fix);
                    rewritten.push_back(ghost);
                    continue;
                }
            }
            rewritten.push_back(n);
        }
        list.swap(rewritten);
    }
    // 2. Collect deletions.
    // releasedGroups: "delete shapes" (5) keeps the GroupItem but destroys its body, which
    // "set to fixed" (3) then dereferences; "delete self" (6) nulls every action instead.
    std::set<int> deletedShapes, releasedGroups, removedGroups;
    for (const auto& list : targets) {
        for (const Node& n : list) {
            const std::string* a = n.get("a");
            if (!a) continue;
            int index = atoi(n.get("i")->c_str());
            if (n.name == "sh" && (*a == "5" || *a == "6")) deletedShapes.insert(index);
            if (n.name == "g" && *a == "5") releasedGroups.insert(index);
            if (n.name == "g" && (*a == "5" || *a == "6")) removedGroups.insert(index);
        }
    }
    // Joint bodies that can disappear at runtime.
    std::vector<bool> fragileJoint(_jointCount, false);
    for (const JointRec& j : _joints) {
        if (!j.keep || j.outIndex < 0 || !j.prismatic) continue;
        bool fragile = false;
        for (const std::string* body : {j.node.get("b1"), j.node.get("b2")}) {
            if (!body || body->empty()) continue;
            if ((*body)[0] == 's') fragile = true;  // specials break and explode
            else if ((*body)[0] == 'g') fragile |= removedGroups.count(atoi(body->c_str() + 1)) > 0;
            else if (atoi(body->c_str()) >= 0) fragile |= deletedShapes.count(atoi(body->c_str())) > 0;
        }
        fragileJoint[j.outIndex] = fragile;
    }
    int removed = 0;
    for (size_t ti = 0; ti < targets.size(); ti++) {
        std::vector<Node> kept;
        for (const Node& n : targets[ti]) {
            const std::string* a = n.get("a");
            int index = atoi(n.get("i")->c_str());
            if (a && n.name == "sh" && *a == "7" && deletedShapes.count(index)) {
                removed++;
                continue;
            }
            if (a && n.name == "g" && *a == "3") {
                if (index >= (int)_groups.size() || !_groups[index].hasBody || releasedGroups.count(index)) {
                    removed++;
                    continue;
                }
            }
            if (n.name == "j" && index < (int)fragileJoint.size() && fragileJoint[index]) {
                removed++;
                continue;
            }
            kept.push_back(n);
        }
        targets[ti].swap(kept);
    }

    // 3. Trigger loops: activation edges (<t a=0>) inside a strongly connected component that
    // contains a trigger repeating each time / continuously are cut.
    const int triggerCount = (int)_triggers.size();
    std::vector<std::vector<int>> edges(triggerCount);
    for (int i = 0; i < triggerCount; i++) {
        for (const Node& n : targets[i]) {
            const std::string* a = n.get("a");
            if (n.name == "t" && a && *a == "0") edges[i].push_back(atoi(n.get("i")->c_str()));
        }
    }
    // Tarjan (iterative).
    std::vector<int> component(triggerCount, -1), low(triggerCount, 0), order(triggerCount, -1), stack;
    std::vector<bool> onStack(triggerCount, false);
    int counter = 0, components = 0;
    for (int root = 0; root < triggerCount; root++) {
        if (order[root] >= 0) continue;
        std::vector<std::pair<int, size_t>> work;
        work.push_back({root, 0});
        order[root] = low[root] = counter++;
        stack.push_back(root);
        onStack[root] = true;
        while (!work.empty()) {
            int v = work.back().first;
            size_t& edge = work.back().second;
            if (edge < edges[v].size()) {
                int w = edges[v][edge++];
                if (w < 0 || w >= triggerCount) continue;
                if (order[w] < 0) {
                    order[w] = low[w] = counter++;
                    stack.push_back(w);
                    onStack[w] = true;
                    work.push_back({w, 0});
                } else if (onStack[w]) {
                    low[v] = std::min(low[v], order[w]);
                }
                continue;
            }
            if (low[v] == order[v]) {
                while (true) {
                    int w = stack.back();
                    stack.pop_back();
                    onStack[w] = false;
                    component[w] = components;
                    if (w == v) break;
                }
                components++;
            }
            work.pop_back();
            if (!work.empty()) low[work.back().first] = std::min(low[work.back().first], low[v]);
        }
    }
    std::vector<int> size(components, 0);
    std::vector<bool> risky(components, false);
    for (int i = 0; i < triggerCount; i++) {
        size[component[i]]++;
        if (_triggers[i].repeat == 2 || _triggers[i].repeat == 3) risky[component[i]] = true;
    }
    int cut = 0;
    for (int i = 0; i < triggerCount; i++) {
        std::vector<Node> kept;
        for (const Node& n : targets[i]) {
            const std::string* a = n.get("a");
            if (n.name == "t" && a && *a == "0") {
                int w = atoi(n.get("i")->c_str());
                bool loop = w >= 0 && w < triggerCount && component[w] == component[i] &&
                            (size[component[i]] > 1 || w == i);
                if (loop && risky[component[i]]) {
                    cut++;
                    continue;
                }
            }
            kept.push_back(n);
        }
        targets[i].swap(kept);
    }
    if (cut) warn(plural(cut, "trigger loop link") + " cut (would hang this version)");
    if (removed) _counts["unsafeAction"] += removed;

    // 4. Emit; disabling a trigger runs one step later (d > 0) so it never edits the trigger
    // list LevelB2D::update is walking.
    _triggerOut.clear();
    for (int i = 0; i < triggerCount; i++) {
        Node node = _triggers[i].node;
        double delay = _triggers[i].delay;
        bool disables = false;
        for (const Node& n : targets[i]) {
            const std::string* a = n.get("a");
            if (n.name == "t" && a && *a == "1") disables = true;
        }
        if (_triggers[i].type == 1) {
            if (disables && delay <= 0.0) delay = 0.001;
            node.setNum("d", delay);
        } else if (_triggers[i].type == 2) {
            node.setNum("d", delay);
        }
        // Mobile pass 2 reads children by kind (t, sh, j, g, sp); keep that order.
        for (const char* kind : {"t", "sh", "j", "g", "sp"}) {
            for (const Node& n : targets[i]) {
                if (n.name == kind) node.children.push_back(n);
            }
        }
        _triggerOut.push_back(node);
    }
}

// ---------------------------------------------------------------------------------------------

std::string Converter::write() {
    Node root("levelXML");
    root.children.push_back(_info);
    Node shapes("shapes");
    for (const ShapeRec& s : _shapes) {
        if (s.outIndex >= 0) shapes.children.push_back(s.node);
    }
    for (const ShapeRec& s : _extra) shapes.children.push_back(s.node);
    if (!shapes.children.empty()) root.children.push_back(shapes);

    Node specials("specials");
    for (const SpecialRec& s : _specials) {
        if (s.outIndex >= 0) specials.children.push_back(s.node);
    }
    if (!specials.children.empty()) root.children.push_back(specials);

    Node groups("groups");
    for (const GroupRec& g : _groups) {
        Node n = g.node;
        for (const ShapeRec& s : g.shapes) {
            if (!s.dropped) n.children.push_back(s.node);
        }
        for (const Node& sp : g.specials) n.children.push_back(sp);
        groups.children.push_back(n);
    }
    if (!groups.children.empty()) root.children.push_back(groups);

    Node joints("joints");
    for (const JointRec& j : _joints) {
        if (j.keep && j.outIndex >= 0) joints.children.push_back(j.node);
    }
    for (const JointRec& j : _extraJoints) {
        if (j.keep && j.outIndex >= 0) joints.children.push_back(j.node);
    }
    if (!joints.children.empty()) root.children.push_back(joints);

    Node triggers("triggers");
    for (const Node& t : _triggerOut) triggers.children.push_back(t);
    if (!triggers.children.empty()) root.children.push_back(triggers);

    std::string out;
    out.reserve(1 << 16);
    writeNode(out, root, 0);
    return out;
}

std::string Converter::run(const std::string& flashXml) {
    tinyxml2::XMLDocument doc;
    if (flashXml.empty() || doc.Parse(flashXml.c_str(), flashXml.size()) != tinyxml2::XML_SUCCESS) {
        _report.error = "level data is not valid XML";
        return std::string();
    }
    const XMLElement* root = doc.FirstChildElement("levelXML");
    if (!root) root = doc.FirstChildElement();
    const XMLElement* info = root ? root->FirstChildElement("info") : nullptr;
    if (!info) {
        _report.error = "level data has no <info>";
        return std::string();
    }
    convertInfo(info);

    // Flash builds shapes, then specials, then groups (vertex lists are shared in that order).
    if (const XMLElement* shapes = root->FirstChildElement("shapes")) {
        for (const XMLElement* e = shapes->FirstChildElement("sh"); e; e = e->NextSiblingElement("sh")) {
            ShapeRec s;
            convertShape(e, false, s);
            _shapes.push_back(s);
        }
    }
    if (const XMLElement* specials = root->FirstChildElement("specials")) {
        int index = 0;
        for (const XMLElement* e = specials->FirstChildElement("sp"); e; e = e->NextSiblingElement("sp")) {
            convertSpecial(e, index++);
        }
    }
    if (const XMLElement* groups = root->FirstChildElement("groups")) {
        for (const XMLElement* e = groups->FirstChildElement("g"); e; e = e->NextSiblingElement("g")) {
            convertGroup(e);
        }
    }
    convertJoints(root->FirstChildElement("joints"));
    parseTriggers(root->FirstChildElement("triggers"));

    markReferences();
    enforceDrawBudget();
    assignIndices();

    // Joints: resolve both ends against what the mobile loader will create.
    _jointCount = 0;
    int droppedJoints = 0;
    auto resolve = [&](JointRec& j) {
        if (!j.keep) return;
        Pt anchor(atof(j.node.get("x")->c_str()), atof(j.node.get("y")->c_str()));
        std::string b1, b2;
        int id1 = 0, id2 = 0;
        if (!resolveJointBody(j.b1, anchor, &b1, &id1) || !resolveJointBody(j.b2, anchor, &b2, &id2) || id1 == id2) {
            j.keep = false;
            droppedJoints++;
            return;
        }
        // Attribute order as in the shipped levels: t x y b1 b2 ...
        std::vector<std::pair<std::string, std::string>> ordered;
        ordered.emplace_back("t", *j.node.get("t"));
        ordered.emplace_back("x", *j.node.get("x"));
        ordered.emplace_back("y", *j.node.get("y"));
        ordered.emplace_back("b1", b1);
        ordered.emplace_back("b2", b2);
        for (const auto& a : j.node.attrs) {
            if (a.first != "t" && a.first != "x" && a.first != "y") ordered.push_back(a);
        }
        j.node.attrs.swap(ordered);
        j.outIndex = _jointCount++;
    };
    for (JointRec& j : _joints) resolve(j);
    int chainJointsLost = 0;
    for (JointRec& j : _extraJoints) {
        int before = droppedJoints;
        resolve(j);
        if (droppedJoints != before) {
            chainJointsLost++;
            droppedJoints--;
        }
    }
    (void)chainJointsLost;
    _counts["droppedJoint"] += droppedJoints;

    finishTriggers();
    std::string out = write();

    // Report.
    int dropped = 0, substituted = 0;
    std::map<int, int> droppedByType, substitutedByType;
    for (const auto& c : _counts) {
        if (c.first.compare(0, 8, "dropped:") == 0) {
            droppedByType[atoi(c.first.c_str() + 8)] += c.second;
            dropped += c.second;
        } else if (c.first.compare(0, 12, "placeholder:") == 0) {
            substitutedByType[atoi(c.first.c_str() + 12)] += c.second;
            substituted += c.second;
        }
    }
    dropped += _counts["unknownSpecial"];
    _report.droppedItems = dropped;
    _report.substitutedItems = substituted;

    auto name = [](int type) { return std::string(type >= 0 && type <= 35 ? kSpecialNames[type] : "item"); };
    if (droppedByType.count(17)) warn(plural(droppedByType[17], "NPC character") + " removed");
    if (droppedByType.count(16)) warn(plural(droppedByType[16], "text box", "text boxes") + " not shown");
    if (droppedByType.count(33)) warn(plural(droppedByType[33], "cannon") + " removed");
    for (const auto& d : droppedByType) {
        if (d.first == 16 || d.first == 17 || d.first == 33) continue;
        warn(plural(d.second, name(d.first).c_str()) + " removed");
    }
    if (substituted) {
        std::string list;
        int shown = 0;
        for (const auto& s : substitutedByType) {
            if (shown++ == 3) {
                list += ", ...";
                break;
            }
            if (!list.empty()) list += ", ";
            list += plural(s.second, name(s.first).c_str());
        }
        warn(list + (substituted == 1 ? " replaced by a plain block" : " replaced by plain blocks"));
    }
    if (_counts["chain"]) warn(plural(_counts["chain"], "chain") + " rebuilt from simple links");
    if (!_unknownSpecialIds.empty()) warn(plural(_counts["unknownSpecial"], "newer item") + " not supported, skipped");
    if (_counts["unknownShape"]) warn(plural(_counts["unknownShape"], "unknown shape") + " skipped");
    if (_report.hasUserVehicle) warn("custom vehicle won't drive");
    if (droppedJoints) warn(plural(droppedJoints, "joint") + " to missing items removed");
    if (_counts["badTrigger"]) warn(plural(_counts["badTrigger"], "trigger") + " of an unknown kind ignored");
    if (_counts["badSound"]) warn(plural(_counts["badSound"], "sound trigger") + " silent (unknown sound)");
    if (_counts["unsafeAction"] + _counts["badAction"]) {
        warn(plural(_counts["unsafeAction"] + _counts["badAction"], "trigger action") + " left out");
    }
    _report.ok = true;
    return out;
}

}  // namespace

std::string FlashLevelConverter::toMobile(const std::string& flashXml, ConversionReport* report) {
    ConversionReport local;
    ConversionReport& r = report ? *report : local;
    r = ConversionReport();
    Converter converter(r);
    std::string out = converter.run(flashXml);
    if (!r.ok) return std::string();
    return out;
}

}  // namespace online
