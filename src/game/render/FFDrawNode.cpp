// FFDrawNode: a copy of the pre-3.16 cocos2d::DrawNode (buffer setup inlined into init(), no
// _isolated / visit override) plus a table of FFDrawNodeDelegate polygons. Delegate polygons are
// triangulated by an ear-clipping port of polypartition (TPPLPartition::Triangulate_EC with
// UpdateVertex/IsConvex/IsInside as FFDrawNode members) and re-transformed / re-tinted every frame
// by updateVerts().
//
// The DrawNode part is the engine code (thirdparty/cocos2d-x/cocos/2d/CCDrawNode.cpp) and is kept
// identical to it, including its quirks (drawPoly does not set _dirtyGLLine, ...).

#include "FFDrawNode.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#include "FFDrawNodeDelegate.h"
#include "PolyFill.h"
#include "base/CCConfiguration.h"
#include "base/CCDirector.h"
#include "base/CCEventDispatcher.h"
#include "base/CCEventListenerCustom.h"
#include "base/CCEventType.h"
#include "renderer/CCGLProgram.h"
#include "renderer/CCGLProgramCache.h"
#include "renderer/CCGLProgramState.h"
#include "renderer/CCRenderer.h"
#include "renderer/ccGLStateCache.h"

USING_NS_CC;

namespace {

// ---- CCDrawNode.cpp vector helpers (copied with the DrawNode code) ----

inline Vec2 v2f(float x, float y)
{
    Vec2 ret(x, y);
    return ret;
}

inline Vec2 v2fadd(const Vec2& v0, const Vec2& v1)
{
    return v2f(v0.x + v1.x, v0.y + v1.y);
}

inline Vec2 v2fsub(const Vec2& v0, const Vec2& v1)
{
    return v2f(v0.x - v1.x, v0.y - v1.y);
}

inline Vec2 v2fmult(const Vec2& v, float s)
{
    return v2f(v.x * s, v.y * s);
}

inline Vec2 v2fperp(const Vec2& p0)
{
    return v2f(-p0.y, p0.x);
}

inline Vec2 v2fneg(const Vec2& p0)
{
    return v2f(-p0.x, -p0.y);
}

inline float v2fdot(const Vec2& p0, const Vec2& p1)
{
    return p0.x * p1.x + p0.y * p1.y;
}

inline Vec2 v2fnormalize(const Vec2& p)
{
    Vec2 r(p.x, p.y);
    r.normalize();
    return v2f(r.x, r.y);
}

inline Vec2 __v2f(const Vec2& v)
{
    return v2f(v.x, v.y);
}

inline Tex2F __t(const Vec2& v)
{
    return *(Tex2F*)&v;
}

// Single-precision CGPointApplyAffineTransform (the game does not use the engine's
// PointApplyAffineTransform, which is out of line and computes in double).
inline Vec2 applyArtTransform(const Vec2& p, const AffineTransform& t)
{
    return Vec2(t.a * p.x + t.c * p.y + t.tx, t.b * p.x + t.d * p.y + t.ty);
}

inline void applyArtTransform(V2F_C4B_T2F_Triangle& dst, const V2F_C4B_T2F_Triangle& src,
                              const AffineTransform& t)
{
    dst.a.vertices = applyArtTransform(src.a.vertices, t);
    dst.b.vertices = applyArtTransform(src.b.vertices, t);
    dst.c.vertices = applyArtTransform(src.c.vertices, t);
}

// PC addition: a record's local triangle i (the inline array, then ArtDelegate::moreTriangles).
const unsigned int kInlineTriangles =
    sizeof(ArtDelegate::triangles) / sizeof(V2F_C4B_T2F_Triangle);

inline V2F_C4B_T2F_Triangle& localTriangle(ArtDelegate& art, unsigned int i)
{
    if (i < kInlineTriangles)
    {
        return art.triangles[i];
    }
    if (art.moreTriangles.size() <= i - kInlineTriangles)
    {
        art.moreTriangles.resize(i - kInlineTriangles + 1);
    }
    return art.moreTriangles[i - kInlineTriangles];
}

inline const V2F_C4B_T2F_Triangle& localTriangle(const ArtDelegate& art, unsigned int i)
{
    return i < kInlineTriangles ? art.triangles[i] : art.moreTriangles[i - kInlineTriangles];
}

}  // namespace

// @005ae038
FFDrawNode::FFDrawNode(float lineWidth)
: _vao(0)
, _vbo(0)
, _vaoGLPoint(0)
, _vboGLPoint(0)
, _vaoGLLine(0)
, _vboGLLine(0)
, _bufferCapacity(0)
, _bufferCount(0)
, _buffer(nullptr)
, _bufferCapacityGLPoint(0)
, _bufferCountGLPoint(0)
, _bufferGLPoint(nullptr)
, _bufferCapacityGLLine(0)
, _bufferCountGLLine(0)
, _bufferGLLine(nullptr)
, _dirty(false)
, _dirtyGLPoint(false)
, _dirtyGLLine(false)
, _lineWidth(lineWidth)
, _defaultLineWidth(lineWidth)
, _artDelegates(1600)
, _artDelegateCount(0)
{
    // _pointSize is left uninitialised (never used), as in the original.
    _blendFunc = BlendFunc::ALPHA_PREMULTIPLIED;
}

// @005ae1c0
FFDrawNode::~FFDrawNode()
{
    free(_buffer);
    _buffer = nullptr;
    free(_bufferGLPoint);
    _bufferGLPoint = nullptr;
    free(_bufferGLLine);
    _bufferGLLine = nullptr;

    glDeleteBuffers(1, &_vbo);
    glDeleteBuffers(1, &_vboGLLine);
    glDeleteBuffers(1, &_vboGLPoint);
    _vbo = 0;
    _vboGLPoint = 0;
    _vboGLLine = 0;

    if (Configuration::getInstance()->supportsShareableVAO())
    {
        GL::bindVAO(0);
        glDeleteVertexArrays(1, &_vao);
        glDeleteVertexArrays(1, &_vaoGLLine);
        glDeleteVertexArrays(1, &_vaoGLPoint);
        _vao = _vaoGLLine = _vaoGLPoint = 0;
    }
}

// @005ae2fc
FFDrawNode* FFDrawNode::create(float lineWidth)
{
    FFDrawNode* ret = new (std::nothrow) FFDrawNode(lineWidth);
    if (ret && ret->init())
    {
        ret->autorelease();
    }
    else
    {
        CC_SAFE_DELETE(ret);
    }
    return ret;
}

// @005ae39c
void FFDrawNode::ensureCapacity(int count)
{
    if (_bufferCount + count > _bufferCapacity)
    {
        _bufferCapacity += MAX(_bufferCapacity, count);
        _buffer = (V2F_C4B_T2F*)realloc(_buffer, _bufferCapacity * sizeof(V2F_C4B_T2F));
    }
}

// @005ae3f0
void FFDrawNode::ensureCapacityGLPoint(int count)
{
    if (_bufferCountGLPoint + count > _bufferCapacityGLPoint)
    {
        _bufferCapacityGLPoint += MAX(_bufferCapacityGLPoint, count);
        _bufferGLPoint =
            (V2F_C4B_T2F*)realloc(_bufferGLPoint, _bufferCapacityGLPoint * sizeof(V2F_C4B_T2F));
    }
}

