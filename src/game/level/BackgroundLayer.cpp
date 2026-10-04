#include "BackgroundLayer.h"

#include "2d/CCLayer.h"
#include "2d/CCRenderTexture.h"
#include "2d/CCSprite.h"
#include "2d/CCSpriteFrameCache.h"
#include "Backdrop.h"
#include "LevelDataElement.h"
#include "online/FlashCity.h"     // ONLINE (PC addition)
#include "online/FlashRuntime.h"  // ONLINE (PC addition)
#include "base/CCDirector.h"
#include "renderer/CCGLProgram.h"
#include "renderer/CCGLProgramCache.h"
#include "renderer/CCTexture2D.h"
#include "renderer/ccGLStateCache.h"

USING_NS_CC;

// FUN_00584cc8 is the std::vector<Backdrop*> length error (library code instantiated by
// push_back).

// @00582530
BackgroundLayer* BackgroundLayer::create()
{
    BackgroundLayer* layer = new BackgroundLayer();
    layer->autorelease();
    return layer;
}

// @00582578
BackgroundLayer* BackgroundLayer::create(Size stageSize, backgrounds type, long color,
                                         float ptmRatio, LevelDataElement* parameters)
{
    BackgroundLayer* layer = new BackgroundLayer();
    layer->init(stageSize, type, color, ptmRatio, parameters);
    layer->autorelease();
    return layer;
}

// @0058264c
bool BackgroundLayer::init(Size stageSize, backgrounds type, long color, float ptmRatio,
                           LevelDataElement* parameters)
{
    Size winSize = Director::getInstance()->getWinSize();
    _type = type;
    _stageSize = stageSize;
    _ptmRatio = ptmRatio;

    Color4B bgColor((color >> 16), (color >> 8), color, 255);
    // The original tests the four bytes of bgColor inline (no Color4B::operator!= call; this
    // expression compiles to the same vectorised test). Alpha is 255, so the colour layer is
    // always added for BackgroundNone.
    if (_type == BackgroundNone && (bgColor.r != 0 || bgColor.g != 0 || bgColor.b != 0 || bgColor.a != 0))
    {
        addChild(LayerColor::create(bgColor, winSize.width, winSize.height));
    }

    create(parameters);
    return true;
}

// @00582748
BackgroundLayer::BackgroundLayer()
{
    _batchNode = nullptr;
    _type = BackgroundNone;
    _stageSize = Size::ZERO;
    _ptmRatio = 1.0f;
}

// @005827e4 (D1), @00582878 (D0)
BackgroundLayer::~BackgroundLayer()
{
    for (std::vector<Backdrop*>::iterator it = _backdrops.begin(); it != _backdrops.end(); ++it)
    {
        if (*it != nullptr)
        {
            delete *it;
        }
    }
    _backdrops.clear();
}

