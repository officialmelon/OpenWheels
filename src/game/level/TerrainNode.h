#pragma once

// TerrainNode: renders the terrain polygons of a level. A copy of FFDrawNode (itself derived from
// an older cocos2d::DrawNode) kept as a separate class: same fields, same "art delegate" records
// and ear-clipping triangulation, but drawPolyWithVerts/updateVertex take `unsigned long` counts.
// cocos2d::Node subclass; sizeof 0x902870 (1600 inline ArtDelegate records!).
// Vtable: Node's, with ~TerrainNode, draw (+0x348), init (+0x4f8) overridden and three new
// virtuals onDraw (+0x528), onDrawGLLine (+0x530), onDrawGLPoint (+0x538).

#include <cstdint>

#include "2d/CCNode.h"
#include "base/ccTypes.h"
#include "math/CCAffineTransform.h"
#include "math/Vec2.h"
#include "platform/CCGL.h"
#include "renderer/CCCustomCommand.h"

namespace cocos2d {
class Mat4;
class Renderer;
}

class FFDrawNodeDelegate;
// Ear-clipping vertex (polypartition's PartitionVertex), sizeof 0x28:
// {bool isActive; bool isConvex; bool isEar; Vec2 p @4; PartitionVert* previous @0x10;
//  PartitionVert* next @0x18; float angle @0x20}. Shared with FFDrawNode::updateVertex (M5).
struct PartitionVert;

class TerrainNode : public cocos2d::Node
{
public:
    // One polygon drawn on behalf of an FFDrawNodeDelegate (iOS: struct ArtDelegate; 2000 records
    // there, 1600 here). sizeof 0x1710. Copied by assignment: memcpy of 0x1709 bytes (dsize).
    struct ArtDelegate
    {
        unsigned int triangleIndex;                   // +0x0000  first triangle in _buffer
        unsigned int triangleCount;                   // +0x0004
        cocos2d::V2F_C4B_T2F_Triangle triangles[98];  // +0x0008  untransformed copy of the art
        FFDrawNodeDelegate* delegate;                 // +0x1700
        bool update;                                  // +0x1708  re-transform every updateVerts()
    };

    explicit TerrainNode(float lineWidth);
    virtual ~TerrainNode();
    static TerrainNode* create(float lineWidth);

    void ensureCapacity(int count);
    void ensureCapacityGLPoint(int count);
    void ensureCapacityGLLine(int count);

    bool init() override;                                                        // +0x4f8
    void draw(cocos2d::Renderer* renderer, const cocos2d::Mat4& transform,
              uint32_t flags) override;                                          // +0x348
    virtual void onDraw(const cocos2d::Mat4& transform, uint32_t flags);         // +0x528
    virtual void onDrawGLLine(const cocos2d::Mat4& transform, uint32_t flags);   // +0x530
    virtual void onDrawGLPoint(const cocos2d::Mat4& transform, uint32_t flags);  // +0x538

    void clear();
    const cocos2d::BlendFunc& getBlendFunc() const;
    void setBlendFunc(const cocos2d::BlendFunc& blendFunc);
    void setLineWidth(float lineWidth);
    float getLineWidth();

    // Triangulates (ear clipping) into _buffer; returns the number of vertices added (3 * tris).
    int drawPolyWithVerts(cocos2d::Vec2* verts, unsigned long count, cocos2d::Color4F fillColor,
                          double borderWidth, cocos2d::Color4F borderColor);
    void updateVertex(PartitionVert* v, PartitionVert* vertices, unsigned long numVertices);
    bool isConvex(cocos2d::Vec2 p1, cocos2d::Vec2 p2, cocos2d::Vec2 p3);
    bool isInside(cocos2d::Vec2 p1, cocos2d::Vec2 p2, cocos2d::Vec2 p3, cocos2d::Vec2 p);
    bool compareTransforms(cocos2d::AffineTransform t1, cocos2d::AffineTransform t2);
    // Draws and records an ArtDelegate for `delegate`.
    void drawPolyWithVerts(cocos2d::Vec2* verts, unsigned long count, cocos2d::Color4F fillColor,
                           double borderWidth, cocos2d::Color4F borderColor, bool update,
                           cocos2d::AffineTransform initialTransform,
                           cocos2d::AffineTransform initialOffset, FFDrawNodeDelegate* delegate);
    void removeDelegate(FFDrawNodeDelegate* delegate);
    void replaceDelegate(FFDrawNodeDelegate* oldDelegate, FFDrawNodeDelegate* newDelegate);
    void setArtDelegateToStatic(bool isStatic, cocos2d::AffineTransform transform,
                                FFDrawNodeDelegate* delegate);
    void updateVerts();

protected:
    GLuint _vao;                                   // +0x2f8
    GLuint _vbo;                                   // +0x2fc
    GLuint _vaoGLPoint;                            // +0x300
    GLuint _vboGLPoint;                            // +0x304
    GLuint _vaoGLLine;                             // +0x308
    GLuint _vboGLLine;                             // +0x30c

    int _bufferCapacity;                           // +0x310
    GLsizei _bufferCount;                          // +0x314
    cocos2d::V2F_C4B_T2F* _buffer;                 // +0x318

    int _bufferCapacityGLPoint;                    // +0x320
    GLsizei _bufferCountGLPoint;                   // +0x324
    cocos2d::V2F_C4B_T2F* _bufferGLPoint;          // +0x328  (V2F_C4B_T2F, as in old DrawNode)
    cocos2d::Color4F _pointColor;                  // +0x330
    int _pointSize;                                // +0x340  never initialised

    int _bufferCapacityGLLine;                     // +0x344
    GLsizei _bufferCountGLLine;                    // +0x348
    cocos2d::V2F_C4B_T2F* _bufferGLLine;           // +0x350

    cocos2d::BlendFunc _blendFunc;                 // +0x358
    cocos2d::CustomCommand _customCommand;         // +0x360
    cocos2d::CustomCommand _customCommandGLPoint;  // +0x3b0
    cocos2d::CustomCommand _customCommandGLLine;   // +0x400

    bool _dirty;                                   // +0x450
    bool _dirtyGLPoint;                            // +0x451
    bool _dirtyGLLine;                             // +0x452

    float _lineWidth;                              // +0x454
    float _defaultLineWidth;                       // +0x458

    ArtDelegate _artDelegates[1600];               // +0x460
    unsigned int _artDelegateCount;                // +0x902860
};