// @005ae444
void FFDrawNode::ensureCapacityGLLine(int count)
{
    if (_bufferCountGLLine + count > _bufferCapacityGLLine)
    {
        _bufferCapacityGLLine += MAX(_bufferCapacityGLLine, count);
        _bufferGLLine =
            (V2F_C4B_T2F*)realloc(_bufferGLLine, _bufferCapacityGLLine * sizeof(V2F_C4B_T2F));
    }
}

// @005ae498
bool FFDrawNode::init()
{
    _blendFunc = BlendFunc::ALPHA_PREMULTIPLIED;

    setGLProgramState(
        GLProgramState::getOrCreateWithGLProgramName(GLProgram::SHADER_NAME_POSITION_LENGTH_TEXTURE_COLOR));

    ensureCapacity(512);
    ensureCapacityGLPoint(64);
    ensureCapacityGLLine(256);

    if (Configuration::getInstance()->supportsShareableVAO())
    {
        glGenVertexArrays(1, &_vao);
        GL::bindVAO(_vao);
        glGenBuffers(1, &_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(V2F_C4B_T2F) * _bufferCapacity, _buffer, GL_STREAM_DRAW);
        // vertex
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_POSITION);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, vertices));
        // color
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_COLOR);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, colors));
        // texcoord
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_TEX_COORD);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_TEX_COORD, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, texCoords));

        glGenVertexArrays(1, &_vaoGLLine);
        GL::bindVAO(_vaoGLLine);
        glGenBuffers(1, &_vboGLLine);
        glBindBuffer(GL_ARRAY_BUFFER, _vboGLLine);
        glBufferData(GL_ARRAY_BUFFER, sizeof(V2F_C4B_T2F) * _bufferCapacityGLLine, _bufferGLLine,
                     GL_STREAM_DRAW);
        // vertex
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_POSITION);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, vertices));
        // color
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_COLOR);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, colors));
        // texcoord
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_TEX_COORD);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_TEX_COORD, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, texCoords));

        glGenVertexArrays(1, &_vaoGLPoint);
        GL::bindVAO(_vaoGLPoint);
        glGenBuffers(1, &_vboGLPoint);
        glBindBuffer(GL_ARRAY_BUFFER, _vboGLPoint);
        glBufferData(GL_ARRAY_BUFFER, sizeof(V2F_C4B_T2F) * _bufferCapacityGLPoint, _bufferGLPoint,
                     GL_STREAM_DRAW);
        // vertex
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_POSITION);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, vertices));
        // color
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_COLOR);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, colors));
        // Texture coord as pointsize
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_TEX_COORD);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_TEX_COORD, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, texCoords));

        GL::bindVAO(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    else
    {
        glGenBuffers(1, &_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(V2F_C4B_T2F) * _bufferCapacity, _buffer, GL_STREAM_DRAW);

        glGenBuffers(1, &_vboGLLine);
        glBindBuffer(GL_ARRAY_BUFFER, _vboGLLine);
        glBufferData(GL_ARRAY_BUFFER, sizeof(V2F_C4B_T2F) * _bufferCapacityGLLine, _bufferGLLine,
                     GL_STREAM_DRAW);

        glGenBuffers(1, &_vboGLPoint);
        glBindBuffer(GL_ARRAY_BUFFER, _vboGLPoint);
        glBufferData(GL_ARRAY_BUFFER, sizeof(V2F_C4B_T2F) * _bufferCapacityGLPoint, _bufferGLPoint,
                     GL_STREAM_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    CHECK_GL_ERROR_DEBUG();

    _dirty = true;
    _dirtyGLLine = true;
    _dirtyGLPoint = true;

    // Need to listen the event only when not use batchnode, because it will use VBO.
    // (The pre-3.16 engine code had this under CC_ENABLE_CACHE_TEXTURE_DATA, which is 1 on
    // Android; the binary registers it unconditionally, so it is kept unconditional here.)
    auto listener = EventListenerCustom::create(EVENT_RENDERER_RECREATED, [this](EventCustom* event) {
        // @005b3138  listen the event that renderer was recreated on Android/WP8
        this->init();
    });
    _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);

    return true;
}

// @005aea80
void FFDrawNode::draw(Renderer* renderer, const Mat4& transform, uint32_t flags)
{
    if (_bufferCount)
    {
        _customCommand.init(_globalZOrder, transform, flags);
        _customCommand.func = CC_CALLBACK_0(FFDrawNode::onDraw, this, transform, flags);
        renderer->addCommand(&_customCommand);
    }

    if (_bufferCountGLPoint)
    {
        _customCommandGLPoint.init(_globalZOrder, transform, flags);
        _customCommandGLPoint.func = CC_CALLBACK_0(FFDrawNode::onDrawGLPoint, this, transform, flags);
        renderer->addCommand(&_customCommandGLPoint);
    }

    if (_bufferCountGLLine)
    {
        _customCommandGLLine.init(_globalZOrder, transform, flags);
        _customCommandGLLine.func = CC_CALLBACK_0(FFDrawNode::onDrawGLLine, this, transform, flags);
        renderer->addCommand(&_customCommandGLLine);
    }
}

// @005aed8c
void FFDrawNode::onDraw(const Mat4& transform, uint32_t /*flags*/)
{
    getGLProgramState()->apply(transform);
    auto glProgram = this->getGLProgram();
    glProgram->setUniformLocationWith1f(glProgram->getUniformLocation("u_alpha"),
                                        _displayedOpacity / 255.0);
    GL::blendFunc(_blendFunc.src, _blendFunc.dst);

    if (_dirty)
    {
        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(V2F_C4B_T2F) * _bufferCapacity, _buffer, GL_STREAM_DRAW);

        _dirty = false;
    }
    if (Configuration::getInstance()->supportsShareableVAO())
    {
        GL::bindVAO(_vao);
    }
    else
    {
        GL::enableVertexAttribs(GL::VERTEX_ATTRIB_FLAG_POS_COLOR_TEX);

        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        // vertex
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, vertices));
        // color
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, colors));
        // texcoord
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_TEX_COORD, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, texCoords));
    }

    glDrawArrays(GL_TRIANGLES, 0, _bufferCount);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (Configuration::getInstance()->supportsShareableVAO())
    {
        GL::bindVAO(0);
    }

    CC_INCREMENT_GL_DRAWN_BATCHES_AND_VERTICES(1, _bufferCount);
    CHECK_GL_ERROR_DEBUG();
}

