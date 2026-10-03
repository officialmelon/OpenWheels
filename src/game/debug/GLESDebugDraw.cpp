#include "GLESDebugDraw.h"

USING_NS_CC;

// The game's copy of the cocos2d-x test-bed GLES-Render. Differences from the 3.17.2 test-bed:
// every outline/fill uses alpha 0.7 (the test-bed uses 1 and 0.5), DrawAABB keeps alpha 1, and the
// circle vertex buffers come from a plain new[] (the test-bed uses new (std::nothrow)).
// The destructor is implicit (deleting destructor @005beae4).

namespace
{
const float kDebugDrawAlpha = 0.7f;
} // namespace

// @005bdfbc
GLESDebugDraw::GLESDebugDraw()
    : mRatio(1.0f)
{
    initShader();
}

// @005bdff8
void GLESDebugDraw::initShader()
{
    mShaderProgram = GLProgramCache::getInstance()->getGLProgram(GLProgram::SHADER_NAME_POSITION_U_COLOR);
    mColorLocation = glGetUniformLocation(mShaderProgram->getProgram(), "u_color");
}

// @005be144
GLESDebugDraw::GLESDebugDraw(float32 ratio)
    : mRatio(ratio)
{
    initShader();
}

// @005be188
void GLESDebugDraw::DrawPolygon(const b2Vec2* oldVertices, int vertexCount, const b2Color& color)
{
    mShaderProgram->use();
    mShaderProgram->setUniformsForBuiltins();

    b2Vec2* vertices = new b2Vec2[vertexCount];
    for (int i = 0; i < vertexCount; i++)
    {
        vertices[i] = oldVertices[i];
        vertices[i] *= mRatio;
    }

    mShaderProgram->setUniformLocationWith4f(mColorLocation, color.r, color.g, color.b, kDebugDrawAlpha);

    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glDrawArrays(GL_LINE_LOOP, 0, vertexCount);

    CC_INCREMENT_GL_DRAWN_BATCHES_AND_VERTICES(1, vertexCount);

    delete[] vertices;
}

// @005be294
void GLESDebugDraw::DrawSolidPolygon(const b2Vec2* oldVertices, int vertexCount, const b2Color& color)
{
    mShaderProgram->use();
    mShaderProgram->setUniformsForBuiltins();

    b2Vec2* vertices = new b2Vec2[vertexCount];
    for (int i = 0; i < vertexCount; i++)
    {
        vertices[i] = oldVertices[i];
        vertices[i] *= mRatio;
    }

    mShaderProgram->setUniformLocationWith4f(mColorLocation, color.r * 0.5f, color.g * 0.5f, color.b * 0.5f,
                                             kDebugDrawAlpha);

    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glDrawArrays(GL_TRIANGLE_FAN, 0, vertexCount);

    mShaderProgram->setUniformLocationWith4f(mColorLocation, color.r, color.g, color.b, kDebugDrawAlpha);
    glDrawArrays(GL_LINE_LOOP, 0, vertexCount);

    CC_INCREMENT_GL_DRAWN_BATCHES_AND_VERTICES(2, vertexCount * 2);

    delete[] vertices;
}

// @005be3e0
void GLESDebugDraw::DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color)
{
    mShaderProgram->use();
    mShaderProgram->setUniformsForBuiltins();

    const float32 k_segments = 16.0f;
    int vertexCount = 16;
    const float32 k_increment = 2.0f * b2_pi / k_segments;
    float32 theta = 0.0f;

    GLfloat* glVertices = new GLfloat[vertexCount * 2];
    for (int i = 0; i < k_segments; ++i)
    {
        b2Vec2 v = center + radius * b2Vec2(cosf(theta), sinf(theta));
        glVertices[i * 2] = v.x * mRatio;
        glVertices[i * 2 + 1] = v.y * mRatio;
        theta += k_increment;
    }

    mShaderProgram->setUniformLocationWith4f(mColorLocation, color.r, color.g, color.b, kDebugDrawAlpha);
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, glVertices);
    glDrawArrays(GL_LINE_LOOP, 0, vertexCount);

    CC_INCREMENT_GL_DRAWN_BATCHES_AND_VERTICES(1, vertexCount);

    delete[] glVertices;
}

