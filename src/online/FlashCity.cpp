// ONLINE (PC addition): see FlashCity.h.
#include "online/FlashCity.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "cocos2d.h"

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
                for (float fy = p.y; fy < p.tileTo; fy += p.tileH) add(p, fx, fy);
            } else {
                add(p, fx, p.y);
            }
        }
    }
    return out;
}

}  // namespace online
