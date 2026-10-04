// ONLINE (PC addition): see Token.h. Port of com.totaljerkface.game.level.userspecials.Token and
// the token HUD of com.totaljerkface.game.level.UserLevel.
#include "online/items/Token.h"

#include <cmath>

#include "CharacterB2D.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Session.h"
#include "Settings.h"

USING_NS_CC;

namespace online {

namespace {

FlashSpecialRegistration s_reg(31, [] { return (LevelItem*)new (std::nothrow) Token(); },
                               FlashSpecialUse::Override);

// CoinMC: container (the face) x offset (twips) and scaleX per frame; frame 9 is edge-on.
const float kFaceX[16] = {0, -14, -26, -38, -49, -57, -64, -67, 0, 67, 64, 57, 49, 38, 26, 14};
const float kFaceScale[16] = {1.0f, 0.98125f, 0.9225f, 0.83125f, 0.705f, 0.5543f, 0.38199f, 0.19696f,
                              0.0f, 0.19696f, 0.38199f, 0.5543f, 0.705f, 0.83125f, 0.9225f, 0.98125f};

// UserLevel token HUD: one per gameplay session.
struct TokenHud {
    Session* session = nullptr;
    LevelB2D* level = nullptr;
    Node* node = nullptr;  // retained
    Label* label = nullptr;
    Node* icon = nullptr;
    float scale = 1.0f;  // screen points per Flash stage px
    int total = 0;
    int remaining = 0;
};
TokenHud g_hud;

void layoutHud()
{
    if (!g_hud.label) return;
    g_hud.label->setString(std::to_string(g_hud.total - g_hud.remaining) + "/" + std::to_string(g_hud.total));
    // Flash: tokenField right-aligned 8 px left of the HUD origin, icon 3 px left of the text.
    const Size text = g_hud.label->getContentSize() * g_hud.label->getScale();
    g_hud.label->setPosition(Vec2(-8.0f * g_hud.scale, 0.0f));
    if (g_hud.icon) {
        const float iconW = g_hud.icon->getBoundingBox().size.width;
        g_hud.icon->setPosition(Vec2(-8.0f * g_hud.scale - text.width - iconW * 0.5f - 3.0f * g_hud.scale,
                                     -(text.height * 0.5f + 1.0f * g_hud.scale)));
    }
}

void registerToken()
{
    Session* session = Settings::getInstance()->getCurrentSession();
    LevelB2D* level = session ? session->getLevel() : nullptr;
    if (g_hud.session != session || g_hud.level != level || !g_hud.node) {
        if (g_hud.node) {
            g_hud.node->removeFromParent();
            g_hud.node->release();
        }
        g_hud = TokenHud();
        g_hud.session = session;
        g_hud.level = level;
        Node* parent = session ? session->getParent() : nullptr;
        if (parent) {
            const Size visible = Director::getInstance()->getVisibleSize();
            const Vec2 origin = Director::getInstance()->getVisibleOrigin();
            g_hud.scale = visible.height / 500.0f;  // Flash stage 900 x 500
            g_hud.node = Node::create();
            g_hud.node->retain();
            g_hud.node->setName("flashTokenHud");
            // Flash: x 900 (right edge), y 4; here below the eject button.
            g_hud.node->setPosition(Vec2(origin.x + visible.width - 4.0f * g_hud.scale,
                                         origin.y + visible.height - 105.0f * g_hud.scale));
            parent->addChild(g_hud.node, 6);

            const float size = 18.0f * g_hud.scale;
            const std::string font = "generated/flash/fonts/clarendon_bold.ttf";
            if (FileUtils::getInstance()->isFileExist(font)) {
                g_hud.label = Label::createWithTTF(TTFConfig(font, std::min(size, 72.0f)), "0/0");
            }
            if (!g_hud.label) g_hud.label = Label::createWithSystemFont("0/0", "Georgia Bold", std::min(size, 72.0f));
            g_hud.label->setScale(size / std::min(size, 72.0f));
            g_hud.label->setTextColor(Color4B(0x3d, 0x88, 0xc7, 255));
            g_hud.label->enableShadow(Color4B(0, 0, 0, 64), Size(0, -2.0f * g_hud.scale), 2);
            g_hud.label->setAnchorPoint(Vec2(1.0f, 1.0f));
            g_hud.node->addChild(g_hud.label);

            if (Sprite* icon = createFlashSprite("token_icon")) {
                icon->setScale(icon->getScale() / pointsPerFlashPx() * g_hud.scale);
                g_hud.icon = icon;
            } else {
                auto* d = DrawNode::create();
                d->drawSolidCircle(Vec2::ZERO, 8.0f * g_hud.scale, 0, 20, Color4F(0.95f, 0.85f, 0.3f, 1.0f));
                g_hud.icon = d;
            }
            g_hud.node->addChild(g_hud.icon);
        }
    }
    g_hud.total++;
    g_hud.remaining++;
    layoutHud();
}

}  // namespace

Token::~Token()
{
    if (_mc) _mc->release();
}

bool Token::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    (void)groupBody;
    (void)groupOffset;
    const float x = num(element, "p0", 0.0f);
    const float y = num(element, "p1", 0.0f);
    const int type = std::max(1, std::min(6, inum(element, "p2", 1)));

