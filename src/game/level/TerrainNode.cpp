// TerrainNode: draws the level's terrain polygons. Same code as FFDrawNode (an old cocos2d::DrawNode
// copy with "art delegate" records and polypartition-style ear clipping), with `unsigned long`
// vertex counts.

#include "TerrainNode.h"

#include <cstdlib>
#include <cstring>
#include <vector>

#include "FFDrawNode.h"  // struct PartitionVert
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

// @00634004
TerrainNode::TerrainNode(float lineWidth)
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
, _artDelegateCount(0)
{
    _blendFunc = BlendFunc::ALPHA_PREMULTIPLIED;
}

// @0063418c
TerrainNode::~TerrainNode()
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

// @006342c8
TerrainNode* TerrainNode::create(float lineWidth)
{
    TerrainNode* ret = new (std::nothrow) TerrainNode(lineWidth);
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

// @00634368
void TerrainNode::ensureCapacity(int count)
{
    if (_bufferCount + count > _bufferCapacity)
    {
        _bufferCapacity += MAX(_bufferCapacity, count);
        _buffer = (V2F_C4B_T2F*)realloc(_buffer, _bufferCapacity * sizeof(V2F_C4B_T2F));
    }
}

// @006343bc
void TerrainNode::ensureCapacityGLPoint(int count)
{
    if (_bufferCountGLPoint + count > _bufferCapacityGLPoint)
    {
        _bufferCapacityGLPoint += MAX(_bufferCapacityGLPoint, count);
        _bufferGLPoint = (V2F_C4B_T2F*)realloc(_bufferGLPoint,
                                               _bufferCapacityGLPoint * sizeof(V2F_C4B_T2F));
    }
}

// @00634410
void TerrainNode::ensureCapacityGLLine(int count)
{
    if (_bufferCountGLLine + count > _bufferCapacityGLLine)
    {
        _bufferCapacityGLLine += MAX(_bufferCapacityGLLine, count);
        _bufferGLLine = (V2F_C4B_T2F*)realloc(_bufferGLLine,
                                              _bufferCapacityGLLine * sizeof(V2F_C4B_T2F));
    }
}

// @00634464
bool TerrainNode::init()
{
    _blendFunc = BlendFunc::ALPHA_PREMULTIPLIED;

    setGLProgramState(GLProgramState::getOrCreateWithGLProgramName(
        GLProgram::SHADER_NAME_POSITION_LENGTH_TEXTURE_COLOR));

    ensureCapacity(512);
    ensureCapacityGLPoint(64);
    ensureCapacityGLLine(256);

    // DrawNode::setupBuffer(), inlined.
    if (Configuration::getInstance()->supportsShareableVAO())
    {
        glGenVertexArrays(1, &_vao);
        GL::bindVAO(_vao);
        glGenBuffers(1, &_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(V2F_C4B_T2F) * _bufferCapacity, _buffer, GL_STREAM_DRAW);
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_POSITION);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, vertices));
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_COLOR);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, colors));
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_TEX_COORD);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_TEX_COORD, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, texCoords));

        glGenVertexArrays(1, &_vaoGLLine);
        GL::bindVAO(_vaoGLLine);
        glGenBuffers(1, &_vboGLLine);
        glBindBuffer(GL_ARRAY_BUFFER, _vboGLLine);
        glBufferData(GL_ARRAY_BUFFER, sizeof(V2F_C4B_T2F) * _bufferCapacityGLLine, _bufferGLLine,
                     GL_STREAM_DRAW);
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_POSITION);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, vertices));
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_COLOR);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, colors));
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_TEX_COORD);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_TEX_COORD, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, texCoords));

        glGenVertexArrays(1, &_vaoGLPoint);
        GL::bindVAO(_vaoGLPoint);
        glGenBuffers(1, &_vboGLPoint);
        glBindBuffer(GL_ARRAY_BUFFER, _vboGLPoint);
        glBufferData(GL_ARRAY_BUFFER, sizeof(V2F_C4B_T2F) * _bufferCapacityGLPoint, _bufferGLPoint,
                     GL_STREAM_DRAW);
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_POSITION);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, vertices));
        glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_COLOR);
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, colors));
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

    _dirty = true;
    _dirtyGLLine = true;
    _dirtyGLPoint = true;

    // The renderer was recreated (Android GL context loss): rebuild the buffers.
    auto listener = EventListenerCustom::create(EVENT_RENDERER_RECREATED, [this](EventCustom* event) {
        // @00636924
        this->init();
    });
    _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);

    return true;
}