// @0058289c
void BackgroundLayer::create(LevelDataElement* parameters)
{
    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA8888);

    switch (_type)
    {
        case BackgroundGreenHills:
        {
            addFillBGWithFile("backgrounds/blueGradient_1x768.png");
            addSpriteBatchNode("backgrounds/greenhills");

            Vec2 pos = Vec2(5.44f, 4.47839975f) * _ptmRatio;
            Sprite* sprite = Sprite::createWithSpriteFrameName("greenSource3.png");
            sprite->setPosition(pos);
            sprite->setAnchorPoint(Vec2::ZERO);
            sprite->setScale(3.5f);
            Backdrop* backdrop = new Backdrop(sprite, pos, 0.01f);
            _batchNode->addChild(sprite);
            _backdrops.push_back(backdrop);

            pos = Vec2(-8.5208f, 5.37679958f) * _ptmRatio;
            sprite = Sprite::createWithSpriteFrameName("greenSource3.png");
            sprite->setPosition(pos);
            sprite->setAnchorPoint(Vec2::ZERO);
            sprite->setScale(3.5f);
            backdrop = new Backdrop(sprite, pos, 0.01f);
            _batchNode->addChild(sprite);
            _backdrops.push_back(backdrop);

            pos = Vec2(-0.512f, -0.512f) * _ptmRatio;
            sprite = Sprite::createWithSpriteFrameName("greenSource2.png");
            sprite->setPosition(pos);
            sprite->setAnchorPoint(Vec2::ZERO);
            sprite->setScale(3.5f);
            backdrop = new Backdrop(sprite, pos, 0.049f);
            _batchNode->addChild(sprite);
            _backdrops.push_back(backdrop);

            pos = Vec2(29.677599f, -0.512f) * _ptmRatio;
            sprite = Sprite::createWithSpriteFrameName("greenSource2.png");
            sprite->setPosition(pos);
            sprite->setAnchorPoint(Vec2::ZERO);
            sprite->setScaleX(-3.5f);
            sprite->setScaleY(3.5f);
            backdrop = new Backdrop(sprite, pos, 0.049f);
            _batchNode->addChild(sprite);
            _backdrops.push_back(backdrop);

            pos = Vec2(-0.256f, -0.256f) * _ptmRatio;
            sprite = Sprite::createWithSpriteFrameName("greenSource1.png");
            sprite->setPosition(pos);
            sprite->setAnchorPoint(Vec2::ZERO);
            sprite->setScale(3.5f);
            backdrop = new Backdrop(sprite, pos, 0.1f);
            _batchNode->addChild(sprite);
            _backdrops.push_back(backdrop);

            pos = Vec2(63.856f, -2.4f) * _ptmRatio;
            sprite = Sprite::createWithSpriteFrameName("greenSource1.png");
            sprite->setPosition(pos);
            sprite->setAnchorPoint(Vec2::ZERO);
            sprite->setScaleX(-3.5f);
            sprite->setScaleY(3.5f);
            backdrop = new Backdrop(sprite, pos, 0.1f);
            _batchNode->addChild(sprite);
            _backdrops.push_back(backdrop);
            break;
        }
        case BackgroundCity:
            if (online::flashLevel())
            {
                // ONLINE (PC addition): the browser game's city backdrops (online/FlashCity.h),
                // moved by online::updateCityBackground (they follow the QoL camera zoom);
                // without the generated art, the night horizon (Android has no city gradient).
                std::vector<online::CityBackdropPiece> pieces =
                    online::createCityBackground(this, _ptmRatio);
                if (pieces.empty())
                {
                    addFillBGWithFile("backgrounds/nightHorizon_1x768.png");
                }
                break;
            }
            addFillBGWithFile("backgrounds/cityGradient_1x768.png");
            break;
        case BackgroundLab:
        {
            LayerGradient* gradient = createGradient(parameters, 0.5f);
            addChild(gradient);
            Sprite* tile = createTilingSprite("backgrounds/lab/lab_tile.png", 4.0f);
            addChild(tile, gradient->getLocalZOrder() - 1);
            break;
        }
        case BackgroundBricks:
        {
            LayerGradient* gradient = createGradient(parameters, 1.0f);
            addChild(gradient);
            Sprite* tile = createTilingSprite("backgrounds/bricks_256_blur1.png", 6.0f);
            addChild(tile, gradient->getLocalZOrder() - 1);
            break;
        }
        case BackgroundNightHorizon:
            addFillBGWithFile("backgrounds/nightHorizon_1x768.png");
            break;
        case BackgroundGreenHillsSimple:
        {
            addFillBGWithFile("backgrounds/blueGradient_1x768.png");
            addSpriteBatchNode("backgrounds/greenhills");

            Vec2 pos = Vec2(5.44f, 4.47839975f) * _ptmRatio;
            Sprite* sprite = Sprite::createWithSpriteFrameName("greenSource3.png");
            sprite->setPosition(pos);
            sprite->setAnchorPoint(Vec2::ZERO);
            sprite->setScale(4.0f);
            Backdrop* backdrop = new Backdrop(sprite, pos, 0.01f);
            _batchNode->addChild(sprite);
            _backdrops.push_back(backdrop);

            pos = Vec2(-8.5208f, 5.37679958f) * _ptmRatio;
            sprite = Sprite::createWithSpriteFrameName("greenSource3.png");
            sprite->setPosition(pos);
            sprite->setAnchorPoint(Vec2::ZERO);
            sprite->setScale(4.0f);
            backdrop = new Backdrop(sprite, pos, 0.01f);
            _batchNode->addChild(sprite);
            _backdrops.push_back(backdrop);
            break;
        }
        case BackgroundSunset:
        {
            Size winSize = Director::getInstance()->getWinSize();
            addFillBGWithFile("backgrounds/sunsetGradient_1x400.png");
            addSpriteBatchNode("backgrounds/sunset");

            // The sun has no Backdrop (it does not scroll) and keeps the default anchor point.
            Vec2 sunPos = Vec2(winSize.width * 0.66f, winSize.height * 0.5f);
            Sprite* sprite = Sprite::createWithSpriteFrameName("sunset_sun.png");
            sprite->setPosition(sunPos);
            sprite->setScale(6.0f);
            _batchNode->addChild(sprite);

            Vec2 pos = Vec2(2.45f, 1.3f) * _ptmRatio;
            sprite = Sprite::createWithSpriteFrameName("sunset_mountain2.png");
            sprite->setPosition(pos);
            sprite->setAnchorPoint(Vec2::ZERO);
            sprite->setScale(7.0f);
            Backdrop* backdrop = new Backdrop(sprite, pos, 0.0275f);
            _batchNode->addChild(sprite);
            _backdrops.push_back(backdrop);

            pos = Vec2(-4.5f, 3.3f) * _ptmRatio;
            sprite = Sprite::createWithSpriteFrameName("sunset_mountain1.png");
            sprite->setPosition(pos);
            sprite->setAnchorPoint(Vec2::ZERO);
            sprite->setScale(7.0f);
            backdrop = new Backdrop(sprite, pos, 0.045f);
            _batchNode->addChild(sprite);
            _backdrops.push_back(backdrop);
            break;
        }
        case BackgroundDots:
        {
            LayerGradient* gradient = createGradient(parameters, 0.5f);
            addChild(gradient);
            Sprite* tile = createTilingSprite("backgrounds/dots_tile.png", 2.0f);
            addChild(tile, gradient->getLocalZOrder() - 1);
            break;
        }
        default:
            break;
    }

    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA4444);
}

