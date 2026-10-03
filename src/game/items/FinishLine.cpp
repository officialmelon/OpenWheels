#include "FinishLine.h"

#include <new>

#include "CharacterB2D.h"
#include "EmitterNode.h"
#include "FlowEmitter.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Patch.h"
#include "Session.h"

USING_NS_CC;

// @005d06d4 (header-inline in the original; emitted in LevelB2D's TU)
FinishLine* FinishLine::create(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    FinishLine* finishLine = new (std::nothrow) FinishLine();
    if (finishLine)
    {
        if (finishLine->init(element, groupBody, groupOffset))
        {
            finishLine->autorelease();
        }
        else
        {
            delete finishLine;
            finishLine = nullptr;
        }
    }
    return finishLine;
}

// @005b3e10 (D2), @005b3e78 (D0)
FinishLine::~FinishLine()
{
    if (_characterDeadListener)
    {
        Director::getInstance()->getEventDispatcher()->removeEventListener(_characterDeadListener);
        _characterDeadListener->release();
        _characterDeadListener = nullptr;
    }
}

// @005b33ac
bool FinishLine::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);

    float y = 0.0f;
    float x = 0.0f;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    getLevel()->convertPositionAndRotationData(&x, &y, nullptr);
    _updateAnimation = true;
    _frameIndex = 1;

    Node* itemsNode = getLevelItemsNode();
    _mc = Node::create();
    _mc->setPosition(Vec2(x * getPtm(), y * getPtm()));
    itemsNode->addChild(_mc);

    createBody(b2Vec2(x, y));
    createSprites();
    addToBeginContact(_shape);
    addToEndContact(_shape);
    getLevel()->addToFrameActions(this);
    getLevel()->addToActions(this);

    // @005b418c ($_0)
    _characterDeadListener = Director::getInstance()->getEventDispatcher()->addCustomEventListener(
        "characterDead", [this](EventCustom*) { characterDead(); });
    _characterDeadListener->retain();
    return true;
}

// @005b3674
void FinishLine::createBody(b2Vec2 position)
{
    b2FixtureDef fixtureDef;
    fixtureDef.friction = 0.3f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 75.0f;
    fixtureDef.filter.categoryBits = 8;
    b2PolygonShape shape;
    shape.SetAsBox(3.2f, 0.32f, position, 0.0f);
    fixtureDef.shape = &shape;
    _shape = getLevelBody()->CreateFixture(&fixtureDef);

    _upperBounds = b2Vec2(position.x + 3.2f, position.y + 3.2f);
    _lowerBounds = b2Vec2(position.x - 3.2f, position.y);
    _sparkCoords = Vec2(position.x * getPtm() - 3.136f * getPtm(),
                        position.y * getPtm() + 3.68f * getPtm());
}

// @005b37f8
void FinishLine::createSprites()
{
    float ptm = getPtm();

    Sprite* pole = Sprite::createWithSpriteFrameName("finishline_pole.png");
    float segment = ptm * 0.32f;
    float left = segment * 20.0f * -0.5f;
    pole->setScaleY(ptm * 3.472f / pole->getTextureRect().size.height);
    float poleX = ptm * 0.056f + left;
    pole->setPosition(Vec2(poleX, ptm * 1.92f));
    _mc->addChild(pole);

    Sprite* cap = Sprite::createWithSpriteFrameName("finishline_cap.png");
    cap->setPosition(Vec2(poleX, ptm * 3.6f));
    _mc->addChild(cap);

    float x = segment * 0.5f + left;
    for (int i = 20; i != 0; i--)
    {
        Sprite* banner = Sprite::createWithSpriteFrameName("finishline_banner.png");
        banner->setPosition(Vec2(x, 0.0f));
        banner->setScaleX(1.05f);
        x = segment + x;
        _mc->addChild(banner);
    }

    for (int i = 1; i < 19; i++)
    {
        _frames.push_back(SpriteFrameCache::getInstance()->getSpriteFrameByName(
            "flag_frame_" + patch::to_string(i) + ".png"));
    }

    _flagSprite = Sprite::createWithSpriteFrameName("flag_frame_1.png");
    _flagSprite->setScale(1.5f);
    _flagSprite->setPosition(Vec2(-2.288f * ptm, 3.04f * ptm));
    _mc->addChild(_flagSprite);
}

// @005b3e9c
void FinishLine::characterDead()
{
    if (getLevel()->getCharacter()->getDead())
    {
        removeListeners();
    }
}

// @005b3f14
void FinishLine::removeListeners()
{
    Director::getInstance()->getEventDispatcher()->removeEventListener(_characterDeadListener);
    _characterDeadListener->release();
    _characterDeadListener = nullptr;
    getLevel()->removeFromActions(this);
    removeBeginContact(_shape);
    removeEndContact(_shape);
}

// @005b3f70
void FinishLine::actions()
{
    if (_contactCount == 0)
    {
        return;
    }
    CharacterB2D* character = getLevel()->getCharacter();
    if (character->getDead())
    {
        return;
    }
    b2Vec2 position = character->getFocus()->GetPosition();
    if (_lowerBounds.x < position.x && _lowerBounds.y < position.y && position.x < _upperBounds.x &&
        position.y < _upperBounds.y)
    {
        removeListeners();
        getLevel()->levelCompleted();
        EmitterNode* particles = getSession()->getParticlesMidground();
        if (particles)
        {
            Emitter* sparkles1 = FlowEmitter::createSparkleFlow(_sparkCoords, 65.0f);
            Emitter* sparkles2 = FlowEmitter::createSparkleFlow(_sparkCoords, 115.0f);
            particles->addChild(sparkles1);
            particles->addChild(sparkles2);
        }
    }
}

// @005b40ac
void FinishLine::frameAction()
{
    if (_updateAnimation)
    {
        _frameIndex++;
        if (_frameIndex == _frames.size())
        {
            _frameIndex = 0;
        }
        _flagSprite->setSpriteFrame(_frames[_frameIndex]);
    }
    _updateAnimation = !_updateAnimation;
}

// @005b4118
void FinishLine::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    _contactCount++;
}

// @005b4128
void FinishLine::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    _contactCount--;
}