// @00634a4c
void TerrainNode::draw(Renderer* renderer, const Mat4& transform, uint32_t flags)
{
    if (_bufferCount)
    {
        _customCommand.init(_globalZOrder, transform, flags);
        _customCommand.func = CC_CALLBACK_0(TerrainNode::onDraw, this, transform, flags);
        renderer->addCommand(&_customCommand);
    }

    if (_bufferCountGLPoint)
    {
        _customCommandGLPoint.init(_globalZOrder, transform, flags);
        _customCommandGLPoint.func = CC_CALLBACK_0(TerrainNode::onDrawGLPoint, this, transform, flags);
        renderer->addCommand(&_customCommandGLPoint);
    }

    if (_bufferCountGLLine)
    {
        _customCommandGLLine.init(_globalZOrder, transform, flags);
        _customCommandGLLine.func = CC_CALLBACK_0(TerrainNode::onDrawGLLine, this, transform, flags);
        renderer->addCommand(&_customCommandGLLine);
    }
}

// @00634d58
void TerrainNode::onDraw(const Mat4& transform, uint32_t flags)
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
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, vertices));
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, colors));
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
}

// @00634f64
void TerrainNode::onDrawGLLine(const Mat4& transform, uint32_t flags)
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
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, vertices));
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                              sizeof(V2F_C4B_T2F), (GLvoid*)offsetof(V2F_C4B_T2F, colors));
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
}

// @00635238
void TerrainNode::onDrawGLPoint(const Mat4& transform, uint32_t flags)
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
}

// @00635504
void TerrainNode::clear()
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

// @0063553c
const BlendFunc& TerrainNode::getBlendFunc() const
{
    return _blendFunc;
}

// @00635544
void TerrainNode::setBlendFunc(const BlendFunc& blendFunc)
{
    _blendFunc = blendFunc;
}

// @00635550
void TerrainNode::setLineWidth(float lineWidth)
{
    _lineWidth = lineWidth;
}

// @00635558
float TerrainNode::getLineWidth()
{
    return _lineWidth;
}

