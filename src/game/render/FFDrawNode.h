#pragma once

// FFDrawNode: a copy of the (pre-3.16) cocos2d::DrawNode, deriving from cocos2d::Node directly,
// extended with a fixed table of delegate-owned polygons whose triangles are re-transformed and
// re-tinted every frame (updateVerts) from their FFDrawNodeDelegate (ShapeItem/GroupItem art).
// Polygons are triangulated with an ear-clipping port of polypartition (PartitionVert).
// sizeof 0x902870 (arm64): the delegate table is a member array.

#include "2d/CCNode.h"
#include "base/ccTypes.h"
#include "math/CCAffineTransform.h"
#include "math/Vec2.h"
#include "renderer/CCCustomCommand.h"

class FFDrawNodeDelegate;

// Ear-clipping vertex (polypartition's PartitionVertex with float points), sizeof 0x28.
struct PartitionVert
{
    bool isActive;            // +0x00
    bool isConvex;            // +0x01
    bool isEar;               // +0x02
    cocos2d::Vec2 p;          // +0x04
    PartitionVert* previous;  // +0x10
    PartitionVert* next;      // +0x18
    float angle;              // +0x20
};

// One delegate's polygon inside FFDrawNode::_buffer (sizeof 0x1710). Type and field names from
// the iOS original (`ArtDelegate`, 2000 entries there). Copied around with memcpy(.., 0x1709),
// i.e. everything up to and including `update`.
struct ArtDelegate
{
    int triangleIndex;                             // +0x0000 first triangle in _buffer
    int triangleCount;                             // +0x0004 int (iOS); loops use unsigned indices
    cocos2d::V2F_C4B_T2F_Triangle triangles[98];   // +0x0008 local-space copy
    FFDrawNodeDelegate* delegate;                  // +0x1700
    bool update;                                   // +0x1708 re-transform every updateVerts()
};

class FFDrawNode : public cocos2d::Node
{
public:
    static FFDrawNode* create(float lineWidth);

    // ---- cocos2d::DrawNode API ----
    void drawPoint(const cocos2d::Vec2& point, const float pointSize, const cocos2d::Color4F& color);
    void drawPoints(const cocos2d::Vec2* position, unsigned int numberOfPoints,
                    const cocos2d::Color4F& color);
    void drawPoints(const cocos2d::Vec2* position, unsigned int numberOfPoints,
                    const float pointSize, const cocos2d::Color4F& color);
    void drawLine(const cocos2d::Vec2& origin, const cocos2d::Vec2& destination,
                  const cocos2d::Color4F& color);
    void drawRect(const cocos2d::Vec2& origin, const cocos2d::Vec2& destination,
                  const cocos2d::Color4F& color);
    void drawPoly(const cocos2d::Vec2* poli, unsigned int numberOfPoints, bool closePolygon,
                  const cocos2d::Color4F& color);
    void drawCircle(const cocos2d::Vec2& center, float radius, float angle, unsigned int segments,
                    bool drawLineToCenter, float scaleX, float scaleY,
                    const cocos2d::Color4F& color);
    void drawCircle(const cocos2d::Vec2& center, float radius, float angle, unsigned int segments,
                    bool drawLineToCenter, const cocos2d::Color4F& color);
    void drawQuadBezier(const cocos2d::Vec2& origin, const cocos2d::Vec2& control,
                        const cocos2d::Vec2& destination, unsigned int segments,
                        const cocos2d::Color4F& color);
    void drawCubicBezier(const cocos2d::Vec2& origin, const cocos2d::Vec2& control1,
                         const cocos2d::Vec2& control2, const cocos2d::Vec2& destination,
                         unsigned int segments, const cocos2d::Color4F& color);
    void drawDot(const cocos2d::Vec2& pos, float radius, const cocos2d::Color4F& color);
    void drawRect(const cocos2d::Vec2& p1, const cocos2d::Vec2& p2, const cocos2d::Vec2& p3,
                  const cocos2d::Vec2& p4, const cocos2d::Color4F& color);
    void drawSegment(const cocos2d::Vec2& from, const cocos2d::Vec2& to, float radius,
                     const cocos2d::Color4F& color);
    void drawPolygon(const cocos2d::Vec2* verts, int count, const cocos2d::Color4F& fillColor,
                     float borderWidth, const cocos2d::Color4F& borderColor);
    void drawSolidRect(const cocos2d::Vec2& origin, const cocos2d::Vec2& destination,
                       const cocos2d::Color4F& color);
    void drawSolidPoly(const cocos2d::Vec2* poli, unsigned int numberOfPoints,
                       const cocos2d::Color4F& color);
    void drawSolidCircle(const cocos2d::Vec2& center, float radius, float angle,
                         unsigned int segments, float scaleX, float scaleY,
                         const cocos2d::Color4F& color);
    void drawSolidCircle(const cocos2d::Vec2& center, float radius, float angle,
                         unsigned int segments, const cocos2d::Color4F& color);
    void drawTriangle(const cocos2d::Vec2& p1, const cocos2d::Vec2& p2, const cocos2d::Vec2& p3,
                      const cocos2d::Color4F& color);
    void drawQuadraticBezier(const cocos2d::Vec2& from, const cocos2d::Vec2& control,
                             const cocos2d::Vec2& to, unsigned int segments,
                             const cocos2d::Color4F& color);  // tail call to drawQuadBezier
    void clear();
    const cocos2d::BlendFunc& getBlendFunc() const;
    void setBlendFunc(const cocos2d::BlendFunc& blendFunc);
    void setLineWidth(float lineWidth);
    float getLineWidth();