// @005aef98
void FFDrawNode::onDrawGLLine(const Mat4& transform, uint32_t /*flags*/)
{
    auto glProgram = GLProgramCache::getInstance()->getGLProgram(
        GLProgram::SHADER_NAME_POSITION_LENGTH_TEXTURE_COLOR);
    glProgram->use();
    glProgram->setUniformsForBuiltins(transform);
    glProgram->setUniformLocationWith1f(glProgram->getUniformLocation("u_alpha"),
                                        _displayedOpacity / 255.0);

    GL::blendFunc(_blendFunc.src, _blendFunc.dst);

    if (_dirtyGLLine)
    {
        glBindBuffer(GL_ARRAY_BUFFER, _vboGLLine);
        glBufferData(GL_ARRAY_BUFFER, sizeof(V2F_C4B_T2F) * _bufferCapacityGLLine, _bufferGLLine,
                     GL_STREAM_DRAW);
        _dirtyGLLine = false;
    }
    if (Configuration::getInstance()->supportsShareableVAO())
    {
        GL::bindVAO(_vaoGLLine);
    }
    else
    {
        glBindBuffer(GL_ARRAY_BUFFER, _vboGLLine);
        GL::enableVertexAttribs(GL::VERTEX_ATTRIB_FLAG_POS_COLOR_TEX);
        // vertex
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, vertices));
        // color
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, colors));
        // texcoord
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_TEX_COORD, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, texCoords));
    }

    glLineWidth(_lineWidth);
    glDrawArrays(GL_LINES, 0, _bufferCountGLLine);

    if (Configuration::getInstance()->supportsShareableVAO())
    {
        GL::bindVAO(0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    CC_INCREMENT_GL_DRAWN_BATCHES_AND_VERTICES(1, _bufferCountGLLine);

    CHECK_GL_ERROR_DEBUG();
}

// @005af26c
void FFDrawNode::onDrawGLPoint(const Mat4& transform, uint32_t /*flags*/)
{
    auto glProgram = GLProgramCache::getInstance()->getGLProgram(
        GLProgram::SHADER_NAME_POSITION_COLOR_TEXASPOINTSIZE);
    glProgram->use();
    glProgram->setUniformsForBuiltins(transform);
    glProgram->setUniformLocationWith1f(glProgram->getUniformLocation("u_alpha"),
                                        _displayedOpacity / 255.0);

    GL::blendFunc(_blendFunc.src, _blendFunc.dst);

    if (_dirtyGLPoint)
    {
        glBindBuffer(GL_ARRAY_BUFFER, _vboGLPoint);
        glBufferData(GL_ARRAY_BUFFER, sizeof(V2F_C4B_T2F) * _bufferCapacityGLPoint, _bufferGLPoint,
                     GL_STREAM_DRAW);

        _dirtyGLPoint = false;
    }

    if (Configuration::getInstance()->supportsShareableVAO())
    {
        GL::bindVAO(_vaoGLPoint);
    }
    else
    {
        glBindBuffer(GL_ARRAY_BUFFER, _vboGLPoint);
        GL::enableVertexAttribs(GL::VERTEX_ATTRIB_FLAG_POS_COLOR_TEX);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, vertices));
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, colors));
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_TEX_COORD, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, texCoords));
    }

    glDrawArrays(GL_POINTS, 0, _bufferCountGLPoint);

    if (Configuration::getInstance()->supportsShareableVAO())
    {
        GL::bindVAO(0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    CC_INCREMENT_GL_DRAWN_BATCHES_AND_VERTICES(1, _bufferCountGLPoint);
    CHECK_GL_ERROR_DEBUG();
}

// @005af538
void FFDrawNode::drawPoint(const Vec2& position, const float pointSize, const Color4F& color)
{
    ensureCapacityGLPoint(1);

    V2F_C4B_T2F* point = (V2F_C4B_T2F*)(_bufferGLPoint + _bufferCountGLPoint);
    V2F_C4B_T2F a = {position, Color4B(color), Tex2F(pointSize, 0)};
    *point = a;

    _bufferCountGLPoint += 1;
    _dirtyGLPoint = true;
}

// @005af628
void FFDrawNode::drawPoints(const Vec2* position, unsigned int numberOfPoints, const Color4F& color)
{
    drawPoints(position, numberOfPoints, 1.0, color);
}

// @005af73c
void FFDrawNode::drawPoints(const Vec2* position, unsigned int numberOfPoints,
                            const float pointSize, const Color4F& color)
{
    ensureCapacityGLPoint(numberOfPoints);

    V2F_C4B_T2F* point = (V2F_C4B_T2F*)(_bufferGLPoint + _bufferCountGLPoint);

    for (unsigned int i = 0; i < numberOfPoints; i++, point++)
    {
        V2F_C4B_T2F a = {position[i], Color4B(color), Tex2F(pointSize, 0)};
        *point = a;
    }

    _bufferCountGLPoint += numberOfPoints;
    _dirtyGLPoint = true;
}

// @005af84c
void FFDrawNode::drawLine(const Vec2& origin, const Vec2& destination, const Color4F& color)
{
    ensureCapacityGLLine(2);

    V2F_C4B_T2F* point = (V2F_C4B_T2F*)(_bufferGLLine + _bufferCountGLLine);

    V2F_C4B_T2F a = {origin, Color4B(color), Tex2F(0.0, 0.0)};
    V2F_C4B_T2F b = {destination, Color4B(color), Tex2F(0.0, 0.0)};

    *point = a;
    *(point + 1) = b;

    _bufferCountGLLine += 2;
    _dirtyGLLine = true;
}

// @005af974
void FFDrawNode::drawRect(const Vec2& origin, const Vec2& destination, const Color4F& color)
{
    drawLine(Vec2(origin.x, origin.y), Vec2(destination.x, origin.y), color);
    drawLine(Vec2(destination.x, origin.y), Vec2(destination.x, destination.y), color);
    drawLine(Vec2(destination.x, destination.y), Vec2(origin.x, destination.y), color);
    drawLine(Vec2(origin.x, destination.y), Vec2(origin.x, origin.y), color);
}

// @005afcf4
void FFDrawNode::drawPoly(const Vec2* poli, unsigned int numberOfPoints, bool closePolygon,
                          const Color4F& color)
{
    unsigned int vertex_count;
    if (closePolygon)
    {
        vertex_count = 2 * numberOfPoints;
        ensureCapacityGLLine(vertex_count);
    }
    else
    {
        vertex_count = 2 * (numberOfPoints - 1);
        ensureCapacityGLLine(vertex_count);
    }

    V2F_C4B_T2F* point = (V2F_C4B_T2F*)(_bufferGLLine + _bufferCountGLLine);

    unsigned int i = 0;
    for (; i < numberOfPoints - 1; i++)
    {
        V2F_C4B_T2F a = {poli[i], Color4B(color), Tex2F(0.0, 0.0)};
        V2F_C4B_T2F b = {poli[i + 1], Color4B(color), Tex2F(0.0, 0.0)};

        *point = a;
        *(point + 1) = b;
        point += 2;
    }
    if (closePolygon)
    {
        V2F_C4B_T2F a = {poli[i], Color4B(color), Tex2F(0.0, 0.0)};
        V2F_C4B_T2F b = {poli[0], Color4B(color), Tex2F(0.0, 0.0)};
        *point = a;
        *(point + 1) = b;
    }

    // Engine quirk kept: _dirtyGLLine is not set here.
    _bufferCountGLLine += vertex_count;
}

// @005afecc
void FFDrawNode::drawCircle(const Vec2& center, float radius, float angle, unsigned int segments,
                            bool drawLineToCenter, float scaleX, float scaleY, const Color4F& color)
{
    const float coef = 2.0f * (float)M_PI / segments;

    Vec2* vertices = new (std::nothrow) Vec2[segments + 2];
    if (!vertices)
        return;

    for (unsigned int i = 0; i <= segments; i++)
    {
        float rads = i * coef;
        GLfloat j = radius * cosf(rads + angle) * scaleX + center.x;
        GLfloat k = radius * sinf(rads + angle) * scaleY + center.y;

        vertices[i].x = j;
        vertices[i].y = k;
    }
    if (drawLineToCenter)
    {
        vertices[segments + 1].x = center.x;
        vertices[segments + 1].y = center.y;
        drawPoly(vertices, segments + 2, true, color);
    }
    else
        drawPoly(vertices, segments + 1, true, color);

    CC_SAFE_DELETE_ARRAY(vertices);
}

// @005b01a8
void FFDrawNode::drawCircle(const Vec2& center, float radius, float angle, unsigned int segments,
                            bool drawLineToCenter, const Color4F& color)
{
    drawCircle(center, radius, angle, segments, drawLineToCenter, 1.0f, 1.0f, color);
}

// @005b0460
void FFDrawNode::drawQuadBezier(const Vec2& origin, const Vec2& control, const Vec2& destination,
                                unsigned int segments, const Color4F& color)
{
    Vec2* vertices = new (std::nothrow) Vec2[segments + 1];
    if (!vertices)
        return;

    float t = 0.0f;
    for (unsigned int i = 0; i < segments; i++)
    {
        vertices[i].x = powf(1 - t, 2) * origin.x + 2.0f * (1 - t) * t * control.x + t * t * destination.x;
        vertices[i].y = powf(1 - t, 2) * origin.y + 2.0f * (1 - t) * t * control.y + t * t * destination.y;
        t += 1.0f / segments;
    }
    vertices[segments].x = destination.x;
    vertices[segments].y = destination.y;

    drawPoly(vertices, segments + 1, false, color);

    CC_SAFE_DELETE_ARRAY(vertices);
}

// @005b0588
void FFDrawNode::drawCubicBezier(const Vec2& origin, const Vec2& control1, const Vec2& control2,
                                 const Vec2& destination, unsigned int segments, const Color4F& color)
{
    Vec2* vertices = new (std::nothrow) Vec2[segments + 1];
    if (!vertices)
        return;

    float t = 0;
    for (unsigned int i = 0; i < segments; i++)
    {
        vertices[i].x = powf(1 - t, 3) * origin.x + 3.0f * powf(1 - t, 2) * t * control1.x +
                        3.0f * (1 - t) * t * t * control2.x + t * t * t * destination.x;
        vertices[i].y = powf(1 - t, 3) * origin.y + 3.0f * powf(1 - t, 2) * t * control1.y +
                        3.0f * (1 - t) * t * t * control2.y + t * t * t * destination.y;
        t += 1.0f / segments;
    }
    vertices[segments].x = destination.x;
    vertices[segments].y = destination.y;

    drawPoly(vertices, segments + 1, false, color);

    CC_SAFE_DELETE_ARRAY(vertices);
}

// @005b0710
void FFDrawNode::drawDot(const Vec2& pos, float radius, const Color4F& color)
{
    unsigned int vertex_count = 2 * 3;
    ensureCapacity(vertex_count);

    V2F_C4B_T2F a = {Vec2(pos.x - radius, pos.y - radius), Color4B(color), Tex2F(-1.0, -1.0)};
    V2F_C4B_T2F b = {Vec2(pos.x - radius, pos.y + radius), Color4B(color), Tex2F(-1.0, 1.0)};
    V2F_C4B_T2F c = {Vec2(pos.x + radius, pos.y + radius), Color4B(color), Tex2F(1.0, 1.0)};
    V2F_C4B_T2F d = {Vec2(pos.x + radius, pos.y - radius), Color4B(color), Tex2F(1.0, -1.0)};

    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)(_buffer + _bufferCount);
    V2F_C4B_T2F_Triangle triangle0 = {a, b, c};
    V2F_C4B_T2F_Triangle triangle1 = {a, c, d};
    triangles[0] = triangle0;
    triangles[1] = triangle1;

    _bufferCount += vertex_count;

    _dirty = true;
}

