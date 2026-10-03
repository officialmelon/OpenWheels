#include "qol/BloodCompositor.h"

#include "qol/QoL.h"

USING_NS_CC;

namespace qol {
namespace {

// Particle cloud -> blob: a 9-tap blur of the coverage, then a soft threshold (Flash: BlurFilter
// 4x4 then threshold at alpha 0x54). Mode 1 adds the bevel lighting of blood setting 4 from the
// blurred coverage's gradient.
const char* const kFragment = R"(
#ifdef GL_ES
precision mediump float;
#endif
varying vec4 v_fragmentColor;
varying vec2 v_texCoord;
uniform vec2 u_texel;
uniform float u_radius;
uniform float u_mode;

float cov(vec2 uv) { return texture2D(CC_Texture0, uv).a; }

float blurred(vec2 uv) {
    vec2 r = u_texel * u_radius;
    vec2 q = r * 0.7071;
    float s = cov(uv) * 0.2;
    s += (cov(uv + vec2(r.x, 0.0)) + cov(uv - vec2(r.x, 0.0)) + cov(uv + vec2(0.0, r.y)) + cov(uv - vec2(0.0, r.y))) * 0.12;
    s += (cov(uv + q) + cov(uv - q) + cov(uv + vec2(q.x, -q.y)) + cov(uv + vec2(-q.x, q.y))) * 0.08;
    return s;
}

void main() {
    float a = blurred(v_texCoord);
    float m = smoothstep(0.16, 0.26, a);
    if (m <= 0.0) {
        gl_FragColor = vec4(0.0);
        return;
    }
    if (u_mode < 0.5) {
        vec3 base = vec3(0.60, 0.0, 0.0);
        gl_FragColor = vec4(base, 1.0) * (m * 0.85);
        return;
    }
    vec2 d = u_texel * u_radius;
    float gx = blurred(v_texCoord + vec2(d.x, 0.0)) - blurred(v_texCoord - vec2(d.x, 0.0));
    float gy = blurred(v_texCoord + vec2(0.0, d.y)) - blurred(v_texCoord - vec2(0.0, d.y));
    vec3 n = normalize(vec3(-gx * 2.5, -gy * 2.5, 1.0));
    vec3 l = normalize(vec3(-0.35, 0.75, 0.6));
    float diffuse = clamp(dot(n, l), 0.0, 1.0);
    float spec = pow(clamp(dot(n, normalize(l + vec3(0.0, 0.0, 1.0))), 0.0, 1.0), 28.0);
    float depth = smoothstep(0.3, 0.9, a);
    vec3 deep = vec3(0.32, 0.0, 0.01);
    vec3 fresh = vec3(0.62, 0.02, 0.03);
    vec3 c = mix(fresh, deep, depth) * (0.55 + 0.6 * diffuse) + vec3(1.0, 0.86, 0.86) * spec * 0.6;
    gl_FragColor = vec4(c, 1.0) * m;
}
)";

GLProgram* program() {
    static GLProgram* p = nullptr;
    if (!p) {
        p = GLProgram::createWithByteArrays(ccPositionTextureColor_noMVP_vert, kFragment);
        p->retain();
    }
    return p;
}

// One offscreen target per layer, kept on the layer (released with it).
RenderTexture* targetFor(Node* layer) {
    const Size win = Director::getInstance()->getWinSize();
    auto* rt = dynamic_cast<RenderTexture*>(layer->getUserObject());
    if (rt && rt->getSprite()->getContentSize().equals(win)) return rt;
    rt = RenderTexture::create((int)win.width, (int)win.height, Texture2D::PixelFormat::RGBA8888);
    if (!rt) return nullptr;
    Sprite* sprite = rt->getSprite();
    sprite->setPosition(win / 2.0f);
    sprite->getTexture()->setAntiAliasTexParameters();
    GLProgramState* state = GLProgramState::create(program());
    sprite->setGLProgramState(state);
    sprite->setBlendFunc(BlendFunc::ALPHA_PREMULTIPLIED);
    layer->setUserObject(rt);
    return rt;
}

}  // namespace

bool BloodCompositor::active() {
    const BloodStyle s = bloodStyle();
    return s == BloodStyle::Liquid || s == BloodStyle::Realistic;
}

void BloodCompositor::draw(Node* layer, const std::vector<Node*>& blood, Renderer* renderer, const Mat4& layerTransform,
                           uint32_t flags) {
    RenderTexture* rt = targetFor(layer);
    if (!rt) return;
    rt->beginWithClear(0.0f, 0.0f, 0.0f, 0.0f);
    for (Node* b : blood) b->visit(renderer, layerTransform, flags | Node::FLAGS_DIRTY_MASK);
    rt->end();

    Sprite* sprite = rt->getSprite();
    const Size px = sprite->getTexture()->getContentSizeInPixels();
    GLProgramState* state = sprite->getGLProgramState();
    state->setUniformVec2("u_texel", Vec2(1.0f / px.width, 1.0f / px.height));
    // Flash blurs by 4 px of an 900 px wide stage; keep that share of the screen width.
    state->setUniformFloat("u_radius", std::max(1.5f, px.width * (3.0f / 900.0f)));
    state->setUniformFloat("u_mode", bloodStyle() == BloodStyle::Realistic ? 1.0f : 0.0f);
    sprite->visit(renderer, Mat4::IDENTITY, Node::FLAGS_DIRTY_MASK);
}

}  // namespace qol