    virtual void onDraw(const cocos2d::Mat4& transform, uint32_t flags);        // +0x528
    virtual void onDrawGLLine(const cocos2d::Mat4& transform, uint32_t flags);  // +0x530
    virtual void onDrawGLPoint(const cocos2d::Mat4& transform, uint32_t flags); // +0x538
    virtual void draw(cocos2d::Renderer* renderer, const cocos2d::Mat4& transform,
                      uint32_t flags) override;

    // ---- FFDrawNode additions ----
    // Triangulates a polygon (max 100 verts) into _buffer; returns the number of vertices added.
    int drawPolyWithVerts(cocos2d::Vec2* verts, int count, cocos2d::Color4F fillColor,
                          double borderWidth, cocos2d::Color4F borderColor);
    void updateVertex(PartitionVert* vertex, PartitionVert* vertices, int numVertices);
    void drawDotWithOffset(cocos2d::Vec2 position, cocos2d::Vec2 offset, float radius,
                           cocos2d::Color4F color, float borderWidth, cocos2d::Color4F borderColor,
                           bool updateArt, FFDrawNodeDelegate* artDelegate);
    bool isConvex(cocos2d::Vec2 p1, cocos2d::Vec2 p2, cocos2d::Vec2 p3);
    bool isInside(cocos2d::Vec2 p1, cocos2d::Vec2 p2, cocos2d::Vec2 p3, cocos2d::Vec2 p);
    bool compareTransforms(cocos2d::AffineTransform t1, cocos2d::AffineTransform t2);
    // Adds a delegate polygon: verts transformed by `transform` into _buffer, a local copy kept
    // (updateArt: re-transformed each frame by the delegate's art transform).
    void drawPolyWithVerts(cocos2d::Vec2* verts, int count, cocos2d::Color4F fillColor,
                           double borderWidth, cocos2d::Color4F borderColor, bool updateArt,
                           cocos2d::AffineTransform transform,
                           cocos2d::AffineTransform artTransform,
                           FFDrawNodeDelegate* artDelegate);
    void removeDelegate(FFDrawNodeDelegate* artDelegate);
    void replaceDelegate(FFDrawNodeDelegate* oldDelegate, FFDrawNodeDelegate* newDelegate);
    void setArtDelegateToStatic(bool isStatic, cocos2d::AffineTransform transform,
                                FFDrawNodeDelegate* artDelegate);
    void updateVerts();  // called by Session::update after the physics step

CC_CONSTRUCTOR_ACCESS:
    FFDrawNode(float lineWidth);
    virtual ~FFDrawNode();
    virtual bool init() override;

protected:
    void ensureCapacity(int count);
    void ensureCapacityGLPoint(int count);
    void ensureCapacityGLLine(int count);

    GLuint _vao;                              // +0x2f8
    GLuint _vbo;                              // +0x2fc
    GLuint _vaoGLPoint;                       // +0x300
    GLuint _vboGLPoint;                       // +0x304
    GLuint _vaoGLLine;                        // +0x308
    GLuint _vboGLLine;                        // +0x30c
    int _bufferCapacity;                      // +0x310
    GLsizei _bufferCount;                     // +0x314
    cocos2d::V2F_C4B_T2F* _buffer;            // +0x318
    int _bufferCapacityGLPoint;               // +0x320
    GLsizei _bufferCountGLPoint;              // +0x324
    cocos2d::V2F_C4B_T2F* _bufferGLPoint;     // +0x328
    cocos2d::Color4F _pointColor;             // +0x330
    int _pointSize;                           // +0x340
    int _bufferCapacityGLLine;                // +0x344
    GLsizei _bufferCountGLLine;               // +0x348
    cocos2d::V2F_C4B_T2F* _bufferGLLine;      // +0x350
    cocos2d::BlendFunc _blendFunc;            // +0x358
    cocos2d::CustomCommand _customCommand;        // +0x360
    cocos2d::CustomCommand _customCommandGLPoint; // +0x3b0
    cocos2d::CustomCommand _customCommandGLLine;  // +0x400
    bool _dirty;                              // +0x450
    bool _dirtyGLPoint;                       // +0x451
    bool _dirtyGLLine;                        // +0x452
    float _lineWidth;                         // +0x454
    float _defaultLineWidth;                  // +0x458
    ArtDelegate _artDelegates[1600];          // +0x460
    unsigned int _artDelegateCount;           // +0x902860

private:
    CC_DISALLOW_COPY_AND_ASSIGN(FFDrawNode);
};