    _mc = Node::create();
    _mc->retain();
    _edge = createFlashSprite("coin_edge_1");
    if (_edge) _mc->addChild(_edge, 0);
    _face = createFlashSprite("coin_face_" + std::to_string(type));
    if (!_face) {
        auto* d = DrawNode::create();
        d->drawSolidCircle(Vec2::ZERO, 20.0f * pointsPerFlashPx(), 0, 24, Color4F(0.95f, 0.8f, 0.25f, 1.0f));
        _face = d;
    }
    _mc->addChild(_face, 1);
    if (Node* layer = flashBackgroundLayer()) layer->addChild(_mc);
    misc::placeFlash(_mc, x, y, 0.0f);

    b2BodyDef bd;
    bd.position = flashToWorld(x, y);
    _body = getWorld()->CreateBody(&bd);
    b2CircleShape circle;
    circle.m_radius = 23.0f / kFlashPtm;
    b2FixtureDef fd;
    fd.shape = &circle;
    fd.isSensor = true;
    fd.filter.categoryBits = 8;
    _shape = _body->CreateFixture(&fd);
    addToBeginContact(_shape);
    getLevel()->addToPaintItem(this);
    registerToken();
    return true;
}

void Token::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    (void)fixture;
    (void)contact;
    if (_collected) return;
    if (!(getLevel()->getFixtureMaterial(otherFixture) & 2)) return;
    auto* item = static_cast<LevelItem*>(otherFixture->GetUserData());
    if (!item || item->getSpecialType() != SpecialTypeCharacter) return;
    if (static_cast<CharacterB2D*>(item)->getDead()) return;
    _collected = true;  // the listener is removed in singleAction (not while dispatching)
    const b2Vec2 p = _body->GetPosition();
    const char* sound = FileUtils::getInstance()->isFileExist("sounds/Bleep3.ogg") ? "Bleep3" : "HomingMineBeep";
    createPositionSound(sound, Vec2(p.x, p.y), 1.0f, false);
    getLevel()->addToSingleActions(this);
}

void Token::singleAction()
{
    if (!_body) return;
    removeBeginContact(_shape);
    getLevel()->removeFromPaintItem(this);
    if (_mc) _mc->removeFromParent();
    getWorld()->DestroyBody(_body);
    _body = nullptr;
    _shape = nullptr;

    Session* session = Settings::getInstance()->getCurrentSession();
    if (g_hud.session == session && g_hud.level == getLevel() && g_hud.remaining > 0) {
        g_hud.remaining--;
        layoutHud();
        if (g_hud.remaining == 0) getLevel()->levelCompleted();  // Gameplay plays "Victory"
    }
}

void Token::paint()
{
    const int frames = _clock.advance(getTimeStepOverFlashTimeStep());
    if (frames == 0) return;
    _frame = (_frame + frames) % 16;
    if (_edge) {
        _edge->setVisible(_frame != 0);
        if (_frame != 0) misc::setFlashFrame(_edge, "coin_edge_" + std::to_string(_frame + 1));
    }
    if (_face) {
        _face->setPosition(misc::localPx(kFaceX[_frame] / 20.0f, 0.0f));
        _face->setScaleX(std::fabs(_face->getScaleY()) * kFaceScale[_frame]);
        _face->setVisible(kFaceScale[_frame] > 0.0f);
    }
}

}  // namespace online
