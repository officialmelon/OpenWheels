// RESTORED (PC addition): see FlashParticles.h.

#include "FlashParticles.h"

#include <cmath>

#include "EmitterNode.h"
#include "Session.h"

USING_NS_CC;

namespace restored {

namespace {

const char* kName = "restored_flash_particles";
const float kScaler = 62.5f / 30.0f;                 // Flash Particle/Spark.scaler
const float kGravity = 1.0f / 3.0f * 62.5f / 30.0f;  // Flash px per frame^2
const float kMetresPerFlashPx = 1.0f / 62.5f;  // Flash m_physScale
const float kSparkDecay = 0.65f;

float random01()
{
    return CCRANDOM_0_1();
}

}  // namespace

FlashParticles* FlashParticles::forSession(Session* session)
{
    if (!session) {
        return nullptr;
    }
    Node* parent = session->getParticlesForeground();
    if (!parent) {
        parent = session;
    }
    FlashParticles* particles = dynamic_cast<FlashParticles*>(parent->getChildByName(kName));
    if (!particles) {
        particles = new (std::nothrow) FlashParticles();
        particles->_session = session;
        if (!particles->init()) {
            delete particles;
            return nullptr;
        }
        particles->autorelease();
        particles->setName(kName);
        parent->addChild(particles, 100);
    }
    return particles;
}

bool FlashParticles::init()
{
    if (!Node::init()) {
        return false;
    }
    _sparkNode = DrawNode::create();
    addChild(_sparkNode, 1);
    scheduleUpdate();
    return true;
}

float FlashParticles::pointsPerFlashPx() const
{
    return kMetresPerFlashPx * _session->getPtmRatio();
}

void FlashParticles::burst(const std::vector<std::string>& frames, b2Vec2 position, b2Vec2 velocity,
                           float initialRange, float speedRange, int count)
{
    if (frames.empty()) {
        return;
    }
    float k = pointsPerFlashPx();
    float ptm = _session->getPtmRatio();
    Vec2 start(position.x * ptm, position.y * ptm);
    // Burst2 adds the body's velocity (m/s, Flash y down) to the particle speed before the
    // Particle.scaler.
    float vx = velocity.x;
    float vy = -velocity.y;
    for (int i = 0; i < count; i++) {
        const std::string& frame = frames[(size_t)(random01() * frames.size()) % frames.size()];
        Sprite* sprite = Sprite::createWithSpriteFrameName(frame);
        if (!sprite) {
            continue;
        }
        // Flash Burst.createParticle (y down): x speed rand*(rand*range - range/2), y speed
        // rand*(rand*range - 3/4 range), times Particle.scaler; per 30 Hz frame.
        float sx = (vx + random01() * (random01() * speedRange - speedRange * 0.5f)) * kScaler;
        float sy = (vy + random01() * (random01() * speedRange - speedRange * 0.75f)) * kScaler;
        Vec2 v(sx * k * 0.5f, -sy * k * 0.5f);
        Vec2 p = start + Vec2((random01() * initialRange - initialRange * 0.5f) * k,
                              -(random01() * initialRange - initialRange * 0.5f) * k);
        sprite->setPosition(p);
        addChild(sprite, 0);
        _pieces.push_back({sprite, v, start.y - 40.0f * ptm});
    }
}

void FlashParticles::sparks(b2Vec2 position, b2Vec2 startVel, float initialRange, float speedRange, int count)
{
    float ptm = _session->getPtmRatio();
    float k = pointsPerFlashPx();
    Vec2 start(position.x * ptm, position.y * ptm);
    for (int i = 0; i < count; i++) {
        // Flash SparkBurstPoint.createParticle / Spark (Flash px per frame, y down).
        float sx = (startVel.x + random01() * (random01() * speedRange - speedRange * 0.5f)) * kScaler;
        float sy = (startVel.y + random01() * (random01() * speedRange - speedRange * 0.5f)) * kScaler;
        Spark s;
        s.position = start + Vec2((random01() * initialRange - initialRange * 0.5f) * k,
                                  -(random01() * initialRange - initialRange * 0.5f) * k);
        s.speed = Vec2(sx, sy);
        s.gravity = 0.0f;
        s.color = Color4F(Color4B(255, 255, (GLubyte)(random01() * 255.0f), 255));
        s.phase = 0;
        _sparks.push_back(s);
    }
}

void FlashParticles::snow(const std::vector<std::string>& frames, b2Vec2 position, b2Vec2 velocity)
{
    if (frames.empty() || _pieces.size() > 600) {
        return;
    }
    Sprite* sprite = Sprite::createWithSpriteFrameName(frames[(size_t)(random01() * frames.size()) % frames.size()]);
    if (!sprite) {
        return;
    }
    float ptm = _session->getPtmRatio();
    float k = pointsPerFlashPx();
    // Flash: speed = body velocity (Flash m/s) x Particle.scaler px per frame.
    Vec2 v(velocity.x * kScaler * k * 0.5f, velocity.y * kScaler * k * 0.5f);
    sprite->setPosition(Vec2(position.x * ptm, position.y * ptm));
    addChild(sprite, 0);
    Piece piece{sprite, v, position.y * ptm - 40.0f * ptm};
    piece.snow = true;
    _pieces.push_back(piece);
}

void FlashParticles::update(float dt)
{
    _accumulator += dt;
    if (_accumulator > 0.25f) {
        _accumulator = 0.25f;
    }
    float k = pointsPerFlashPx();
    float gravityPerStep = kGravity * k * 0.25f;  // (px/frame^2) -> points/step^2
    while (_accumulator >= 1.0f / 60.0f) {
        _accumulator -= 1.0f / 60.0f;
        for (int i = (int)_pieces.size() - 1; i >= 0; i--) {
            Piece& p = _pieces[i];
            if (p.snow) {
                // SnowFlake: half gravity, slowed above 20 px per frame
                p.velocity.y -= gravityPerStep * 0.5f;
                float cap = 20.0f * k * 0.5f;
                if (-p.velocity.y > cap) {
                    p.velocity.y *= 0.9f;
                }
                if (p.velocity.x > cap) {
                    p.velocity.x *= 0.9f;
                }
            } else {
                p.velocity.y -= gravityPerStep;
            }
            p.sprite->setPosition(p.sprite->getPosition() + p.velocity);
            if (p.sprite->getPositionY() < p.limitY) {
                p.sprite->removeFromParent();
                _pieces.erase(_pieces.begin() + i);
            }
        }
        // Sparks step at the Flash rate (every other step).
        for (int i = (int)_sparks.size() - 1; i >= 0; i--) {
            Spark& s = _sparks[i];
            if (++s.phase % 2) {
                continue;
            }
            float changeY = s.speed.y + s.gravity;
            s.position += Vec2(s.speed.x * k, -changeY * k);
            s.speed *= kSparkDecay;
            s.gravity += kGravity;
            if (std::fabs(s.speed.y) < 1.0f && std::fabs(s.speed.x) < 1.0f) {
                _sparks.erase(_sparks.begin() + i);
            }
        }
    }
    _sparkNode->clear();
    for (const Spark& s : _sparks) {
        Vec2 tail = s.position - Vec2(s.speed.x * k, -(s.speed.y + s.gravity) * k);
        _sparkNode->drawSegment(tail, s.position, std::max(0.6f, k * 0.5f), s.color);
    }
}

}  // namespace restored