// @00584214
Sprite* BackgroundLayer::addFillBGWithFile(std::string file)
{
    Size winSize = Director::getInstance()->getWinSize();
    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA8888);
    Sprite* sprite = Sprite::create(file);
    sprite->setScaleX(winSize.width / sprite->getTextureRect().size.width);
    sprite->setScaleY(winSize.height / sprite->getTextureRect().size.height);
    sprite->setPosition(winSize.width * 0.5f, winSize.height * 0.5f);
    sprite->setTag(0);
    addChild(sprite);
    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA4444);
    return sprite;
}

// @00584314
void BackgroundLayer::addSpriteBatchNode(std::string name)
{
    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA8888);
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile(name + ".plist");
    _batchNode = Node::create();
    addChild(_batchNode);
    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA4444);
}

// @005844a8
LayerGradient* BackgroundLayer::createGradient(LevelDataElement* parameters, float endGradientAlpha)
{
    Color4B startColor = Color4B::GREEN;
    if (parameters != nullptr)
    {
        int color = 0;
        parameters->intAttribute("b0", &color);
        float alpha = 0.0f;
        parameters->floatAttribute("b1", &alpha);
        alpha = alpha / 100.0f;
        startColor = Color4B((GLubyte)(color >> 16), (GLubyte)(color >> 8), (GLubyte)color,
                             (GLubyte)(int)(alpha * 255.0f));
    }
    Color4B endColor = startColor;
    endColor.a = (GLubyte)(int)(startColor.a * endGradientAlpha);
    return LayerGradient::create(startColor, endColor);
}

// @00584620
Sprite* BackgroundLayer::createTilingSprite(std::string file, float scale)
{
    scale = Director::getInstance()->getContentScaleFactor() * scale;
    Sprite* sprite = Sprite::create(file, Rect(0.0f, 0.0f, _stageSize.width / scale,
                                               _stageSize.height / scale));
    sprite->setAnchorPoint(Vec2::ZERO);
    sprite->setScale(scale);
    Texture2D::TexParams params = {GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT};
    sprite->getTexture()->setTexParameters(params);
    _backdrops.push_back(new Backdrop(sprite, Vec2(0.0f, 0.0f), 1.0f / scale));
    return sprite;
}

// @005848b4
// Renders a transparent-to-black vertical gradient into a RenderTexture, multiplies the texture
// `file` over it and returns a sprite of the result. Not used by create(LevelDataElement*).
Sprite* BackgroundLayer::spriteWithTextureFile(std::string file, Color4F color, Size size,
                                               float endGradientAlpha)
{
    RenderTexture* rt = RenderTexture::create((int)size.width, (int)size.height);
    rt->beginWithClear(color.r, color.g, color.b, color.a);

    setGLProgram(GLProgramCache::getInstance()->getGLProgram(GLProgram::SHADER_NAME_POSITION_COLOR));
    getGLProgram()->use();
    getGLProgram()->setUniformsForBuiltins(_modelViewTransform);

    Vec2 vertices[4];
    Color4F colors[4];
    int nVertices = 0;
    vertices[nVertices] = Vec2(0.0f, 0.0f);
    colors[nVertices++] = Color4F(0.0f, 0.0f, 0.0f, 0.0f);
    vertices[nVertices] = Vec2(size.width, 0.0f);
    colors[nVertices++] = Color4F(0.0f, 0.0f, 0.0f, 0.0f);
    vertices[nVertices] = Vec2(0.0f, size.height);
    colors[nVertices++] = Color4F(0.0f, 0.0f, 0.0f, endGradientAlpha);
    vertices[nVertices] = Vec2(size.width, size.height);
    colors[nVertices++] = Color4F(0.0f, 0.0f, 0.0f, endGradientAlpha);

    GL::enableVertexAttribs(GL::VERTEX_ATTRIB_FLAG_POSITION | GL::VERTEX_ATTRIB_FLAG_COLOR);
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_FLOAT, GL_FALSE, 0, colors);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei)nVertices);

    Sprite* noise = Sprite::create(file, Rect(0.0f, 0.0f, size.width, size.height));
    noise->setScaleY(-1.0f);
    noise->setPosition(Vec2(size.width * 0.5f, size.height * 0.5f));
    noise->setBlendFunc({GL_DST_COLOR, GL_ZERO});
    Texture2D::TexParams params = {GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT};
    noise->getTexture()->setTexParameters(params);
    noise->visit();

    rt->end();
    return Sprite::createWithTexture(rt->getSprite()->getTexture());
}

// @00584c70
void BackgroundLayer::update(Vec2 pos)
{
    for (std::vector<Backdrop*>::iterator it = _backdrops.begin(); it != _backdrops.end(); ++it)
    {
        (*it)->update(pos);
    }
    if (online::flashLevel())
    {
        online::updateCityBackground(this, pos);  // ONLINE (PC addition): browser city backdrops
    }
}