// @00635560
int TerrainNode::drawPolyWithVerts(Vec2* verts, unsigned long count, Color4F fillColor,
                                   double borderWidth, Color4F borderColor)
{
    int vertexCount = ((int)count - 2) * 3;
    ensureCapacity(vertexCount);

    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)(_buffer + _bufferCount);

    if (count < 3)
    {
        return 0;
    }

    // Orientation (polypartition TPPLPoly::GetOrientation): clockwise polygons are inverted.
    float area = 0.0f;
    for (unsigned long i1 = 0; i1 < count; i1++)
    {
        unsigned long i2 = i1 + 1;
        if (i2 == count)
        {
            i2 = 0;
        }
        area += verts[i1].x * verts[i2].y - verts[i1].y * verts[i2].x;
    }
    if (area < 0)
    {
        for (int i = 0, j = (int)count - 1; i < j; i++, j--)
        {
            Vec2 tmp = verts[i];
            verts[i] = verts[j];
            verts[j] = tmp;
        }
    }

    if (count == 3)
    {
        V2F_C4B_T2F_Triangle tmp = {
            {verts[0], Color4B(fillColor), Tex2F(0.0f, 0.0f)},
            {verts[1], Color4B(fillColor), Tex2F(0.0f, 0.0f)},
            {verts[2], Color4B(fillColor), Tex2F(0.0f, 0.0f)},
        };
        *triangles = tmp;
    }
    else
    {
        // Ear clipping (polypartition TPPLPartition::Triangulate_EC).
        PartitionVert vertices[2000];
        memset(vertices, 0, sizeof(vertices));

        for (unsigned long i = 0; i < count; i++)
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
        for (unsigned long i = 0; i < count; i++)
        {
            updateVertex(&vertices[i], vertices, count);
        }

        PartitionVert* ear = nullptr;
        for (unsigned long i = 0; i < count; i++)
        {
            bool earFound = false;
            // find the most extruded ear
            for (unsigned long j = 0; j < count; j++)
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
                // The original returns 0 here and the terrain is not drawn at all. PC addition
                // (render fix): fill it with the repaired / even-odd triangulation (PolyFill.h).
                std::vector<Vec2> points;
                polyfill::triangulate(verts, (int)count, points);
                const int fallbackCount = (int)points.size();
                if (fallbackCount == 0)
                {
                    return 0;
                }
                ensureCapacity(fallbackCount);
                for (int k = 0; k < fallbackCount; k++)
                {
                    _buffer[_bufferCount + k] = {points[(size_t)k], Color4B(fillColor),
                                                 Tex2F(0.0f, 0.0f)};
                }
                _dirty = true;
                _bufferCount += fallbackCount;
                return fallbackCount;
            }

            V2F_C4B_T2F_Triangle tmp = {
                {ear->previous->p, Color4B(fillColor), Tex2F(0.0f, 0.0f)},
                {ear->p, Color4B(fillColor), Tex2F(0.0f, 0.0f)},
                {ear->next->p, Color4B(fillColor), Tex2F(0.0f, 0.0f)},
            };
            *triangles++ = tmp;

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

        for (unsigned long i = 0; i < count; i++)
        {
            if (vertices[i].isActive)
            {
                V2F_C4B_T2F_Triangle tmp = {
                    {vertices[i].previous->p, Color4B(fillColor), Tex2F(0.0f, 0.0f)},
                    {vertices[i].p, Color4B(fillColor), Tex2F(0.0f, 0.0f)},
                    {vertices[i].next->p, Color4B(fillColor), Tex2F(0.0f, 0.0f)},
                };
                *triangles = tmp;
                break;
            }
        }
    }

    _dirty = true;
    _bufferCount += vertexCount;
    return vertexCount;
}

// @00635aa8
void TerrainNode::updateVertex(PartitionVert* v, PartitionVert* vertices, unsigned long numVertices)
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
        for (unsigned long i = 0; i < numVertices; i++)
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

// @00635c78
bool TerrainNode::isConvex(Vec2 p1, Vec2 p2, Vec2 p3)
{
    float tmp = (p3.y - p1.y) * (p2.x - p1.x) - (p3.x - p1.x) * (p2.y - p1.y);
    return tmp > 0;
}

// @00635c9c
bool TerrainNode::isInside(Vec2 p1, Vec2 p2, Vec2 p3, Vec2 p)
{
    if (isConvex(p1, p, p2))
    {
        return false;
    }
    if (isConvex(p2, p, p3))
    {
        return false;
    }
    if (isConvex(p3, p, p1))
    {
        return false;
    }
    return true;
}

// @00635d10
bool TerrainNode::compareTransforms(AffineTransform t1, AffineTransform t2)
{
    return t1.a == t2.a && t1.b == t2.b && t1.c == t2.c && t1.d == t2.d && t1.tx == t2.tx &&
           t1.ty == t2.ty;
}

