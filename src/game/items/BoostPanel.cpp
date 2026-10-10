#include "BoostPanel.h"

#include <algorithm>
#include <cmath>

#include "DestructionListener.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Patch.h"
#include "Session.h"
#include "Sound.h"
#include "online/FlashPhysics.h"  // ONLINE (PC addition)

USING_NS_CC;

// @00587108
BoostPanel::BoostPanel()
    : _sinVal(0.0f),
      _cosVal(0.0f),
      _mc(nullptr),
      _sensor(nullptr),
      _bodies(),
      _frameCounter(0),
      _frameIndex(0),
      _power(0),
      _panels(),
      _sound(nullptr),
      _frames()
{
}

// @0058715c (D1), @005871bc (D0)
BoostPanel::~BoostPanel()
{
}

// @005871e0
bool BoostPanel::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);

    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;
    int numPanels = 1;
    _power = 20;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p2", &rotation);
    getLevel()->convertPositionAndRotationData(&x, &y, &rotation);
    element->intAttribute("p3", &numPanels);
    element->intAttribute("p4", &_power);

    // The original clears only the cosine here (it is overwritten below).
    _cosVal = 0.0f;
    float segLength = 180.0f / LevelItem::s_flashPtmRatio * getPtm();
    float angle = rotation * -0.0174532924f;
    createBodies(b2Vec2(x, y), angle, numPanels, segLength);  // inlined in the original

    _sinVal = sinf(angle - M_PI_2);
    _cosVal = cosf(angle - M_PI_2);

    setUpSprites(Vec2(x * getPtm(), y * getPtm()), rotation, numPanels, segLength);
    addToBeginContact(_sensor);
    addToEndContact(_sensor);
    getLevel()->addToFrameActions(this);
    if (online::browserPhysicsWanted()) getLevel()->addToActions(this);  // ONLINE (PC addition)
    return true;
}

// @00587590
void BoostPanel::createBodies(b2Vec2 position, float angle, unsigned int numPanels, float segLength)
{
    b2PolygonShape shape;
    shape.SetAsBox(segLength * 0.5f * numPanels / getPtm(), segLength * 0.5f / getPtm(), position,
                   angle);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.isSensor = true;
    fixtureDef.filter.categoryBits = 8;
    fixtureDef.filter.groupIndex = -20;
    _sensor = getLevelBody()->CreateFixture(&fixtureDef);
}

// @005876a0
void BoostPanel::setUpSprites(Vec2 position, float angleDegrees, unsigned int numPanels,
                              float segLength)
{
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    for (unsigned int i = 1; i < 5; i++)
    {
        _frames.push_back(
            cache->getSpriteFrameByName("boostpanel_" + patch::to_string(i) + ".png"));
    }

    _mc = Node::create();
    _mc->setPosition(position);
    _mc->setRotation(angleDegrees);
    getLevelItemsNode()->addChild(_mc);

    float halfSeg = segLength * 0.5f;
    float x = halfSeg - halfSeg * numPanels;
    unsigned int frame = 1;
    for (unsigned int i = 0; i != numPanels; i++)
    {
        Sprite* panel = Sprite::createWithSpriteFrameName("boostpanel_4.png");
        panel->setTag(frame);
        setFrameForSprite(panel, frame);
        panel->setScaleX(1.003f);
        panel->setPosition(Vec2(x, 0.0f));
        _mc->addChild(panel);
        _panels.push_back(panel);

        Sprite* liner = Sprite::createWithSpriteFrameName("boostpanel_liner.png");
        liner->setPosition(Vec2(x, halfSeg));
        _mc->addChild(liner, numPanels + 3);

        liner = Sprite::createWithSpriteFrameName("boostpanel_liner.png");
        liner->setPosition(Vec2(x, -halfSeg));
        _mc->addChild(liner, numPanels + 3);

        x += segLength;
        // frames run 1, 8, 6, 4, 2, 9, 7, 5, 3, 1, ... along the strip
        frame += ((int)frame > 2) ? -2 : 7;
    }
}

