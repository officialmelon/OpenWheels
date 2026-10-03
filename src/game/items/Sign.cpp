#include "Sign.h"

#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Patch.h"

USING_NS_CC;

// @00613718 (D0; the complete destructor is LevelItem's)
Sign::~Sign()
{
}

// @00612d28
bool Sign::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);

    float y = 0.0f;
    float x = 0.0f;
    float rotation = 0.0f;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p2", &rotation);
    int signType = 0;
    bool showPost = true;
    element->intAttribute("p3", &signType);
    element->boolAttribute("p4", &showPost);
    getLevel()->convertPositionData(&x, &y);

    addSpritesWithSignType(signType, Vec2(x * getPtm(), y * getPtm()), rotation, showPost);
    return true;
}

// @00612f70
void Sign::addSpritesWithSignType(unsigned int signType, Vec2 positionPoints, float rotation,
                                  bool showPost)
{
    if (signType <= 4)
    {
        Sprite* arrow = Sprite::createWithSpriteFrameName("sign_arrow.png");
        switch (signType)
        {
        case 1:
            _mc = Sprite::createWithSpriteFrameName("sign_back_green_h.png");
            break;
        case 2:
            arrow->setRotation(90.0f);
            _mc = Sprite::createWithSpriteFrameName("sign_back_green_v.png");
            break;
        case 3:
            arrow->setRotation(180.0f);
            _mc = Sprite::createWithSpriteFrameName("sign_back_green_h.png");
            break;
        case 4:
            arrow->setRotation(-90.0f);
            _mc = Sprite::createWithSpriteFrameName("sign_back_green_v.png");
            break;
        default:
            // Type 0: no board is created and the (still null) _mc is used below, as in the
            // original.
            break;
        }
        _mc->setPosition(positionPoints);
        _mc->setRotation(rotation);
        getLevelItemsNode()->addChild(_mc);
        arrow->setPosition(Vec2(_mc->getTextureRect().size.width * 0.5f, _mc->getTextureRect().size.height * 0.5f));
        arrow->setTag(0);
        _mc->addChild(arrow);
    }
    else if (signType == 6)
    {
        _mc = Sprite::createWithSpriteFrameName("sign_stop.png");
        _mc->setPosition(positionPoints);
        _mc->setRotation(rotation);
        getLevelItemsNode()->addChild(_mc);
    }
    else if (signType == 5)
    {
        _mc = Sprite::createWithSpriteFrameName("sign_slow.png");
        _mc->setPosition(positionPoints);
        _mc->setRotation(rotation);
        getLevelItemsNode()->addChild(_mc);
    }
    else
    {
        _mc = Sprite::createWithSpriteFrameName("sign_back_yellow.png");
        _mc->setPosition(positionPoints);
        _mc->setRotation(rotation);
        getLevelItemsNode()->addChild(_mc);
        Sprite* decal = Sprite::createWithSpriteFrameName("sign_decal_" + patch::to_string(signType) + ".png");
        decal->setTag(0);
        decal->setPosition(Vec2(_mc->getTextureRect().size.width * 0.5f, _mc->getTextureRect().size.height * 0.5f));
        _mc->addChild(decal);
    }

    if (showPost)
    {
        Sprite* post = Sprite::createWithSpriteFrameName("sign_post.png");
        post->setTag(1);
        post->setPosition(Vec2(_mc->getTextureRect().size.width * 0.5f, _mc->getTextureRect().size.height * 0.5f));
        post->setAnchorPoint(Vec2(0.5f, 1.0f));
        _mc->addChild(post, -1);
    }
}

// @00613590
void Sign::setOpacity(float opacity)
{
    _mc->setOpacity((int)(opacity * 255.0f));
    Node* post = _mc->getChildByTag(1);
    Node* decal = _mc->getChildByTag(0);
    GLubyte mcOpacity = _mc->getOpacity();
    if (post)
    {
        post->setOpacity(mcOpacity);
    }
    if (decal)
    {
        decal->setOpacity(mcOpacity);
    }
}

// @00613654
float Sign::getOpacity()
{
    return _mc->getOpacity() == 255 ? 1.0f : 0.0f;
}

// @00613688
void Sign::removeSprites()
{
    _mc->removeFromParentAndCleanup(false);
}

// @0061369c
void Sign::paintWithOffsetPoints(Vec2 offset, float angleDegrees)
{
    _mc->setPosition(offset);
    _mc->setRotation(angleDegrees);
}