// @00635d7c
void TerrainNode::drawPolyWithVerts(Vec2* verts, unsigned long count, Color4F fillColor,
                                    double borderWidth, Color4F borderColor, bool update,
                                    AffineTransform initialTransform,
                                    AffineTransform initialOffset, FFDrawNodeDelegate* delegate)
{
    ArtDelegate artDelegate;
    artDelegate.triangleIndex = _bufferCount / 3;
    artDelegate.update = update;
    artDelegate.delegate = delegate;

    int vertexCount = drawPolyWithVerts(verts, count, fillColor, borderWidth, borderColor);
    artDelegate.triangleCount = vertexCount / 3;

    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)_buffer;

    if (!compareTransforms(initialTransform, AffineTransformIdentity))
    {
        for (unsigned int i = 0; i < artDelegate.triangleCount; i++)
        {
            V2F_C4B_T2F_Triangle& t = triangles[artDelegate.triangleIndex + i];
            const AffineTransform& m = initialTransform;
            t.a.vertices = Vec2(m.a * t.a.vertices.x + m.c * t.a.vertices.y + m.tx,
                                m.b * t.a.vertices.x + m.d * t.a.vertices.y + m.ty);
            t.b.vertices = Vec2(m.a * t.b.vertices.x + m.c * t.b.vertices.y + m.tx,
                                m.b * t.b.vertices.x + m.d * t.b.vertices.y + m.ty);
            t.c.vertices = Vec2(m.a * t.c.vertices.x + m.c * t.c.vertices.y + m.tx,
                                m.b * t.c.vertices.x + m.d * t.c.vertices.y + m.ty);
        }
    }

    if (update)
    {
        // keep the local-space art; updateVerts() re-transforms it every frame
        for (unsigned int i = 0; i < artDelegate.triangleCount; i++)
        {
            artDelegate.triangles[i] = triangles[artDelegate.triangleIndex + i];
        }
    }
    else
    {
        for (unsigned int i = 0; i < artDelegate.triangleCount; i++)
        {
            V2F_C4B_T2F_Triangle& t = triangles[artDelegate.triangleIndex + i];
            artDelegate.triangles[i] = t;
            const AffineTransform& m = initialOffset;
            t.a.vertices = Vec2(m.a * t.a.vertices.x + m.c * t.a.vertices.y + m.tx,
                                m.b * t.a.vertices.x + m.d * t.a.vertices.y + m.ty);
            t.b.vertices = Vec2(m.a * t.b.vertices.x + m.c * t.b.vertices.y + m.tx,
                                m.b * t.b.vertices.x + m.d * t.b.vertices.y + m.ty);
            t.c.vertices = Vec2(m.a * t.c.vertices.x + m.c * t.c.vertices.y + m.tx,
                                m.b * t.c.vertices.x + m.d * t.c.vertices.y + m.ty);
        }
    }

    _artDelegates[_artDelegateCount] = artDelegate;
    _artDelegateCount++;
}

// @00636184
// RE-TODO(@00636184): the binary leaves x0 = &removed (a dead stack local) on return, as if the
// original returned a pointer to its local copy; no caller exists, return type kept void.
void TerrainNode::removeDelegate(FFDrawNodeDelegate* delegate)
{
    ArtDelegate removed;
    unsigned int index;
    for (index = 0; index < _artDelegateCount; index++)
    {
        if (_artDelegates[index].delegate == delegate)
        {
            removed = _artDelegates[index];
            break;
        }
    }

    // RE-TODO(@00636184): with _artDelegateCount == 0 this loop runs (count - 1 wraps); kept as
    // in the binary.
    for (unsigned int i = index; i < _artDelegateCount - 1; i++)
    {
        ArtDelegate moved = _artDelegates[i + 1];
        moved.triangleIndex -= removed.triangleCount;
        _artDelegates[i] = moved;
    }

    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)_buffer;
    unsigned int triangleTotal = _bufferCount / 3;
    for (unsigned int i = removed.triangleIndex; i < triangleTotal; i++)
    {
        triangles[i] = triangles[i + removed.triangleCount];
    }

    _dirty = true;
    _artDelegateCount--;
    _bufferCount -= removed.triangleCount * 3;
}