// @005b0918
void FFDrawNode::drawRect(const Vec2& p1, const Vec2& p2, const Vec2& p3, const Vec2& p4,
                          const Color4F& color)
{
    drawLine(Vec2(p1.x, p1.y), Vec2(p2.x, p2.y), color);
    drawLine(Vec2(p2.x, p2.y), Vec2(p3.x, p3.y), color);
    drawLine(Vec2(p3.x, p3.y), Vec2(p4.x, p4.y), color);
    drawLine(Vec2(p4.x, p4.y), Vec2(p1.x, p1.y), color);
}

// @005b0c90
void FFDrawNode::drawSegment(const Vec2& from, const Vec2& to, float radius, const Color4F& color)
{
    unsigned int vertex_count = 6 * 3;
    ensureCapacity(vertex_count);

    Vec2 a = __v2f(from);
    Vec2 b = __v2f(to);

    Vec2 n = v2fnormalize(v2fperp(v2fsub(b, a)));
    Vec2 t = v2fperp(n);

    Vec2 nw = v2fmult(n, radius);
    Vec2 tw = v2fmult(t, radius);
    Vec2 v0 = v2fsub(b, v2fadd(nw, tw));
    Vec2 v1 = v2fadd(b, v2fsub(nw, tw));
    Vec2 v2 = v2fsub(b, nw);
    Vec2 v3 = v2fadd(b, nw);
    Vec2 v4 = v2fsub(a, nw);
    Vec2 v5 = v2fadd(a, nw);
    Vec2 v6 = v2fsub(a, v2fsub(nw, tw));
    Vec2 v7 = v2fadd(a, v2fadd(nw, tw));

    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)(_buffer + _bufferCount);

    V2F_C4B_T2F_Triangle triangles0 = {
        {v0, Color4B(color), __t(v2fneg(v2fadd(n, t)))},
        {v1, Color4B(color), __t(v2fsub(n, t))},
        {v2, Color4B(color), __t(v2fneg(n))},
    };
    triangles[0] = triangles0;

    V2F_C4B_T2F_Triangle triangles1 = {
        {v3, Color4B(color), __t(n)},
        {v1, Color4B(color), __t(v2fsub(n, t))},
        {v2, Color4B(color), __t(v2fneg(n))},
    };
    triangles[1] = triangles1;

    V2F_C4B_T2F_Triangle triangles2 = {
        {v3, Color4B(color), __t(n)},
        {v4, Color4B(color), __t(v2fneg(n))},
        {v2, Color4B(color), __t(v2fneg(n))},
    };
    triangles[2] = triangles2;

    V2F_C4B_T2F_Triangle triangles3 = {
        {v3, Color4B(color), __t(n)},
        {v4, Color4B(color), __t(v2fneg(n))},
        {v5, Color4B(color), __t(n)},
    };
    triangles[3] = triangles3;

    V2F_C4B_T2F_Triangle triangles4 = {
        {v6, Color4B(color), __t(v2fsub(t, n))},
        {v4, Color4B(color), __t(v2fneg(n))},
        {v5, Color4B(color), __t(n)},
    };
    triangles[4] = triangles4;

    V2F_C4B_T2F_Triangle triangles5 = {
        {v6, Color4B(color), __t(v2fsub(t, n))},
        {v7, Color4B(color), __t(v2fadd(n, t))},
        {v5, Color4B(color), __t(n)},
    };
    triangles[5] = triangles5;

    _bufferCount += vertex_count;

    _dirty = true;
}

