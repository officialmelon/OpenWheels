// ONLINE (PC addition): see FlashCity.h.
#include "online/FlashCity.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "cocos2d.h"

#include "qol/QoL.h"

USING_NS_CC;

namespace online {

namespace {

struct Piece {
    int layer = 0;
    float multiplier = 0.0f;
    std::string name;
    float x = 0.0f, y = 0.0f;   // Flash px, top-left in backdrop space
    std::string kind;
    float tileTo = 0.0f;
    float repeatDx = 0.0f;
    float tileH = 0.0f;
};

const char* const kDir = "generated/flash/city/";
const float kZoom = 2.0f;  // image px per Flash px (tools/assets/flash_city.py ZOOM)
const char* const kCityNodeName = "online_city_backdrops";

// The pieces of one BackgroundLayer, kept on a child node so that update can find them.
class CityBackdrops : public Node
{
public:
    CREATE_FUNC(CityBackdrops);
    std::vector<CityBackdropPiece> pieces;
    float baseScale = 1.0f;
};

}  // namespace

std::vector<CityBackdropPiece> createCityBackground(Node* parent, float ptmRatio)
{
    std::vector<CityBackdropPiece> out;
    FileUtils* fu = FileUtils::getInstance();
    const std::string tsv = fu->getStringFromFile(std::string(kDir) + "city.tsv");
    if (tsv.empty()) return out;
    std::vector<Piece> pieces;
    std::istringstream in(tsv);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream f(line);
        Piece p;
        if (f >> p.layer >> p.multiplier >> p.name >> p.x >> p.y >> p.kind >> p.tileTo >> p.repeatDx >> p.tileH) {
            pieces.push_back(p);
        }
    }
    if (pieces.empty()) return out;
    std::stable_sort(pieces.begin(), pieces.end(), [](const Piece& a, const Piece& b) { return a.layer < b.layer; });

    const float k = ptmRatio / 62.5f;  // points per Flash px
    const float h = Director::getInstance()->getWinSize().height;
    const float scale = k * Director::getInstance()->getContentScaleFactor() / kZoom;
    // QOL (PC addition): a zoomed-out view at the stage's bottom reaches (H / 2)(1 / zoom - 1)
    // further down, of which a backdrop of multiplier m moves along 1 - m (FlashCity.h).
    const float zoom = qol::cameraZoom();
    const float extraDepth = zoom < 1.0f ? (h / k) * 0.5f * (1.0f / zoom - 1.0f) : 0.0f;
    // Linear filtering, as the GL default of these textures.
    auto add = [&](const Piece& p, float fx, float fy) {
        Sprite* s = Sprite::create(std::string(kDir) + p.name + ".png");
        if (!s) return;
        s->setAnchorPoint(Vec2(0.0f, 1.0f));
        s->setScale(scale);
        parent->addChild(s, p.layer);
        // Screen y (points, up) of Flash backdrop y `fy` with the container at the origin; the
        // Backdrop then adds multiplier * container position (derivation in FlashCity.h terms:
        // Flash backdrop.y = m * containerY, containerY = (H - cocosContainerY) / k - 10000).
        const Vec2 origin(k * fx, h - k * fy + p.multiplier * (10000.0f * k - h));
        s->setPosition(origin);
        out.push_back({s, origin, p.multiplier});
    };
    for (const Piece& p : pieces) {
        for (int copy = 0; copy < (p.repeatDx > 0.0f ? 2 : 1); copy++) {
            const float fx = p.x + copy * p.repeatDx;
            if (p.kind == "strip") {
                if (p.tileH <= 0.0f) continue;
                const float tileTo = p.tileTo + extraDepth * (1.0f - p.multiplier);
                for (float fy = p.y; fy < tileTo; fy += p.tileH) add(p, fx, fy);
            } else {
                add(p, fx, p.y);
            }
        }
    }
    if (!out.empty()) {
        CityBackdrops* holder = CityBackdrops::create();
        holder->setName(kCityNodeName);
        holder->pieces = out;
        holder->baseScale = scale;
        parent->addChild(holder);
    }
    return out;
}

void updateCityBackground(Node* parent, const Vec2& containerPosition)
{
    auto* holder = dynamic_cast<CityBackdrops*>(parent->getChildByName(kCityNodeName));
    if (!holder) return;
    const float zoom = qol::cameraZoom();
    if (zoom >= 1.0f) {
        for (const CityBackdropPiece& piece : holder->pieces) {
            piece.sprite->setPosition(piece.origin + containerPosition * piece.multiplier);
        }
        return;
    }
    // The session is scaled about its origin: screen = container + zoom * world. The same view
    // centre at zoom 1 has the container at centre - (centre - container) / zoom.
    const Size win = Director::getInstance()->getWinSize();
    const Vec2 centre(win.width * 0.5f, win.height * 0.5f);
    const Vec2 unzoomed = centre - (centre - containerPosition) / zoom;
    for (const CityBackdropPiece& piece : holder->pieces) {
        if (piece.multiplier == 0.0f) {
            piece.sprite->setPosition(piece.origin);  // the sky fills the screen
            continue;
        }
        const Vec2 atZoom1 = piece.origin + unzoomed * piece.multiplier;
        piece.sprite->setPosition(centre + (atZoom1 - centre) * zoom);
        piece.sprite->setScale(holder->baseScale * zoom);
    }
}

}  // namespace online
