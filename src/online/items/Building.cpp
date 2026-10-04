// ONLINE (PC addition): browser specials 13 / 14, Building1 / Building2 (Flash userspecials/
// Building1, Building2 + editor Building1Ref, Building2Ref, BuildingRef). The Android game has no
// buildings.
//
// XML (BuildingRef._attributes): p0 x, p1 y (top-left), p2 floorWidth 1..10, p3 numFloors 3..50.
// A static box on the level body (Building1: floorWidth x 300 by numFloors x 165 + 100 px roof;
// Building2: floorWidth x 500 + 226 by numFloors x 180 + 120) drawn like Flash: a roof strip and
// a floor texture tiled over the rest. The textures are built once, as Flash's BitmapManager
// does: Building1 = random-coloured bricks plus windows with random blinds / air conditioners
// and a ledge (Building1Source parts), Building2 = Building2Source's roof and floor textures.

#include <map>

#include "online/items/FlashSpecials.h"
#include "online/items/MiscItemUtil.h"

#include "LevelB2D.h"
#include "LevelDataElement.h"

USING_NS_CC;

namespace online {

namespace {

const float kTexZoom = 2.0f;  // texture pixels per Flash px

struct BuildingTexture {
    RenderTexture* rt = nullptr;     // retained
    std::vector<Node*> keep;         // retained until rendered (kept: cheap)
    float w = 0.0f, h = 0.0f;        // Flash px
};
std::map<std::string, BuildingTexture>& textures()
{
    static std::map<std::string, BuildingTexture> t;
    return t;
}

// Texture points per Flash px inside the render texture.
float texScale() { return kTexZoom / Director::getInstance()->getContentScaleFactor(); }

// Adds a Flash symbol render to the texture at Flash (x, y) (y down; the texture is built upside
// down so that its first row is the Flash top row, which is how sprites sample it).
Sprite* addArt(Node* root, const std::string& name, float x, float y, float sx = 1.0f, float sy = 1.0f)
{
    Sprite* s = createFlashSprite(name);
    if (!s) return nullptr;
    const float k = s->getScale() * texScale() / pointsPerFlashPx();
    s->setScaleX(k * sx);
    s->setScaleY(-k * sy);
    s->setPosition(Vec2(x * texScale(), y * texScale()));
    root->addChild(s);
    return s;
}

void addBricks(DrawNode* d, float w, float h)
{
    const float k = texScale();
    d->drawSolidRect(Vec2::ZERO, Vec2(w * k, h * k), Color4F(Color3B(0xcb, 0xbb, 0xa2)));  // 13351586
    const int cols = (int)std::floor(w / 15.0f);
    const int rows = (int)std::floor(h / 5.0f);
    float y = 0.0f;
    for (int r = 0; r < rows; r++) {
        float x = r % 2 == 1 ? -7.0f : 0.0f;
        for (int c = 0; c < cols; c++) {
            const int delta = (int)std::lround(misc::rand01() * 64.0f) - 32;
            auto ch = [&](int v) { return (GLubyte)std::max(0, std::min(255, v + delta)); };
            const Color4F color(Color3B(ch(0x8c), ch(0x79), ch(0x67)));  // 9206119 +- 32
            d->drawSolidRect(Vec2(x * k, y * k), Vec2((x + 14.0f) * k, (y + 4.0f) * k), color);
            if (x == -7.0f) d->drawSolidRect(Vec2((x + w) * k, y * k), Vec2((x + w + 14.0f) * k, (y + 4.0f) * k), color);
            x += 15.0f;
        }
        y += 5.0f;
    }
}

// Building1Ref.randomizeWindowTexture + param2.draw(window, translate(x, y)).
void addWindow(Node* root, float x, float y)
{
    const bool ac = misc::rand01() < 0.1f;
    const float frameY = ac ? 8.0f : std::ceil(misc::rand01() * 43.0f);
    const float shade = misc::rand01() * 0.8f + 0.2f;
    addArt(root, "b1_win_back", x, y);
    addArt(root, "b1_win_shade", x + 30.0f, y, 1.0f, shade);
    addArt(root, "b1_win_mid", x, y);
    addArt(root, "b1_win_fshadow", x + 7.0f, y + frameY + 4.0f);
    addArt(root, "b1_win_frame", x, y + frameY);
    addArt(root, "b1_win_front", x, y);
    if (ac) addArt(root, "b1_win_ac", x + 5.0f, y + 55.0f);
}

Texture2D* buildTexture(const std::string& key)
{
    auto& cache = textures();
    auto it = cache.find(key);
    if (it != cache.end()) return it->second.rt ? it->second.rt->getSprite()->getTexture() : nullptr;
    BuildingTexture& t = cache[key];
    if (!Director::getInstance()->getOpenGLView()) return nullptr;

    Node* root = Node::create();
    if (key == "b1roof") {
        t.w = 300.0f;
        t.h = 100.0f;
        auto* d = DrawNode::create();
        addBricks(d, t.w, t.h);
        root->addChild(d);
        addArt(root, "b1_roof", 0.0f, 0.0f);
    } else if (key == "b1floor") {
        t.w = 600.0f;
        t.h = 330.0f;
        auto* d = DrawNode::create();
        addBricks(d, t.w, t.h);
        root->addChild(d);
        // addWindows: floor(600 / 300) * 3 per row, floor(330 / 165) rows.
        for (int row = 0; row < 2; row++) {
            float x = 40.0f;
            for (int i = 0; i < 6; i++) {
                addWindow(root, x + i * 80.0f, row * 165.0f);
                if (i % 3 == 2) x += 60.0f;
            }
        }
        // addLedge: the ledge texture scaled 50x horizontally, 90 px down.
        addArt(root, "b1_ledge", 0.0f, 90.0f, 50.0f, 1.0f);
    } else if (key == "b2roof") {
        t.w = 500.0f;
        t.h = 120.0f;
        addArt(root, "b2_roof", 0.0f, 0.0f);
    } else {
        t.w = 500.0f;
        t.h = 180.0f;
        addArt(root, "b2_floor", 0.0f, 0.0f);
    }
    const float k = texScale();
    t.rt = RenderTexture::create((int)std::lround(t.w * k), (int)std::lround(t.h * k),
                                 Texture2D::PixelFormat::RGBA8888);
    if (!t.rt) return nullptr;
    t.rt->retain();
    root->retain();
    t.keep.push_back(root);
    // Building2's bitmaps are opaque (BitmapData(..., false, 0)): transparent parts are black.
    t.rt->beginWithClear(0.0f, 0.0f, 0.0f, 1.0f);
    root->visit(Director::getInstance()->getRenderer(), Mat4::IDENTITY, 0);
    t.rt->end();
    Texture2D* tex = t.rt->getSprite()->getTexture();
    Texture2D::TexParams params = {GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT};
    tex->setTexParameters(params);
    return tex;
}

class Building : public FlashItem
{
public:
    explicit Building(int type) : _type(type) {}
    ~Building() override
    {
        if (_root) _root->release();
    }

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override
    {
        (void)groupBody;
        (void)groupOffset;
        const float x = num(element, "p0", 0.0f);
        const float y = num(element, "p1", 0.0f);
        const int floorWidth = std::max(1, std::min(10, inum(element, "p2", 1)));
        const int floors = std::max(3, std::min(50, inum(element, "p3", 3)));
        const float tileH = _type == 1 ? 165.0f : 180.0f;
        const float roofH = _type == 1 ? 100.0f : 120.0f;
        const float w = _type == 1 ? floorWidth * 300.0f : floorWidth * 500.0f + 226.0f;
        const float h = floors * tileH + roofH;

        b2PolygonShape box;
        const b2Vec2 c = flashToWorld(x + w * 0.5f, y + h * 0.5f) - getLevelBody()->GetPosition();
        box.SetAsBox(w * 0.5f / kFlashPtm, h * 0.5f / kFlashPtm, c, 0.0f);
        b2FixtureDef fd;
        fd.shape = &box;
        fd.friction = 0.3f;
        fd.restitution = 0.1f;
        fd.filter.categoryBits = 8;
        getLevelBody()->CreateFixture(&fd);

        _root = Node::create();
        _root->retain();
        if (Node* layer = flashBackgroundLayer()) layer->addChild(_root);
        misc::placeFlash(_root, x, y, 0.0f);
        const std::string prefix = _type == 1 ? "b1" : "b2";
        addStrip(prefix + "roof", 0.0f, w, roofH, Color4F(0.55f, 0.5f, 0.45f, 1.0f));
        addStrip(prefix + "floor", roofH, w, tileH * floors, Color4F(0.45f, 0.42f, 0.4f, 1.0f));
        return true;
    }

private:
    void addStrip(const std::string& key, float top, float w, float h, const Color4F& fallback)
    {
        const float ppf = pointsPerFlashPx();
        Texture2D* tex = buildTexture(key);
        if (!tex) {
            auto* d = DrawNode::create();
            d->drawSolidRect(Vec2(0.0f, -(top + h) * ppf), Vec2(w * ppf, -top * ppf), fallback);
            _root->addChild(d);
            return;
        }
        const float k = texScale();
        Sprite* s = Sprite::createWithTexture(tex, Rect(0.0f, 0.0f, w * k, h * k));
        s->setAnchorPoint(Vec2(0.0f, 1.0f));
        s->setPosition(Vec2(0.0f, -top * ppf));
        s->setScale(ppf / k);
        _root->addChild(s);
    }

    int _type;
    Node* _root = nullptr;
};

FlashSpecialRegistration s_reg1(13, [] { return (LevelItem*)new (std::nothrow) Building(1); });
FlashSpecialRegistration s_reg2(14, [] { return (LevelItem*)new (std::nothrow) Building(2); });

}  // namespace

}  // namespace online