// @00636390
void TerrainNode::replaceDelegate(FFDrawNodeDelegate* oldDelegate, FFDrawNodeDelegate* newDelegate)
{
    for (unsigned int i = 0; i < _artDelegateCount; i++)
    {
        if (_artDelegates[i].delegate == oldDelegate)
        {
            ArtDelegate artDelegate = _artDelegates[i];
            artDelegate.delegate = newDelegate;
            _artDelegates[i] = artDelegate;
            return;
        }
    }
}

// @006363d8
void TerrainNode::setArtDelegateToStatic(bool isStatic, AffineTransform transform,
                                         FFDrawNodeDelegate* delegate)
{
    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)_buffer;
    for (unsigned int i = 0; i < _artDelegateCount; i++)
    {
        ArtDelegate artDelegate = _artDelegates[i];
        if (artDelegate.delegate == delegate)
        {
            if (artDelegate.update == !isStatic)
            {
                break;
            }
            if (isStatic)
            {
                // bake the current art transform into the buffer once
                for (unsigned int j = 0; j < artDelegate.triangleCount; j++)
                {
                    const V2F_C4B_T2F_Triangle& src = artDelegate.triangles[j];
                    V2F_C4B_T2F_Triangle& dst = triangles[artDelegate.triangleIndex + j];
                    const AffineTransform& m = transform;
                    dst.a.vertices = Vec2(m.a * src.a.vertices.x + m.c * src.a.vertices.y + m.tx,
                                          m.b * src.a.vertices.x + m.d * src.a.vertices.y + m.ty);
                    dst.b.vertices = Vec2(m.a * src.b.vertices.x + m.c * src.b.vertices.y + m.tx,
                                          m.b * src.b.vertices.x + m.d * src.b.vertices.y + m.ty);
                    dst.c.vertices = Vec2(m.a * src.c.vertices.x + m.c * src.c.vertices.y + m.tx,
                                          m.b * src.c.vertices.x + m.d * src.c.vertices.y + m.ty);
                }
            }
            artDelegate.update = !isStatic;
            _artDelegates[i] = artDelegate;
        }
    }
}

// @00636598
void TerrainNode::updateVerts()
{
    V2F_C4B_T2F_Triangle* triangles = (V2F_C4B_T2F_Triangle*)_buffer;
    ArtDelegate artDelegate;
    for (unsigned int i = 0; i < _artDelegateCount; i++)
    {
        artDelegate = _artDelegates[i];
        if (artDelegate.delegate)
        {
            int opacity = artDelegate.delegate->getArtOpacity() * 255.0f;
            if (artDelegate.update && opacity != 0)
            {
                AffineTransform m = artDelegate.delegate->getArtTransform();
                for (unsigned int j = artDelegate.triangleIndex;
                     j < artDelegate.triangleIndex + artDelegate.triangleCount; j++)
                {
                    const V2F_C4B_T2F_Triangle& src = artDelegate.triangles[j - artDelegate.triangleIndex];
                    V2F_C4B_T2F_Triangle& dst = triangles[j];
                    dst.a.colors.a = opacity;
                    dst.b.colors.a = opacity;
                    dst.c.colors.a = opacity;
                    dst.a.vertices = Vec2(m.a * src.a.vertices.x + m.c * src.a.vertices.y + m.tx,
                                          m.b * src.a.vertices.x + m.d * src.a.vertices.y + m.ty);
                    dst.b.vertices = Vec2(m.a * src.b.vertices.x + m.c * src.b.vertices.y + m.tx,
                                          m.b * src.b.vertices.x + m.d * src.b.vertices.y + m.ty);
                    dst.c.vertices = Vec2(m.a * src.c.vertices.x + m.c * src.c.vertices.y + m.tx,
                                          m.b * src.c.vertices.x + m.d * src.c.vertices.y + m.ty);
                }
            }
            else
            {
                for (unsigned int j = artDelegate.triangleIndex;
                     j < artDelegate.triangleIndex + artDelegate.triangleCount; j++)
                {
                    triangles[j].a.colors.a = opacity;
                    triangles[j].b.colors.a = opacity;
                    triangles[j].c.colors.a = opacity;
                }
            }
        }
    }
    _dirty = true;
}
