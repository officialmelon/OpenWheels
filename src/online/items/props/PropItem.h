#pragma once
// ONLINE (PC addition): shared base of the browser-game breakable props (Table, Chair, TV,
// Boombox, Toilet, TrashCan, FoodItem; Flash userspecials/*.as). Only used in converted browser
// levels. It reproduces the pieces of the Flash runtime these classes lean on:
//
//  - LevelB2D.paintBodyVector: Flash paints every body's MovieClip at the body's WORLD CENTRE
//    (not its origin, unlike the mobile LevelB2D::paint), so an item's art and its break-off
//    pieces sit exactly where the browser game draws them (including its offsets).
//  - ContactListener RESULT / ADD listeners: postSolve (strongest normal impulse of the manifold,
//    the mobile convention for Flash thresholds, cf. Bottle/Van) and the hit-sound test of the
//    classes' checkAdd (= LevelItem::contactSoundHandler, buffered and handled in actions()).
//  - ParticleController.createRectBurst / createSpray with the SWF's bitmap particle frames.
//  - The common trigger actions "wake from sleep" (0) and "apply impulse" (1: x, y, spin).
//
// Coordinates: Flash local pixels (x right, y down) map to Box2D local metres (x / 62.5,
// -y / 62.5); Flash body angles are the negated world angles (flashAngle()).

#include <memory>
#include <string>
#include <vector>

#include "Box2D/Box2D.h"
#include "math/Vec2.h"
#include "base/ccTypes.h"

#include "online/items/FlashSpecials.h"

namespace cocos2d {
class Node;
class Sprite;
}

namespace online {

// Flash local pixels -> Box2D local metres.
inline b2Vec2 propLocal(float xPx, float yPx) { return b2Vec2(xPx / 62.5f, -yPx / 62.5f); }

struct PropFixture {
    float density = 2.0f;
    float friction = 0.3f;
    float restitution = 0.1f;
    uint16 category = 8;
    uint16 mask = 0xffff;
};

class PropItem : public FlashItem
{
public:
    ~PropItem() override;

    void paint() override;
    void actions() override;
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;

    b2Body* getJointBody(b2Vec2 point) override { return _body; }
    std::vector<b2Body*> getBodyList() override;
    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override;

    // Groups (art-only variant): the root node follows the group body.
    void paintWithOffsetPoints(cocos2d::Vec2 offset, float rotation) override;
    void setOpacity(float opacity) override;

protected:
    // ---- set-up ----------------------------------------------------------------------------
    // Reads p0 x, p1 y, p2 angle, creates _root (the item's "mc") at that place on the
    // background layer (or the group's layer). Returns false when in a group (art only).
    void placeRoot(LevelDataElement* element, b2Body* groupBody, bool foreground = false);
    // Art of a rendered SWF symbol (generated/flash/<name>.png) or, without the art, a plain box
    // of w x h Flash px centred on (cx, cy).
    static cocos2d::Node* art(const std::string& name, float w, float h, float cx, float cy,
                              const cocos2d::Color4F& color);
    // Node position for a Flash local offset (px, y down).
    static cocos2d::Vec2 pt(float xPx, float yPx);

    // ---- bodies ----------------------------------------------------------------------------
    b2Body* createBody(const b2Vec2& position, float angle, bool sleeping = false);
    b2Fixture* addBox(b2Body* body, float hwPx, float hhPx, float cxPx, float cyPx,
                      const PropFixture& def);
    b2Fixture* addPolygon(b2Body* body, const std::vector<b2Vec2>& flashPx, const PropFixture& def);
    b2Fixture* addCircle(b2Body* body, float radiusPx, const PropFixture& def);
    // Flash paintBodyVector: node drawn at the body's world centre, rotated with it.
    void paintBody(b2Body* body, cocos2d::Node* node);
    void unpaintBody(b2Body* body);
    // Removes every listener / material of the body's fixtures, then destroys it.
    void destroyBody(b2Body* body);
    void destroyFixture(b2Fixture* fixture);
    void listenImpulse(b2Fixture* fixture);   // RESULT
    void forgetImpulse(b2Fixture* fixture);
    void listenHits(b2Fixture* fixture);      // ADD
    void forgetHits(b2Fixture* fixture);

    // ---- hooks -----------------------------------------------------------------------------
    virtual void onImpulse(b2Fixture* fixture, b2Fixture* other, b2Contact* contact, float impulse) {}
    virtual void onHit(b2Fixture* fixture) {}

    // ---- sounds ----------------------------------------------------------------------------
    // Flash playAreaSoundInstance(name, body). Bodies that may be destroyed later get a position
    // sound (the mobile Sound keeps the body pointer).
    void playSound(const std::string& name, b2Body* body, bool bodyStays = false);
    // As above, but only when the previous sound of this slot has finished (Flash keeps the
    // AreaSoundInstance until AREA_SOUND_STOP). slot 0 or 1.
    void playGatedSound(int slot, const std::string& name, b2Body* body, bool bodyStays = false);

    // ---- particles -------------------------------------------------------------------------
    // Flash createRectBurst(type, speedRange, body, total): particles from random points of the
    // bounds of the body's first fixture (a polygon), with the body's point velocity +- range/2.
    void rectBurst(const std::string& prefix, int frames, float speedRange, b2Body* body, int total);
    // Flash createSpray(type, body, start, end, minSpeed, maxSpeed, angleRange, perFrame, total,
    // background layer). Start / end in Flash local px of the body. Returns the spray's node
    // (added to the background layer) so pieces can be put in front of it.
    cocos2d::Node* spray(const std::string& prefix, int frames, b2Body* body, float sx, float sy,
                         float ex, float ey, float minSpeed, float maxSpeed, float angleRangeDeg,
                         int perFrame, int total);
    // Puts a node in front of its siblings (Flash addChildAt below the piece, inverted).
    static void bringToFront(cocos2d::Node* node);

    cocos2d::Node* _root = nullptr;  // the item's mc (retained)
    b2Body* _body = nullptr;         // main body (null when not interactive / broken)
    bool _inGroup = false;
    bool _foreground = false;

private:
    struct Painted {
        b2Body* body;
        cocos2d::Node* node;
    };
    struct Particle {
        cocos2d::Sprite* sprite;
        cocos2d::Vec2 pos;  // points
        cocos2d::Vec2 vel;  // points per second
    };
    struct Spray {
        std::string prefix;
        int frames;
        b2Body* body;
        b2Vec2 start, range;  // Box2D local metres
        float rot, angleRange, minSpeed, maxSpeed;  // Flash angles (radians, y down)
        int perFrame, total, count;
        cocos2d::Node* layer;
        bool finished;
    };
    cocos2d::Sprite* particleSprite(const std::string& prefix, int frames);
    void addParticle(cocos2d::Node* layer, cocos2d::Sprite* sprite, const b2Vec2& worldM,
                     const b2Vec2& velM);
    void stepParticles();

    std::vector<Painted> _painted;
    bool _paintRegistered = false;
    std::vector<b2Fixture*> _impulseFixtures;
    std::vector<b2Fixture*> _hitFixtures;
    std::vector<Particle> _particles;
    std::vector<Spray> _sprays;
    int _stepCounter = 0;
    std::shared_ptr<bool> _soundBusy[2];
    cocos2d::Node* _burstLayer = nullptr;
};

}  // namespace online