// @005be518
void GLESDebugDraw::DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis,
                                    const b2Color& color)
{
    mShaderProgram->use();
    mShaderProgram->setUniformsForBuiltins();

    const float32 k_segments = 16.0f;
    int vertexCount = 16;
    const float32 k_increment = 2.0f * b2_pi / k_segments;
    float32 theta = 0.0f;

    GLfloat* glVertices = new GLfloat[vertexCount * 2];
    for (int i = 0; i < k_segments; ++i)
    {
        b2Vec2 v = center + radius * b2Vec2(cosf(theta), sinf(theta));
        glVertices[i * 2] = v.x * mRatio;
        glVertices[i * 2 + 1] = v.y * mRatio;
        theta += k_increment;
    }

    mShaderProgram->setUniformLocationWith4f(mColorLocation, color.r * 0.5f, color.g * 0.5f, color.b * 0.5f,
                                             kDebugDrawAlpha);
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, glVertices);
    glDrawArrays(GL_TRIANGLE_FAN, 0, vertexCount);

    mShaderProgram->setUniformLocationWith4f(mColorLocation, color.r, color.g, color.b, kDebugDrawAlpha);
    glDrawArrays(GL_LINE_LOOP, 0, vertexCount);

    // Draw the axis line
    DrawSegment(center, center + radius * axis, color);

    CC_INCREMENT_GL_DRAWN_BATCHES_AND_VERTICES(2, vertexCount * 2);

    delete[] glVertices;
}

// @005be6f8
void GLESDebugDraw::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color)
{
    mShaderProgram->use();
    mShaderProgram->setUniformsForBuiltins();

    mShaderProgram->setUniformLocationWith4f(mColorLocation, color.r, color.g, color.b, kDebugDrawAlpha);

    GLfloat glVertices[] = {
        p1.x * mRatio, p1.y * mRatio,
        p2.x * mRatio, p2.y * mRatio,
    };
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, glVertices);
    glDrawArrays(GL_LINES, 0, 2);

    CC_INCREMENT_GL_DRAWN_BATCHES_AND_VERTICES(1, 2);
}

// @005be7f0
void GLESDebugDraw::DrawTransform(const b2Transform& xf)
{
    b2Vec2 p1 = xf.p, p2;
    const float32 k_axisScale = 0.4f;
    p2 = p1 + k_axisScale * xf.q.GetXAxis();
    DrawSegment(p1, p2, b2Color(1, 0, 0));

    p2 = p1 + k_axisScale * xf.q.GetYAxis();
    DrawSegment(p1, p2, b2Color(0, 1, 0));
}

// @005be8dc
void GLESDebugDraw::DrawPoint(const b2Vec2& p, float32 size, const b2Color& color)
{
    mShaderProgram->use();
    mShaderProgram->setUniformsForBuiltins();

    mShaderProgram->setUniformLocationWith4f(mColorLocation, color.r, color.g, color.b, kDebugDrawAlpha);

    GLfloat glVertices[] = {
        p.x * mRatio, p.y * mRatio,
    };
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, glVertices);
    glDrawArrays(GL_POINTS, 0, 1);

    CC_INCREMENT_GL_DRAWN_BATCHES_AND_VERTICES(1, 1);
}

// @005be9c0
void GLESDebugDraw::DrawString(int x, int y, const char* string, ...)
{
    // Unsupported (as in the test-bed).
}

// @005be9e4
void GLESDebugDraw::DrawAABB(b2AABB* aabb, const b2Color& color)
{
    mShaderProgram->use();
    mShaderProgram->setUniformsForBuiltins();

    mShaderProgram->setUniformLocationWith4f(mColorLocation, color.r, color.g, color.b, 1);

    GLfloat glVertices[] = {
        aabb->lowerBound.x * mRatio, aabb->lowerBound.y * mRatio,
        aabb->upperBound.x * mRatio, aabb->lowerBound.y * mRatio,
        aabb->upperBound.x * mRatio, aabb->upperBound.y * mRatio,
        aabb->lowerBound.x * mRatio, aabb->upperBound.y * mRatio,
    };
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, glVertices);
    glDrawArrays(GL_LINE_LOOP, 0, 4);

    CC_INCREMENT_GL_DRAWN_BATCHES_AND_VERTICES(1, 4);
}