// @00587dc0
void BoostPanel::frameAction()
{
    // ONLINE (PC addition): every 4 frames at 1/60, every 2 browser physics steps (1/30).
    if (online::stepsPerFlashFrame() == 1 && _frameIndex == 1) _frameIndex = 3;
    if (_frameIndex++ == 3)
    {
        _frameIndex = 0;
        for (Sprite* panel : _panels)
        {
            int tag = panel->getTag();
            panel->setTag(tag == 9 ? 1 : tag + 1);
            setFrameForSprite(panel, panel->getTag());
        }
    }

    if (_bodies.empty())
    {
        if (_sound)
        {
            _sound->fadeTo(0.0f, 0.2f, true);
        }
        return;
    }

    for (b2Body* body : _bodies)
    {
        if (body->GetType() == b2_dynamicBody && !online::browserPhysics())  // ONLINE (PC addition): see actions()
        {
            float force = body->GetMass() * _power;
            body->ApplyForceToCenter(b2Vec2(-_sinVal * force, force * _cosVal), true);
        }
    }

    if (!_sound)
    {
        b2Body* body = _bodies[0];
        _sound = createBodySound("BoostLoop3", body, 1.0f, true);
        if (_sound)
        {
            // @00588494 ($_0)
            _sound->setFinishCallback([this](int&) { soundStopped(); });
            _sound->setMaxVolume(0.0f);
            _sound->fadeTo(1.0f, 0.2f, false);
            getSession()->getDestructionListener()->addBodyListener(body, this);
        }
    }
}

// @005880c8
void BoostPanel::setFrameForSprite(Sprite* sprite, unsigned int frame)
{
    if (frame < 5)
    {
        sprite->setSpriteFrame(_frames[frame - 1]);
    }
}

// @005880f4
void BoostPanel::soundStopped()
{
    _sound = nullptr;
}

// @005880fc
void BoostPanel::bodyWillBeDestroyed(b2Body* body)
{
}

// @00588100
void BoostPanel::prepareForTrigger()
{
    getLevel()->removeFromFrameActions(this);
    getLevel()->removeFromActions(this);  // ONLINE (PC addition)
    for (unsigned long i = 0; i < _panels.size(); i++)
    {
        Sprite* panel = _panels[i];
        panel->setSpriteFrame(_frames[3]);
        panel->setOpacity(0x99);
    }
}

// @00588184
void BoostPanel::triggerSingleActivation(LevelItem* trigger, int action,
                                         std::vector<float> properties)
{
    if (_triggered)
    {
        return;
    }
    _triggered = true;
    getLevel()->addToFrameActions(this);
    if (online::browserPhysicsWanted()) getLevel()->addToActions(this);  // ONLINE (PC addition)
    for (unsigned long i = 0; i < _panels.size(); i++)
    {
        Sprite* panel = _panels[i];
        setFrameForSprite(panel, panel->getTag());
        panel->setOpacity(255);
    }
}

// @0058823c
void BoostPanel::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    b2Body* body = otherFixture->GetBody();
    if (std::find(_bodies.begin(), _bodies.end(), body) == _bodies.end())
    {
        _bodies.push_back(body);
    }
}

// @005883d8
void BoostPanel::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    std::vector<b2Body*>::iterator it =
        std::find(_bodies.begin(), _bodies.end(), otherFixture->GetBody());
    if (it != _bodies.end())
    {
        _bodies.erase(it);
    }
}

// ONLINE (PC addition): see BoostPanel.h.
void BoostPanel::actions()
{
    if (!online::browserPhysics()) return;
    for (b2Body* body : _bodies)
    {
        if (body->GetType() == b2_dynamicBody && online::flashPersists(body, _sensor))
        {
            float force = body->GetMass() * _power;
            body->ApplyForceToCenter(b2Vec2(-_sinVal * force, force * _cosVal), true);
        }
    }
}