// @005b1124
void FFDrawNode::drawPolygon(const Vec2* verts, int count, const Color4F& fillColor,
                             float borderWidth, const Color4F& borderColor)
{
    bool outline = (borderColor.a > 0.0f && borderWidth > 0.0f);

    auto triangle_count = outline ? (3 * count - 2) : (count - 2);
    auto vertex_count = 3 * triangle_count;
    ensureCapacity(vertex_count);

    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)(_buffer + _bufferCount);
    V2F_C4B_T2F_Triangle* cursor = triangles;

    for (int i = 0; i < count - 2; i++)
    {
        V2F_C4B_T2F_Triangle tmp = {
            {verts[0], Color4B(fillColor), __t(Vec2(0.0f, 0.0f))},
            {verts[i + 1], Color4B(fillColor), __t(Vec2(0.0f, 0.0f))},
            {verts[i + 2], Color4B(fillColor), __t(Vec2(0.0f, 0.0f))},
        };

        *cursor++ = tmp;
    }

    if (outline)
    {
        struct ExtrudeVerts
        {
            Vec2 offset, n;
        };
        struct ExtrudeVerts* extrude = (struct ExtrudeVerts*)malloc(sizeof(struct ExtrudeVerts) * count);
        memset(extrude, 0, sizeof(struct ExtrudeVerts) * count);

        for (int i = 0; i < count; i++)
        {
            Vec2 v0 = __v2f(verts[(i - 1 + count) % count]);
            Vec2 v1 = __v2f(verts[i]);
            Vec2 v2 = __v2f(verts[(i + 1) % count]);

            Vec2 n1 = v2fnormalize(v2fperp(v2fsub(v1, v0)));
            Vec2 n2 = v2fnormalize(v2fperp(v2fsub(v2, v1)));

            Vec2 offset = v2fmult(v2fadd(n1, n2), 1.0f / (v2fdot(n1, n2) + 1.0f));
            struct ExtrudeVerts tmp = {offset, n2};
            extrude[i] = tmp;
        }

        for (int i = 0; i < count; i++)
        {
            int j = (i + 1) % count;
            Vec2 v0 = __v2f(verts[i]);
            Vec2 v1 = __v2f(verts[j]);

            Vec2 n0 = extrude[i].n;

            Vec2 offset0 = extrude[i].offset;
            Vec2 offset1 = extrude[j].offset;

            Vec2 inner0 = v2fsub(v0, v2fmult(offset0, borderWidth));
            Vec2 inner1 = v2fsub(v1, v2fmult(offset1, borderWidth));
            Vec2 outer0 = v2fadd(v0, v2fmult(offset0, borderWidth));
            Vec2 outer1 = v2fadd(v1, v2fmult(offset1, borderWidth));

            V2F_C4B_T2F_Triangle tmp1 = {
                {inner0, Color4B(borderColor), __t(v2fneg(n0))},
                {inner1, Color4B(borderColor), __t(v2fneg(n0))},
                {outer1, Color4B(borderColor), __t(n0)}};
            *cursor++ = tmp1;

            V2F_C4B_T2F_Triangle tmp2 = {
                {inner0, Color4B(borderColor), __t(v2fneg(n0))},
                {outer0, Color4B(borderColor), __t(n0)},
                {outer1, Color4B(borderColor), __t(n0)}};
            *cursor++ = tmp2;
        }

        free(extrude);
    }

    _bufferCount += vertex_count;

    _dirty = true;
}

// @005b1558
void FFDrawNode::drawSolidRect(const Vec2& origin, const Vec2& destination, const Color4F& color)
{
    Vec2 vertices[] = {origin, Vec2(destination.x, origin.y), destination,
                       Vec2(origin.x, destination.y)};

    drawSolidPoly(vertices, 4, color);
}

// @005b1604
void FFDrawNode::drawSolidPoly(const Vec2* poli, unsigned int numberOfPoints, const Color4F& color)
{
    drawPolygon(poli, numberOfPoints, color, 0.0, Color4F(0.0, 0.0, 0.0, 0.0));
}

// @005b1698
void FFDrawNode::drawSolidCircle(const Vec2& center, float radius, float angle,
                                 unsigned int segments, float scaleX, float scaleY,
                                 const Color4F& color)
{
    const float coef = 2.0f * (float)M_PI / segments;

    Vec2* vertices = new (std::nothrow) Vec2[segments];
    if (!vertices)
        return;

    for (unsigned int i = 0; i < segments; i++)
    {
        float rads = i * coef;
        GLfloat j = radius * cosf(rads + angle) * scaleX + center.x;
        GLfloat k = radius * sinf(rads + angle) * scaleY + center.y;

        vertices[i].x = j;
        vertices[i].y = k;
    }

    drawSolidPoly(vertices, segments, color);

    CC_SAFE_DELETE_ARRAY(vertices);
}

// @005b1970
void FFDrawNode::drawSolidCircle(const Vec2& center, float radius, float angle,
                                 unsigned int segments, const Color4F& color)
{
    drawSolidCircle(center, radius, angle, segments, 1.0f, 1.0f, color);
}

// @005b197c
void FFDrawNode::drawTriangle(const Vec2& p1, const Vec2& p2, const Vec2& p3, const Color4F& color)
{
    unsigned int vertex_count = 3;
    ensureCapacity(vertex_count);

    Color4B col = Color4B(color);
    V2F_C4B_T2F a = {Vec2(p1.x, p1.y), col, Tex2F(0.0, 0.0)};
    V2F_C4B_T2F b = {Vec2(p2.x, p2.y), col, Tex2F(0.0, 0.0)};
    V2F_C4B_T2F c = {Vec2(p3.x, p3.y), col, Tex2F(0.0, 0.0)};

    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)(_buffer + _bufferCount);
    V2F_C4B_T2F_Triangle triangle = {a, b, c};
    triangles[0] = triangle;

    _bufferCount += vertex_count;
    _dirty = true;
}

// @005b1a88
void FFDrawNode::drawQuadraticBezier(const Vec2& from, const Vec2& control, const Vec2& to,
                                     unsigned int segments, const Color4F& color)
{
    drawQuadBezier(from, control, to, segments, color);
}

// @005b1a8c
void FFDrawNode::clear()
{
    _bufferCount = 0;
    _dirty = true;
    _artDelegateCount = 0;
    _bufferCountGLLine = 0;
    _dirtyGLLine = true;
    _bufferCountGLPoint = 0;
    _dirtyGLPoint = true;
    _lineWidth = _defaultLineWidth;
}

// @005b1ac4
const BlendFunc& FFDrawNode::getBlendFunc() const
{
    return _blendFunc;
}

// @005b1acc
void FFDrawNode::setBlendFunc(const BlendFunc& blendFunc)
{
    _blendFunc = blendFunc;
}

// @005b1ad8
void FFDrawNode::setLineWidth(float lineWidth)
{
    _lineWidth = lineWidth;
}

// @005b1ae0
float FFDrawNode::getLineWidth()
{
    return this->_lineWidth;
}

