// ONLINE (PC addition): browser special 33, Cannon (Flash userspecials/Cannon + editor CannonRef).
// The Android game has no cannon.
//
// XML (CannonRef._attributes): p0 x, p1 y, p2 angle, p3 startRotation -90..90,
// p4 firingRotation -90..90, p5 cannonType 1..2, p6 cannonDelay 1..10 s, p7 muzzleScale 1..10,
// p8 cannonPower 1..10.
//
// A barrel (dynamic body pinned to the level, limits locked) on a static base. Anything entering
// the barrel's sensor starts the loading meter (the star spins on type 1); after cannonDelay the
// barrel swings to the firing angle (motor, 0.6 rad/s), fires every body inside it (impulse
// along the barrel, (1.5 + power x 0.4) x mass x 10, muzzle flare, Cannon1 sound) and swings
// back. If everything leaves while aiming it swings back without firing.

#include <map>

#include "online/items/FlashSpecials.h"
#include "online/items/MiscItemUtil.h"

#include "LevelB2D.h"
#include "LevelDataElement.h"

USING_NS_CC;

namespace online {

namespace {

const float kDeg = 0.017453292519943295f;
// CannonShapesRefMC (Flash px, y down).
const b2Vec2 kP0[4] = {{-29.0f, -171.15f}, {-22.0f, -171.15f}, {-13.5f, 0.0f}, {-32.5f, 0.0f}};
const b2Vec2 kP1[4] = {{22.0f, -171.15f}, {29.0f, -171.15f}, {32.5f, 0.0f}, {13.5f, 0.0f}};
const b2Vec2 kBase[4] = {{-37.0f, 20.0f}, {37.0f, 20.0f}, {49.0f, 55.0f}, {-49.0f, 55.0f}};
const float kBaseRadius = 32.4969f;  // base.width / 2
// CannonBaseMC.star frames: rotation (degrees, clockwise).
const float kStarRot[8] = {0.0f, 9.06f, 18.3f, 27.5f, 36.8f, 46.0f, 55.2f, 64.5f};

enum class State { Waiting, Loading, Aiming, Returning, Firing };

class Cannon : public FlashItem
{
public:
    ~Cannon() override
    {
        for (Node* n : {_muzzleMC, _baseMC}) {
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
        const float startRot = (float)std::max(-90, std::min(90, inum(element, "p3", 0)));
        const float firingRot = (float)std::max(-90, std::min(90, inum(element, "p4", 0)));
        const int type = std::max(1, std::min(2, inum(element, "p5", 1)));
        const int delay = std::max(1, std::min(10, inum(element, "p6", 1)));
        const int muzzleScale = std::max(1, std::min(10, inum(element, "p7", 1)));
        const int power = std::max(1, std::min(10, inum(element, "p8", 5)));
        _showStar = type == 1;
        _power = 1.5f + power / 10.0f * 4.0f;
        _loadingFrames = delay * 30;
        _startAngle = startRot * kDeg;
        _firingAngle = firingRot * kDeg;
        _scale = 1.0f + muzzleScale / 20.0f;

        // Art: muzzle, then base (above it), both in the foreground.
        Node* fg = flashForegroundLayer();
        _muzzleMC = misc::artOr("cannon_muzzle_" + std::to_string(type), 65.0f, 200.0f,
                                Color4F(0.75f, 0.3f, 0.2f, 1.0f), 0.0f, -85.0f);
        _muzzleMC->setScale(_muzzleMC->getScale() * _scale);
        _muzzleMC->retain();
        _baseMC = Node::create();
        _baseMC->retain();
        _baseMC->addChild(misc::artOr("cannon_base_" + std::to_string(type), 99.0f, 75.0f,
                                      Color4F(0.6f, 0.6f, 0.6f, 1.0f), 0.0f, 35.0f));
        if (Sprite* bg = createFlashSprite("cannon_meter_bg")) {
            bg->setPosition(misc::localPx(0.0f, 37.0f));
            _baseMC->addChild(bg);
            if ((_bar = createFlashSprite("cannon_meter_bar"))) {
                _barScale = _bar->getScale();
                _bar->setPosition(misc::localPx(-22.5f, 37.0f));
                _bar->setScaleY(_barScale * 2.0f);
                _bar->setVisible(false);
                _baseMC->addChild(_bar);
            }
        }
        if (_showStar && (_star = createFlashSprite("cannon_star"))) _baseMC->addChild(_star);
        if (fg) {
            fg->addChild(_muzzleMC);
            fg->addChild(_baseMC);
        }
        misc::placeFlash(_muzzleMC, x, y, rotation + startRot);
        misc::placeFlash(_baseMC, x, y, rotation);

        // Barrel body.
        b2World* world = getWorld();
        b2BodyDef bd;
        bd.type = b2_dynamicBody;
        bd.position = flashToWorld(x, y);
        bd.angle = flashAngle(startRot + rotation);
        bd.allowSleep = false;
        _muzzleBody = world->CreateBody(&bd);
        const float k = _scale / kFlashPtm;
        b2CircleShape circle;
        circle.m_radius = kBaseRadius * k;
        b2FixtureDef fd;
        fd.shape = &circle;
        fd.friction = 0.5f;
        fd.restitution = 0.1f;
        fd.density = 1.0f;
        fd.filter.categoryBits = 8;
        _muzzleBody->CreateFixture(&fd);
        auto quad = [&](const b2Vec2* p, float scale) {
            b2Vec2 v[4];
            for (int i = 0; i < 4; i++) v[i] = b2Vec2(p[i].x * scale, -p[i].y * scale);
            b2PolygonShape poly;
            poly.Set(v, 4);
            return poly;
        };
        b2PolygonShape wall0 = quad(kP0, k), wall1 = quad(kP1, k);
        fd.shape = &wall0;
        _muzzleBody->CreateFixture(&fd);
        fd.shape = &wall1;
        _muzzleBody->CreateFixture(&fd);
        const b2Vec2 inner[4] = {kP0[1], kP1[0], kP1[3], kP0[2]};
        b2PolygonShape sensorShape = quad(inner, k);
        fd.shape = &sensorShape;
        fd.isSensor = true;
        fd.density = 1e-7f;
        _sensor = _muzzleBody->CreateFixture(&fd);
        addToBeginContact(_sensor);
        addToEndContact(_sensor);
        getLevel()->addToPaintItem(this);

        // Base: on the level body, rotated with the cannon.
        {
            const float r = rotation * kDeg, c = std::cos(r), s = std::sin(r);
            b2Vec2 v[4];
            for (int i = 0; i < 4; i++) {
                const float fx = x + kBase[i].x * c - kBase[i].y * s;
                const float fy = y + kBase[i].x * s + kBase[i].y * c;
                v[i] = flashToWorld(fx, fy) - getLevelBody()->GetPosition();
            }
            b2PolygonShape base;
            base.Set(v, 4);
            b2FixtureDef bfd;
            bfd.shape = &base;
            bfd.friction = 0.5f;
            bfd.restitution = 0.1f;
            bfd.filter.categoryBits = 8;
            getLevelBody()->CreateFixture(&bfd);
        }

        b2RevoluteJointDef jd;
        jd.Initialize(getLevelBody(), _muzzleBody, flashToWorld(x, y));
        jd.enableLimit = true;
        jd.lowerAngle = 0.0f;
        jd.upperAngle = 0.0f;
        _joint = (b2RevoluteJoint*)world->CreateJoint(&jd);
        getLevel()->addToActions(this);
        return true;
    }

    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override
    {
        (void)fixture;
        (void)contact;
        _inCannon[otherFixture->GetBody()]++;
        if (_state == State::Waiting) {
            _state = State::Loading;
            _starPlaying = _showStar;
            if (_bar) {
                _bar->setVisible(true);
                _bar->setScaleX(0.0f);
            }
        }
    }

    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override
    {
        (void)fixture;
        (void)contact;
        auto it = _inCannon.find(otherFixture->GetBody());
        if (it == _inCannon.end()) return;
        if (--it->second <= 0) _inCannon.erase(it);
    }

    void actions() override
    {
        const int frames = _clock.advance(getTimeStepOverFlashTimeStep());
        for (int f = 0; f < frames; f++) flashFrame();
    }

    void paint() override
    {
        // Flash: the muzzle clip sits 2 px down the barrel from the body origin.
        const b2Vec2 p = _muzzleBody->GetWorldPoint(b2Vec2(0.0f, -2.0f / kFlashPtm));
        misc::paintFromBody(_muzzleMC, _muzzleBody, p);
    }

private:
    // Joint angle / motor in Flash's clockwise convention.
    float jointAngle() const { return -_joint->GetJointAngle(); }
    void setMotorSpeed(float flashSpeed) { _joint->SetMotorSpeed(-flashSpeed); }
    void setLimits(float lower, float upper) { _joint->SetLimits(-upper, -lower); }

    void setBar(float fraction)
    {
        if (_bar) _bar->setScaleX(_barScale * fraction);
    }

    void flashFrame()
    {
        if (_starPlaying && _star) {
            _starFrame = (_starFrame + 1) % 8;
            _star->setRotation(kStarRot[_starFrame]);
        }
        if (_state == State::Loading) {
            if (++_frameCounter == _loadingFrames) {
                _frameCounter = 0;
                setBar(1.0f);
                moveMuzzleToFire();
            } else {
                setBar((float)_frameCounter / _loadingFrames);
            }
        } else if (_state == State::Aiming) {
            int total = 0;
            for (auto& e : _inCannon) total += e.second;
            if (total == 0) {
                moveMuzzleToStart();
            } else if (std::fabs(_firingAngle - _startAngle - jointAngle()) <= kDeg) {
                fireCannon();
                _starPlaying = false;
            }
        } else if (_state == State::Firing) {
            if (++_frameCounter == 5) {
                _frameCounter = 0;
                moveMuzzleToStart();
            }
        } else if (_state == State::Returning) {
            if (std::fabs(jointAngle()) <= kDeg) {
                _state = State::Waiting;
                _joint->EnableMotor(false);
                setLimits(0.0f, 0.0f);
            }
        }
    }

    void moveMuzzleToFire()
    {
        _state = State::Aiming;
        float upper, lower, speed;
        if (_startAngle < _firingAngle) {
            speed = 0.6f;
            lower = 0.0f;
            upper = _firingAngle - _startAngle;
        } else {
            speed = -0.6f;
            lower = _firingAngle - _startAngle;
            upper = 0.0f;
        }
        setLimits(lower, upper);
        _joint->EnableLimit(true);
        _joint->EnableMotor(true);
        _joint->SetMaxMotorTorque(10000.0f);
        setMotorSpeed(speed);
    }

    void moveMuzzleToStart()
    {
        setBar(0.0f);
        _state = State::Returning;
        setMotorSpeed(_startAngle < _firingAngle ? -0.6f : 0.6f);
    }

    void fireCannon()
    {
        _state = State::Firing;
        // Muzzle flare (MuzzleFlare, 23 frames, removes itself) behind the level art.
        if (hasFlashArt("muzzle_flare_1")) {
            if (Node* layer = flashBackgroundLayer()) {
                Sprite* flare = createFlashSprite("muzzle_flare_1");
                flare->setScale(flare->getScale() * _scale);
                flare->setPosition(_muzzleMC->getPosition());
                flare->setRotation(_muzzleMC->getRotation());
                layer->addChild(flare, -1);
                Vector<SpriteFrame*> frames;
                for (int i = 1; i <= 23; i++) {
                    auto* tex = Director::getInstance()->getTextureCache()->addImage(
                        "generated/flash/muzzle_flare_" + std::to_string(i) + ".png");
                    if (tex) frames.pushBack(SpriteFrame::createWithTexture(tex, Rect(Vec2::ZERO, tex->getContentSize())));
                }
                if (!frames.empty()) {
                    auto* anim = Animation::createWithSpriteFrames(frames, 1.0f / 30.0f);
                    flare->runAction(Sequence::create(Animate::create(anim), RemoveSelf::create(), nullptr));
                }
            }
        }
        const float a = -_muzzleBody->GetAngle();  // Flash angle
        const float s = std::sin(a), c = std::cos(a);
        for (auto& e : _inCannon) {
            b2Body* body = e.first;
            const float m = body->GetMass();
            // Flash (sin a, -cos a) x power x mass x 10 (y down) -> world (sin a, cos a).
            body->ApplyLinearImpulse(b2Vec2(s * (_power * m * 10.0f), c * (_power * m * 10.0f)),
                                     body->GetWorldCenter(), true);
        }
        createBodySound("Cannon1", _muzzleBody, 1.0f, false);
    }

    b2Body* _muzzleBody = nullptr;
    b2Fixture* _sensor = nullptr;
    b2RevoluteJoint* _joint = nullptr;
    Node* _muzzleMC = nullptr;
    Node* _baseMC = nullptr;
    Sprite* _bar = nullptr;
    float _barScale = 1.0f;
    Sprite* _star = nullptr;
    bool _showStar = true;
    bool _starPlaying = false;
    int _starFrame = 0;
    std::map<b2Body*, int> _inCannon;
    State _state = State::Waiting;
    int _frameCounter = 0;
    int _loadingFrames = 30;
    float _startAngle = 0.0f;
    float _firingAngle = 0.0f;
    float _power = 3.5f;
    float _scale = 1.05f;
    misc::FlashClock _clock;
};

FlashSpecialRegistration s_reg(33, [] { return (LevelItem*)new (std::nothrow) Cannon(); });

}  // namespace

}  // namespace online
