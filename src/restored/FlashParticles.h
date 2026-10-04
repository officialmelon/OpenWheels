#pragma once

// RESTORED (PC addition): the browser game's bitmap bursts and sparks (Flash particles/Burst,
// Burst2, Particle, SparkBurstPoint, Spark), which the restored characters use for their debris
// (Irresponsible Mom's basket pieces, the Explorer's cart shards) and wheel sparks. The mobile
// port's BurstEmitter only draws tinted squares, so these are drawn here with the pieces' own art.
//
// Motion follows the Flash classes exactly, converted to mobile units: a Flash particle moves
// speed x 62.5/30 Flash px per 30 Hz frame and falls 1/3 x 62.5/30 px per frame^2 (Flash: 62.5 px per
// metre, the same metres as here; the mobile world steps at 60 Hz). Pieces live until they are well below
// where they started (Flash: Settings.YParticleLimit), sparks until they slow down.

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <string>
#include <vector>

class Session;

namespace restored {

class FlashParticles : public cocos2d::Node
{
public:
    // The session's instance (created on first use, a child of its foreground particles node).
    static FlashParticles* forSession(Session* session);

    // Flash Burst (point) / Burst2 (body): `count` random frames of `frames`, scattered over
    // initialRange Flash px, speeds from speedRange, plus `velocity` (m/s; Burst2: the body's).
    void burst(const std::vector<std::string>& frames, b2Vec2 position, b2Vec2 velocity,
               float initialRange, float speedRange, int count);
    // Flash SparkBurstPoint(position, startVel, initialRange, speedRange, count): startVel in
    // Flash units (as the Flash call sites pass it).
    void sparks(b2Vec2 position, b2Vec2 startVel, float initialRange, float speedRange, int count);
    // Flash SnowSpray / SnowFlake (speed 0): one flake at `position` moving with `velocity` (m/s),
    // falling at half a particle's rate and capped at 20 px per frame.
    void snow(const std::vector<std::string>& frames, b2Vec2 position, b2Vec2 velocity);

    void update(float dt) override;

private:
    struct Piece
    {
        cocos2d::Sprite* sprite;
        cocos2d::Vec2 velocity;  // points per step
        float limitY;            // removed below this (points)
        bool snow = false;
    };
    struct Spark
    {
        cocos2d::Vec2 position;  // points
        cocos2d::Vec2 speed;     // points per step, decays
        float gravity;           // accumulated fall speed
        cocos2d::Color4F color;
        int phase;               // Flash steps every other 60 Hz step
    };

    bool init() override;
    float pointsPerFlashPx() const;

    Session* _session = nullptr;
    std::vector<Piece> _pieces;
    std::vector<Spark> _sparks;
    cocos2d::DrawNode* _sparkNode = nullptr;
    float _accumulator = 0.0f;
};

}  // namespace restored