// @005b1ae8
// Fills a polygon (counter-clockwise or clockwise, at most 100 vertices) with ear clipping and
// returns the number of vertices added to _buffer (0 when count < 3 or no ear is found). A
// clockwise polygon is reversed IN PLACE in the caller's array. borderWidth/borderColor unused.
int FFDrawNode::drawPolyWithVerts(Vec2* verts, int count, Color4F fillColor, double borderWidth,
                                  Color4F borderColor)
{
    int vertex_count = 3 * count - 6;
    ensureCapacity(vertex_count);

    if (count < 3)
    {
        return 0;
    }

    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)(_buffer + _bufferCount);

    // TPPLPoly::GetOrientation: a clockwise polygon is inverted.
    float area = 0;
    for (int i1 = 0; i1 < count; i1++)
    {
        int i2 = i1 + 1;
        if (i2 == count)
        {
            i2 = 0;
        }
        area += verts[i1].x * verts[i2].y - verts[i1].y * verts[i2].x;
    }
    if (area < 0)
    {
        for (int i = 0, j = count - 1; i < j; i++, j--)
        {
            Vec2 tmp = verts[i];
            verts[i] = verts[j];
            verts[j] = tmp;
        }
    }

    if (count == 3)
    {
        V2F_C4B_T2F_Triangle triangle = {
            {verts[0], Color4B(fillColor), Tex2F(0.0, 0.0)},
            {verts[1], Color4B(fillColor), Tex2F(0.0, 0.0)},
            {verts[2], Color4B(fillColor), Tex2F(0.0, 0.0)},
        };
        triangles[0] = triangle;
    }
    else
    {
        // TPPLPartition::Triangulate_EC
        PartitionVert vertices[100];
        memset(vertices, 0, sizeof(vertices));

        for (int i = 0; i < count; i++)
        {
            vertices[i].isActive = true;
            vertices[i].p = verts[i];
            if (i == count - 1)
            {
                vertices[i].next = &vertices[0];
            }
            else
            {
                vertices[i].next = &vertices[i + 1];
            }
            if (i == 0)
            {
                vertices[i].previous = &vertices[count - 1];
            }
            else
            {
                vertices[i].previous = &vertices[i - 1];
            }
        }
        for (int i = 0; i < count; i++)
        {
            updateVertex(&vertices[i], vertices, count);
        }

        PartitionVert* ear = nullptr;
        // (polypartition loops to numvertices - 3; this port loops to count, the break below
        // ends it at the same iteration)
        for (int i = 0; i < count; i++)
        {
            bool earFound = false;
            // find the most extruded ear
            for (int j = 0; j < count; j++)
            {
                if (!vertices[j].isActive)
                {
                    continue;
                }
                if (!vertices[j].isEar)
                {
                    continue;
                }
                if (!earFound)
                {
                    earFound = true;
                    ear = &vertices[j];
                }
                else
                {
                    if (vertices[j].angle > ear->angle)
                    {
                        ear = &vertices[j];
                    }
                }
            }
            if (!earFound)
            {
                // _bufferCount is not advanced: the triangles written so far are discarded.
                // The original returns 0 here and the shape is invisible; PC addition (render
                // fix): fill it with the repaired / even-odd triangulation instead.
                return fallbackFill(verts, count, fillColor);
            }

            V2F_C4B_T2F_Triangle triangle = {
                {ear->previous->p, Color4B(fillColor), Tex2F(0.0, 0.0)},
                {ear->p, Color4B(fillColor), Tex2F(0.0, 0.0)},
                {ear->next->p, Color4B(fillColor), Tex2F(0.0, 0.0)},
            };
            *triangles++ = triangle;

            ear->isActive = false;
            ear->previous->next = ear->next;
            ear->next->previous = ear->previous;

            if (i == count - 4)
            {
                break;
            }

            updateVertex(ear->previous, vertices, count);
            updateVertex(ear->next, vertices, count);
        }

        for (int i = 0; i < count; i++)
        {
            if (vertices[i].isActive)
            {
                V2F_C4B_T2F_Triangle triangle = {
                    {vertices[i].previous->p, Color4B(fillColor), Tex2F(0.0, 0.0)},
                    {vertices[i].p, Color4B(fillColor), Tex2F(0.0, 0.0)},
                    {vertices[i].next->p, Color4B(fillColor), Tex2F(0.0, 0.0)},
                };
                *triangles = triangle;
                break;
            }
        }
    }

    _bufferCount += vertex_count;
    _dirty = true;
    return vertex_count;
}

// @005b1fd8
// TPPLPartition::UpdateVertex
void FFDrawNode::updateVertex(PartitionVert* v, PartitionVert* vertices, int numVertices)
{
    PartitionVert* v1 = v->previous;
    PartitionVert* v3 = v->next;

    v->isConvex = isConvex(v1->p, v->p, v3->p);

    Vec2 vec1 = v1->p - v->p;
    vec1.normalize();
    Vec2 vec3 = v3->p - v->p;
    vec3.normalize();
    v->angle = vec1.x * vec3.x + vec1.y * vec3.y;

    if (v->isConvex)
    {
        v->isEar = true;
        for (int i = 0; i < numVertices; i++)
        {
            if (vertices[i].p.x == v->p.x && vertices[i].p.y == v->p.y)
            {
                continue;
            }
            if (vertices[i].p.x == v1->p.x && vertices[i].p.y == v1->p.y)
            {
                continue;
            }
            if (vertices[i].p.x == v3->p.x && vertices[i].p.y == v3->p.y)
            {
                continue;
            }
            if (isInside(v1->p, v->p, v3->p, vertices[i].p))
            {
                v->isEar = false;
                break;
            }
        }
    }
    else
    {
        v->isEar = false;
    }
}

// @005b21b0
// Adds a dot (drawDot: 2 triangles around `offset`) as a delegate polygon; a static one is moved
// to `position` once, an updated one is re-transformed every frame by updateVerts().
// borderWidth/borderColor are unused (except in a browser-level shape style, PC addition).
void FFDrawNode::drawDotWithOffset(Vec2 position, Vec2 offset, float radius, Color4F color,
                                   float borderWidth, Color4F borderColor, bool updateArt,
                                   FFDrawNodeDelegate* artDelegate)
{
    ArtDelegate art;
    art.triangleIndex = _bufferCount / 3;
    art.update = updateArt;
    art.delegate = artDelegate;

    if (!_onlineStyle)
    {
        drawDot(offset, radius, color);
        art.triangleCount = 2;
    }
    else
    {
        // ONLINE (PC addition): Flash CircleShape: the fill (a ring when it has an inner
        // cutout), then the outline of the circle (and of the cutout) over it.
        int vertexCount = 0;
        const float cutout = std::max(0.0f, std::min(1.0f, _onlineInnerCutout));
        if (_onlineFill && cutout <= 0.0f)
        {
            drawDot(offset, radius, color);
            vertexCount += 6;
        }
        else if (_onlineFill && cutout < 1.0f)
        {
            vertexCount += onlineDrawRing(offset, radius * cutout, radius, color);
        }
        if (_onlineOutlineWidth > 0.0f && borderWidth > 0.0f)
        {
            const float half = _onlineOutlineWidth * 0.5f;
            vertexCount += onlineDrawRing(offset, std::max(0.0f, radius - half), radius + half,
                                          borderColor);
            if (cutout > 0.0f && cutout < 1.0f)
            {
                const float inner = radius * cutout;
                vertexCount += onlineDrawRing(offset, std::max(0.0f, inner - half), inner + half,
                                              borderColor);
            }
        }
        art.triangleCount = vertexCount / 3;
    }

    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)_buffer;
    for (unsigned int i = 0; i < art.triangleCount; i++)
    {
        localTriangle(art, i) = triangles[art.triangleIndex + i];
    }

    if (!updateArt)
    {
        AffineTransform transform =
            AffineTransformTranslate(AffineTransformIdentity, position.x, position.y);
        for (unsigned int i = 0; i < art.triangleCount; i++)
        {
            V2F_C4B_T2F_Triangle& triangle = triangles[art.triangleIndex + i];
            applyArtTransform(triangle, triangle, transform);
        }
    }

    onlineReserveArtDelegate();  // ONLINE (PC addition)
    _artDelegates[_artDelegateCount] = std::move(art);
    _artDelegateCount++;
}

