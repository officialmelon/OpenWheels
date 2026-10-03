#pragma once

#include "Box2D/Box2D.h"
#include "cocos2d.h"

struct b2AABB;

// Box2D debug renderer (b2Draw subclass, arm64 sizeof 0x20) - the cocos2d-x 3.17 test-bed
// "GLES-Render" class as used by the game. Draws in Box2D units scaled by mRatio with the
// SHADER_NAME_POSITION_U_COLOR program. Owned by B2DebugDrawLayer.
//
// vtable: [0] b2Draw::~b2Draw (the destructor is implicit), [1] ~GLESDebugDraw (deleting), then
// DrawPolygon, DrawSolidPolygon, DrawCircle, DrawSolidCircle, DrawSegment, DrawTransform,
// DrawPoint (b2Draw overrides) and the two new virtuals DrawString, DrawAABB.
// Field names are the original test-bed ones.
class GLESDebugDraw : public b2Draw
{
    // b2Draw: vptr +0x00, m_drawFlags +0x08
    float32 mRatio;                              // +0x0c
    cocos2d::GLProgram* mShaderProgram;          // +0x10
    GLint mColorLocation;                        // +0x18

    void initShader();                                                         // @005bdff8

public:
    GLESDebugDraw();                                                           // @005bdfbc (ratio 1)
    GLESDebugDraw(float32 ratio);                                              // @005be144

    void DrawPolygon(const b2Vec2* vertices, int vertexCount, const b2Color& color) override; // @005be188
    void DrawSolidPolygon(const b2Vec2* vertices, int vertexCount, const b2Color& color) override; // @005be294
    void DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color) override; // @005be3e0
    void DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis,
                         const b2Color& color) override;                       // @005be518
    void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) override; // @005be6f8
    void DrawTransform(const b2Transform& xf) override;                        // @005be7f0
    void DrawPoint(const b2Vec2& p, float32 size, const b2Color& color) override; // @005be8dc
    virtual void DrawString(int x, int y, const char* string, ...);            // @005be9c0
    virtual void DrawAABB(b2AABB* aabb, const b2Color& color);                 // @005be9e4
};
