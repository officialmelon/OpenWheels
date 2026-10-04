// ONLINE (PC addition): browser special 35, Paddle (Flash userspecials/Paddle + editor PaddleRef).
// The Android game has no paddle platform.
//
// XML (PaddleRef._attributes): p0 x, p1 y, p2 angle, p3 springDelay 0..2 s, p4 reverse,
// p5 paddleAngle 15..90, p6 paddleSpeed 1..10.
//
// A heavy pad (332 x 30 px, density 200) hinged at one end on a static base. Anything hitting it
// (except its blocker) starts the delay countdown shown on its timer; then the motor flips it up
// to paddleAngle at 0.5..6 rad/s (SpringBoxBounce), lets it fall back, and re-arms. A hinge pulled
// apart breaks (PaddleBreak, red timer), leaving a loose pad. No trigger actions.

#include <cstdio>

#include "online/items/FlashSpecials.h"
#include "online/items/MiscItemUtil.h"

#include "BurstEmitter.h"
#include "EmitterNode.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Session.h"

USING_NS_CC;

namespace online {

namespace {

const float kPi = 3.14159265f;

std::string fixed2(float v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

class Paddle : public FlashItem
{
public:
    ~Paddle() override
    {
        for (Node* n : {_paddle, _base}) {
            if (n) n->release();
        }
    }

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override
    {
        (void)groupBody;
        (void)groupOffset;
        const float x = num(element, "p0", 0.0f);
        const float y = num(element, "p1", 0.0f);
        const float rotation = num(element, "p2", 0.0f);
        const float springDelay = std::max(0.0f, std::min(2.0f, num(element, "p3", 0.0f)));
        const bool reverse = flag(element, "p4", false);
        const float paddleAngle = std::max(15.0f, std::min(90.0f, num(element, "p5", 90.0f)));
        const float paddleSpeed = std::max(1.0f, std::min(10.0f, num(element, "p6", 10.0f)));
        _tarAng = paddleAngle * kPi / 180.0f;
        _speed = 0.5f + (6.0f - 0.5f) * paddleSpeed / 10.0f;
        if (reverse) {
            _direction = -1;
            _tarAng *= -1.0f;
        }
        const float d = (float)_direction;

        // Art: base in the background, paddle in the foreground.
        _base = Node::create();
        _base->retain();
        _base->addChild(misc::artOr("paddle_base", 350.0f, 40.0f, Color4F(0.45f, 0.45f, 0.45f, 1.0f)));
        if (Sprite* nub = createFlashSprite("paddle_base_nub")) {
            nub->setPosition(misc::localPx(151.35f * d, -4.1f));
            _base->addChild(nub);
        }
        if (Node* bg = flashBackgroundLayer()) bg->addChild(_base);
        misc::placeFlash(_base, x, y, rotation);

        _paddle = Node::create();
        _paddle->retain();
        _paddle->addChild(misc::artOr("paddle", 332.0f, 30.0f, Color4F(0.6f, 0.6f, 0.62f, 1.0f), 0.0f, -5.0f));
        if (Sprite* nub = createFlashSprite("paddle_nub")) {
            nub->setPosition(misc::localPx(151.35f * d, 0.95f));
            _paddle->addChild(nub);
        }
        if (Sprite* arrow = createFlashSprite("paddle_arrow")) {
            arrow->setPosition(misc::localPx(-145.2f * d, 0.0f));
            _paddle->addChild(arrow);
            if ((_glow = createFlashSprite("paddle_glow"))) {
                _glow->setPosition(misc::localPx(-145.2f * d + 0.05f, 0.0f));
                _glow->setVisible(false);
                _paddle->addChild(_glow);
            }
        }
        Node* timer = Node::create();
        timer->setPosition(misc::localPx(124.4f * d, 0.85f));
        _paddle->addChild(timer);
        if (Sprite* bg = createFlashSprite("paddle_timer")) timer->addChild(bg);
        const float ppf = pointsPerFlashPx();
        const std::string font = "generated/flash/fonts/helvetica_bold.ttf";
        if (FileUtils::getInstance()->isFileExist(font)) {
            _timerText = Label::createWithTTF(TTFConfig(font, std::min(11.0f * ppf, 72.0f)), "0.00");
        }
        if (!_timerText) _timerText = Label::createWithSystemFont("0.00", "Arial Bold", std::min(11.0f * ppf, 72.0f));
        _timerText->setScale(11.0f * ppf / std::min(11.0f * ppf, 72.0f));
        _timerText->setTextColor(Color4B(0x99, 0xff, 0x66, 255));
        // TimerText field: x -16.45, y -6.65, 34 x 17 px.
        _timerText->setAnchorPoint(Vec2(0.5f, 0.5f));
        _timerText->setPosition(misc::localPx(-16.45f + 17.0f, -6.65f + 8.5f));
        timer->addChild(_timerText);
        if (Node* fg = flashForegroundLayer()) fg->addChild(_paddle);
        misc::placeFlash(_paddle, x, y, rotation);

        _delayTotal = (int)std::lround(springDelay * 30.0f);
        _delayCounter = _delayTotal;
        _delayString = fixed2(_delayCounter / 30.0f);
        _timerText->setString(_delayString);

        createBodies(x, y, rotation);
        getLevel()->addToPaintItem(this);
        getLevel()->addToActions(this);
        _listening = true;
        return true;
    }

    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override
    {
        (void)fixture;
        (void)contact;
        (void)impulse;
        if (!_listening || otherFixture == _blocker) return;
        _listening = false;
        _hit = true;
    }

    void paint() override
    {
        if (!_body->IsAwake()) return;
        misc::paintFromBody(_paddle, _body, _body->GetWorldCenter());
    }

    void actions() override
    {
        const int frames = _clock.advance(getTimeStepOverFlashTimeStep());
        for (int f = 0; f < frames && !_broken; f++) flashFrame();
    }

private:
    float jointAngle() const { return -_joint->GetJointAngle(); }
    float motorSpeed() const { return -_joint->GetMotorSpeed(); }
    void setMotorSpeed(float s) { _joint->SetMotorSpeed(-s); }
    void setLimits(float lower, float upper) { _joint->SetLimits(-upper, -lower); }

    void createBodies(float x, float y, float rotation)
    {
        b2World* world = getWorld();
        const float m = 1.0f / kFlashPtm;
        b2BodyDef bd;
        bd.type = b2_dynamicBody;
        bd.position = flashToWorld(x, y);
        bd.angle = flashAngle(rotation);
        _body = world->CreateBody(&bd);
        b2PolygonShape box;
        box.SetAsBox(332.0f * 0.5f * m, 30.0f * 0.5f * m, b2Vec2(0.0f, 5.0f * m), 0.0f);  // Flash (0, -5)
        b2FixtureDef fd;
        fd.shape = &box;
        fd.density = 200.0f;
        fd.friction = 0.5f;
        fd.restitution = 0.1f;
        fd.filter.categoryBits = 8;
        fd.filter.groupIndex = 15;
        _padShape = _body->CreateFixture(&fd);
        addToPostSolve(_padShape);

        // Base on the level body.
        b2PolygonShape base;
        base.SetAsBox(350.0f * 0.5f * m, 40.0f * 0.5f * m, _body->GetPosition() - getLevelBody()->GetPosition(),
                      _body->GetAngle());
        fd.shape = &base;
        fd.density = 0.0f;
        fd.filter.groupIndex = 0;
        getLevelBody()->CreateFixture(&fd);

        // Blocker the pad rests on (Flash local (0, 15), 262.5 x 10 px, static).
        b2BodyDef sd;
        sd.position = _body->GetWorldPoint(b2Vec2(0.0f, -15.0f * m));
        sd.angle = _body->GetAngle();
        b2Body* blockerBody = world->CreateBody(&sd);
        b2PolygonShape blocker;
        blocker.SetAsBox(350.0f * 0.75f / 2.0f * m, 10.0f / 2.0f * m);
        fd.shape = &blocker;
        fd.filter.groupIndex = 15;
        _blocker = blockerBody->CreateFixture(&fd);

        b2RevoluteJointDef jd;
        const b2Vec2 anchor = _body->GetWorldPoint(b2Vec2(_direction * (332.0f / 2 - 30.0f / 2) * m, 5.0f * m));
        jd.Initialize(getLevelBody(), _body, anchor);
        jd.enableLimit = true;
        jd.lowerAngle = 0.0f;
        jd.upperAngle = 0.0f;
        jd.enableMotor = false;
        jd.maxMotorTorque = 1000000.0f;
        _joint = (b2RevoluteJoint*)world->CreateJoint(&jd);
    }

    void flashFrame()
    {
        if (_hit) {
            if (_delayCounter == 0) {
                if (_glow) _glow->setVisible(true);
                _hit = false;
                _body->SetAwake(true);
                _body->SetBullet(true);
                setMotorSpeed(_direction * _speed);
                if (_direction == -1) {
                    setLimits(-90.0f * kPi / 180.0f, 0.0f);
                } else {
                    setLimits(0.0f, 90.0f * kPi / 180.0f);
                }
                _joint->EnableMotor(true);
                _delayCounter = _delayTotal;
                _timerText->setString("0.00");
                createBodySound("SpringBoxBounce", _body, 1.0f, false);
            } else {
                _timerText->setString(fixed2(_delayCounter / 30.0f));
                --_delayCounter;
            }
        } else if (_joint->IsMotorEnabled()) {
            const b2Vec2 dd = _joint->GetAnchorB() - _joint->GetAnchorA();
            if (dd.LengthSquared() > 0.43f) {
                breakJoint();
                return;
            }
            const float speed = motorSpeed();
            const float angle = jointAngle();
            if ((speed > 0 && _direction == 1) || (speed < 0 && _direction == -1)) {
                if (_direction == 1) {
                    if (angle > _tarAng) setMotorSpeed(-2.0f);
                } else if (angle < _tarAng) {
                    setMotorSpeed(2.0f);
                }
            } else if ((speed < 0 && _direction == 1) || (speed > 0 && _direction == -1)) {
                if ((angle < 0 && _direction == 1) || (angle > 0 && _direction == -1)) {
                    _joint->EnableMotor(false);
                    setLimits(0.0f, 0.0f);
                    setMotorSpeed(0.0f);
                    _body->SetBullet(false);
                    _listening = true;
                    _timerText->setString(_delayString);
                    if (_glow) _glow->setVisible(false);
                }
            }
        }
    }

    void breakJoint()
    {
        const b2Vec2 anchor = _joint->GetAnchorA();
        if (EmitterNode* particles = getSession()->getParticlesForeground()) {
            if (Emitter* burst = BurstEmitter::createCartBurst(misc::worldPoints(anchor))) particles->addChild(burst);
        }
        getWorld()->DestroyJoint(_joint);
        _joint = nullptr;
        _broken = true;
        getLevel()->removeFromActions(this);
        _timerText->setTextColor(Color4B(255, 0, 0, 255));
        _timerText->setOpacity(128);
        _padShape->SetDensity(10.0f);
        _body->ResetMassData();
        if (_glow) _glow->setVisible(false);
        createBodySound("PaddleBreak", _body, 1.0f, false);
    }

    b2Body* _body = nullptr;
    b2Fixture* _padShape = nullptr;
    b2Fixture* _blocker = nullptr;
    b2RevoluteJoint* _joint = nullptr;
    Node* _paddle = nullptr;
    Node* _base = nullptr;
    Sprite* _glow = nullptr;
    Label* _timerText = nullptr;
    int _direction = 1;
    float _tarAng = 0.0f;
    float _speed = 6.0f;
    bool _hit = false;
    bool _listening = false;
    bool _broken = false;
    int _delayCounter = 0;
    int _delayTotal = 0;
    std::string _delayString;
    misc::FlashClock _clock;
};

FlashSpecialRegistration s_reg(35, [] { return (LevelItem*)new (std::nothrow) Paddle(); });

}  // namespace

}  // namespace online