// @005b2488
// TPPLPartition::IsConvex
bool FFDrawNode::isConvex(Vec2 p1, Vec2 p2, Vec2 p3)
{
    float tmp = (p3.y - p1.y) * (p2.x - p1.x) - (p3.x - p1.x) * (p2.y - p1.y);
    if (tmp > 0)
        return true;
    else
        return false;
}

// @005b24ac
// TPPLPartition::IsInside
bool FFDrawNode::isInside(Vec2 p1, Vec2 p2, Vec2 p3, Vec2 p)
{
    if (isConvex(p1, p, p2))
        return false;
    if (isConvex(p2, p, p3))
        return false;
    if (isConvex(p3, p, p1))
        return false;
    return true;
}

// @005b2520
bool FFDrawNode::compareTransforms(AffineTransform t1, AffineTransform t2)
{
    return t1.a == t2.a && t1.b == t2.b && t1.c == t2.c && t1.d == t2.d && t1.tx == t2.tx &&
           t1.ty == t2.ty;
}

// @005b258c
// The polygon is triangulated into _buffer and moved by `transform` (local -> art space). A static
// polygon (!updateArt) keeps that art-space copy in its record and is moved once more by
// `artTransform` in the buffer; an updated one is re-transformed every frame by updateVerts().
void FFDrawNode::drawPolyWithVerts(Vec2* verts, int count, Color4F fillColor, double borderWidth,
                                   Color4F borderColor, bool updateArt, AffineTransform transform,
                                   AffineTransform artTransform, FFDrawNodeDelegate* artDelegate)
{
    ArtDelegate art;
    art.triangleIndex = _bufferCount / 3;
    art.update = updateArt;
    art.delegate = artDelegate;

    int vertexCount;
    if (!_onlineStyle)
    {
        vertexCount = drawPolyWithVerts(verts, count, fillColor, borderWidth, borderColor);
    }
    else
    {
        // ONLINE (PC addition): Flash ShapeRef: the fill (unless p8 is -1), then the outline
        // over it.
        vertexCount = 0;
        if (_onlineFill)
        {
            vertexCount += drawPolyWithVerts(verts, count, fillColor, borderWidth, borderColor);
        }
        if (_onlineOutlineWidth > 0.0f && borderWidth > 0.0f)
        {
            vertexCount += onlineDrawOutline(verts, count, borderColor, _onlineOutlineWidth);
        }
    }
    art.triangleCount = vertexCount / 3;

    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)_buffer;

    if (!compareTransforms(transform, AffineTransformIdentity))
    {
        for (unsigned int i = 0; i < art.triangleCount; i++)
        {
            V2F_C4B_T2F_Triangle& triangle = triangles[art.triangleIndex + i];
            applyArtTransform(triangle, triangle, transform);
        }
    }

    if (!updateArt)
    {
        for (unsigned int i = 0; i < art.triangleCount; i++)
        {
            V2F_C4B_T2F_Triangle& triangle = triangles[art.triangleIndex + i];
            localTriangle(art, i) = triangle;
            applyArtTransform(triangle, triangle, artTransform);
        }
    }
    else
    {
        for (unsigned int i = 0; i < art.triangleCount; i++)
        {
            localTriangle(art, i) = triangles[art.triangleIndex + i];
        }
    }

    onlineReserveArtDelegate();  // ONLINE (PC addition)
    _artDelegates[_artDelegateCount] = std::move(art);
    _artDelegateCount++;
}

// @005b2994
// RE-NOTE(@005b2994): if the delegate is not registered, `removed` keeps uninitialised
// triangleIndex/triangleCount and the shifts below run with garbage, exactly as in the original
// (callers only remove registered delegates). With an empty table, `_artDelegateCount - 1` wraps.
void FFDrawNode::removeDelegate(FFDrawNodeDelegate* artDelegate)
{
    ArtDelegate removed;
    unsigned int index;
    for (index = 0; index < _artDelegateCount; index++)
    {
        if (_artDelegates[index].delegate == artDelegate)
        {
            removed = _artDelegates[index];
            break;
        }
    }

    for (unsigned int i = index; i < _artDelegateCount - 1; i++)
    {
        _artDelegates[i] = std::move(_artDelegates[i + 1]);
        _artDelegates[i].triangleIndex -= removed.triangleCount;
    }

    // Close the gap in _buffer (reads past _bufferCount for the last removed.triangleCount
    // triangles; those slots are dropped by the _bufferCount update below).
    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)_buffer;
    for (unsigned int i = removed.triangleIndex; i < _bufferCount / 3; i++)
    {
        triangles[i] = triangles[i + removed.triangleCount];
    }

    _dirty = true;
    _artDelegateCount--;
    _bufferCount -= removed.triangleCount * 3;
}

// @005b2ba0
void FFDrawNode::replaceDelegate(FFDrawNodeDelegate* oldDelegate, FFDrawNodeDelegate* newDelegate)
{
    for (unsigned int i = 0; i < _artDelegateCount; i++)
    {
        if (_artDelegates[i].delegate == oldDelegate)
        {
            _artDelegates[i].delegate = newDelegate;  // (a record copy-modify-store originally)
            return;
        }
    }
}

// @005b2be8
// isStatic: bake `transform` into the buffer once and stop updating; !isStatic: update again.
// Stops at the first matching record that is already in the requested state.
void FFDrawNode::setArtDelegateToStatic(bool isStatic, AffineTransform transform,
                                        FFDrawNodeDelegate* artDelegate)
{
    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)_buffer;
    for (unsigned int i = 0; i < _artDelegateCount; i++)
    {
        ArtDelegate& art = _artDelegates[i];  // (a record copy stored back in the original)
        if (art.delegate == artDelegate)
        {
            if (art.update == !isStatic)
            {
                break;
            }
            if (isStatic)
            {
                for (unsigned int j = 0; j < art.triangleCount; j++)
                {
                    applyArtTransform(triangles[art.triangleIndex + j], localTriangle(art, j),
                                      transform);
                }
            }
            art.update = !isStatic;
        }
    }
}

