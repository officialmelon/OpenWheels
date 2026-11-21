#include "qol/ColorFilter.h"

#include <algorithm>

#include "cocos2d.h"
#include "qol/QoL.h"

USING_NS_CC;

namespace qol {
namespace {

// Machado et al. (2009), severity 1: how a dichromat sees each colour (applied to the screen's
// sRGB values, which is close enough for a correction). Row-major.
const float kProtan[9] = {0.152286f, 1.052583f, -0.204868f, 0.114503f, 0.786281f, 0.099216f, -0.003882f, -0.048116f, 1.051998f};
const float kDeutan[9] = {0.367322f, 0.860646f, -0.227968f, 0.280085f, 0.672501f, 0.047413f, -0.011820f, 0.042940f, 0.968881f};
const float kTritan[9] = {1.255528f, -0.076749f, -0.178779f, -0.078411f, 0.930809f, 0.147602f, 0.004733f, 0.691367f, 0.303900f};

// "Daltonization": the part of the colour the viewer cannot see (c - sim(c)) is moved into the
// channels they can: result = c + E * (c - sim(c)) = (I + E (I - S)) c.
const float kShiftRG[9] = {0, 0, 0, 0.7f, 1, 0, 0.7f, 0, 1};  // red / green blind: into green and blue
const float kShiftBlue[9] = {1, 0, 0.7f, 0, 1, 0.7f, 0, 0, 0};  // blue blind: into red and green

void mul(const float* a, const float* b, float* out)
{
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            out[r * 3 + c] = a[r * 3] * b[c] + a[r * 3 + 1] * b[3 + c] + a[r * 3 + 2] * b[6 + c];
}

// Row-major correction matrix of a filter.
void matrixFor(ColorFilter filter, float* m)
{
    const float* sim = filter == ColorFilter::Protanopia ? kProtan : filter == ColorFilter::Deuteranopia ? kDeutan : kTritan;
    const float* shift = filter == ColorFilter::Tritanopia ? kShiftBlue : kShiftRG;
    float iMinusS[9], eTimes[9];
    for (int i = 0; i < 9; i++) iMinusS[i] = (i % 4 == 0 ? 1.0f : 0.0f) - sim[i];
    mul(shift, iMinusS, eTimes);
    for (int i = 0; i < 9; i++) m[i] = (i % 4 == 0 ? 1.0f : 0.0f) + eTimes[i];
}

const char* const kVertex =
    "attribute vec4 a_position;\n"
    "varying vec2 v_uv;\n"
    "void main() { gl_Position = a_position; v_uv = a_position.xy * 0.5 + 0.5; }\n";

const char* const kFragment =
    "#ifdef GL_ES\nprecision mediump float;\n#endif\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_screen;\n"
    "uniform mat3 u_matrix;\n"
    "uniform float u_contrast;\n"
    "uniform float u_saturation;\n"
    "void main() {\n"
    "  vec3 c = texture2D(u_screen, v_uv).rgb;\n"
    "  c = u_matrix * c;\n"
    "  float l = dot(c, vec3(0.299, 0.587, 0.114));\n"
    "  c = mix(vec3(l), c, u_saturation);\n"
    "  c = (c - 0.5) * u_contrast + 0.5;\n"
    "  gl_FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);\n"
    "}\n";

struct State
{
    GLProgram* program = nullptr;
    GLuint texture = 0;
    GLuint buffer = 0;
    int width = 0, height = 0;
    bool failed = false;
};

State& state()
{
    static State s;
    return s;
}

void drawFiltered(ColorFilter filter)
{
    State& s = state();
    if (s.failed) return;

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    if (vp[2] <= 0 || vp[3] <= 0) return;

    if (!s.program)
    {
        s.program = GLProgram::createWithByteArrays(kVertex, kFragment);
        if (!s.program)
        {
            s.failed = true;
            return;
        }
        s.program->retain();
        const GLfloat quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
        glGenBuffers(1, &s.buffer);
        glBindBuffer(GL_ARRAY_BUFFER, s.buffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    if (!s.texture || s.width != vp[2] || s.height != vp[3])
    {
        if (!s.texture) glGenTextures(1, &s.texture);
        GL::bindTexture2D(s.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vp[2], vp[3], 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        s.width = vp[2];
        s.height = vp[3];
    }
    GL::bindTexture2D(s.texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vp[0], vp[1], vp[2], vp[3]);

    float m[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    float contrast = 1.0f, saturation = 1.0f;
    if (filter == ColorFilter::HighContrast)
    {
        contrast = 1.35f;
        saturation = 1.3f;
    }
    else
    {
        matrixFor(filter, m);
    }
    // glUniformMatrix3fv wants column-major (GLES2: no transpose).
    const float columnMajor[9] = {m[0], m[3], m[6], m[1], m[4], m[7], m[2], m[5], m[8]};

    const GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
    if (depthTest) glDisable(GL_DEPTH_TEST);
    GL::blendFunc(GL_ONE, GL_ZERO);
    GL::bindVAO(0);
    s.program->use();
    s.program->setUniformLocationWith1i(s.program->getUniformLocationForName("u_screen"), 0);
    glUniformMatrix3fv(s.program->getUniformLocationForName("u_matrix"), 1, GL_FALSE, columnMajor);
    s.program->setUniformLocationWith1f(s.program->getUniformLocationForName("u_contrast"), contrast);
    s.program->setUniformLocationWith1f(s.program->getUniformLocationForName("u_saturation"), saturation);
    GL::enableVertexAttribs(GL::VERTEX_ATTRIB_FLAG_POSITION);
    glBindBuffer(GL_ARRAY_BUFFER, s.buffer);
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (depthTest) glEnable(GL_DEPTH_TEST);
}

}  // namespace

void installColorFilter()
{
    static bool installed = false;
    if (installed) return;
    installed = true;
    Director::getInstance()->getEventDispatcher()->addCustomEventListener(Director::EVENT_AFTER_DRAW, [](EventCustom*) {
        const ColorFilter filter = colorFilter();
        if (filter != ColorFilter::Off) drawFiltered(filter);
    });
}

}  // namespace qol