// @005b2da8
// Per frame: every delegate's triangles get the delegate's opacity as vertex alpha; updated ones
// that are visible are also re-transformed from their local copy by the delegate's art transform.
void FFDrawNode::updateVerts()
{
    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)_buffer;
    for (unsigned int i = 0; i < _artDelegateCount; i++)
    {
        const ArtDelegate& art = _artDelegates[i];  // (a record copy in the original)
        if (art.delegate)
        {
            // float -> int, truncated to the GLubyte alpha (no clamping)
            int opacity = (int)(art.delegate->getArtOpacity() * 255.0f);
            if (art.update && opacity != 0)
            {
                AffineTransform transform = art.delegate->getArtTransform();
                for (unsigned int j = art.triangleIndex; j < art.triangleIndex + art.triangleCount; j++)
                {
                    V2F_C4B_T2F_Triangle& triangle = triangles[j];
                    triangle.a.colors.a = triangle.b.colors.a = triangle.c.colors.a = opacity;
                    applyArtTransform(triangle, localTriangle(art, j - art.triangleIndex),
                                      transform);
                }
            }
            else
            {
                for (unsigned int j = art.triangleIndex; j < art.triangleIndex + art.triangleCount; j++)
                {
                    V2F_C4B_T2F_Triangle& triangle = triangles[j];
                    triangle.a.colors.a = triangle.b.colors.a = triangle.c.colors.a = opacity;
                }
            }
        }
    }
    _dirty = true;
}

// ONLINE (PC addition): grow the delegate table instead of writing past 1600 entries.
void FFDrawNode::onlineReserveArtDelegate()
{
    if (_artDelegateCount >= _artDelegates.size())
    {
        _artDelegates.resize(_artDelegates.size() * 2);
    }
}

// ONLINE (PC addition): see FFDrawNode.h.
void FFDrawNode::onlineSetShapeStyle(bool fill, float outlineWidth, float innerCutout)
{
    _onlineStyle = true;
    _onlineFill = fill;
    _onlineOutlineWidth = outlineWidth;
    _onlineInnerCutout = innerCutout;
}

void FFDrawNode::onlineClearShapeStyle()
{
    _onlineStyle = false;
    _onlineFill = true;
    _onlineOutlineWidth = 0.0f;
    _onlineInnerCutout = 0.0f;
}

// ONLINE (PC addition): Flash strokes a shape's path centred on it. The closed polyline becomes
// one quad per edge between mitred corner points (miter length limited to 4 half-widths, so a
// spike gets a slightly thinner tip instead of a long point).
int FFDrawNode::onlineDrawOutline(const Vec2* verts, int count, const Color4F& color, float width)
{
    if (verts == nullptr || count < 2 || !(width > 0.0f))
    {
        return 0;
    }
    std::vector<Vec2> ring;
    ring.reserve((size_t)count);
    for (int i = 0; i < count; i++)
    {
        if (ring.empty() || ring.back().distanceSquared(verts[i]) > 1e-8f)
        {
            ring.push_back(verts[i]);
        }
    }
    while (ring.size() > 1 && ring.back().distanceSquared(ring.front()) <= 1e-8f)
    {
        ring.pop_back();
    }
    const int n = (int)ring.size();
    if (n < 2)
    {
        return 0;
    }

    const float half = width * 0.5f;
    std::vector<Vec2> outer((size_t)n);
    std::vector<Vec2> inner((size_t)n);
    for (int i = 0; i < n; i++)
    {
        const Vec2& previous = ring[(i + n - 1) % n];
        const Vec2& current = ring[i];
        const Vec2& next = ring[(i + 1) % n];
        Vec2 d0 = current - previous;
        Vec2 d1 = next - current;
        d0.normalize();
        d1.normalize();
        const Vec2 n0(-d0.y, d0.x);
        const Vec2 n1(-d1.y, d1.x);
        Vec2 miter = n0 + n1;
        float scale = 1.0f;
        const float length = miter.length();
        if (length < 1e-3f)
        {
            miter = n1;  // the path turns back on itself
        }
        else
        {
            miter = miter / length;
            const float cosine = miter.dot(n1);
            scale = cosine > 0.25f ? 1.0f / cosine : 4.0f;
        }
        const Vec2 offset = miter * (half * scale);
        outer[(size_t)i] = current + offset;
        inner[(size_t)i] = current - offset;
    }

    const int vertexCount = n * 6;
    ensureCapacity(vertexCount);
    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)(_buffer + _bufferCount);
    const Color4B col(color);
    for (int i = 0; i < n; i++)
    {
        const int j = (i + 1) % n;
        const V2F_C4B_T2F a = {outer[(size_t)i], col, Tex2F(0.0f, 0.0f)};
        const V2F_C4B_T2F b = {outer[(size_t)j], col, Tex2F(0.0f, 0.0f)};
        const V2F_C4B_T2F c = {inner[(size_t)j], col, Tex2F(0.0f, 0.0f)};
        const V2F_C4B_T2F d = {inner[(size_t)i], col, Tex2F(0.0f, 0.0f)};
        *triangles++ = {a, b, c};
        *triangles++ = {a, c, d};
    }
    _bufferCount += vertexCount;
    _dirty = true;
    return vertexCount;
}

// ONLINE (PC addition): an annulus (innerRadius may be 0: a full disc) of plain triangles.
int FFDrawNode::onlineDrawRing(const Vec2& center, float innerRadius, float outerRadius,
                               const Color4F& color)
{
    if (!(outerRadius > innerRadius))
    {
        return 0;
    }
    // About one segment per 12 points of circumference.
    const int segments =
        std::max(24, std::min(160, (int)std::ceil(outerRadius * 6.2831855f / 12.0f)));
    const int vertexCount = segments * 6;
    ensureCapacity(vertexCount);
    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)(_buffer + _bufferCount);
    const Color4B col(color);
    for (int i = 0; i < segments; i++)
    {
        const float a0 = 6.2831855f * (float)i / (float)segments;
        const float a1 = 6.2831855f * (float)(i + 1) / (float)segments;
        const Vec2 u0(std::cos(a0), std::sin(a0));
        const Vec2 u1(std::cos(a1), std::sin(a1));
        const V2F_C4B_T2F a = {center + u0 * outerRadius, col, Tex2F(0.0f, 0.0f)};
        const V2F_C4B_T2F b = {center + u1 * outerRadius, col, Tex2F(0.0f, 0.0f)};
        const V2F_C4B_T2F c = {center + u1 * innerRadius, col, Tex2F(0.0f, 0.0f)};
        const V2F_C4B_T2F d = {center + u0 * innerRadius, col, Tex2F(0.0f, 0.0f)};
        *triangles++ = {a, b, c};
        *triangles++ = {a, c, d};
    }
    _bufferCount += vertexCount;
    _dirty = true;
    return vertexCount;
}

// PC addition (render fix): see PolyFill.h. Writes from _bufferCount like the ear clipping.
int FFDrawNode::fallbackFill(const Vec2* verts, int count, const Color4F& fillColor)
{
    std::vector<Vec2> points;
    polyfill::triangulate(verts, count, points);
    const int vertexCount = (int)points.size();
    if (vertexCount == 0)
    {
        return 0;
    }
    ensureCapacity(vertexCount);
    const Color4B col(fillColor);
    V2F_C4B_T2F* vertex = _buffer + _bufferCount;
    for (int i = 0; i < vertexCount; i++)
    {
        vertex[i] = {points[(size_t)i], col, Tex2F(0.0f, 0.0f)};
    }
    _bufferCount += vertexCount;
    _dirty = true;
    return vertexCount;
}
