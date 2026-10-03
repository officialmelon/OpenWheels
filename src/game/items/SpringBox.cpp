#include "SpringBox.h"

#include <cmath>
#include <iomanip>
#include <sstream>

#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Session.h"

USING_NS_CC;

// @006332cc (D0; the complete destructor is LevelItem's)
SpringBox::~SpringBox()
{
}

// @00632014
bool SpringBox::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);

    float y = 0.0f;
    float x = 0.0f;
    float rotation = 0.0f;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p2", &rotation);
    getLevel()->convertPositionAndRotationData(&x, &y, &rotation);
    _hit = false;
    _delayCounter = 0;
    _delayTotal = 0;
    float ptm = getPtm();
    float delay = 0.0f;
    element->floatAttribute("p3", &delay);
    _translation = 0.512f;

    _pad = Sprite::createWithSpriteFrameName("springbox_top.png");
    Sprite* piston = Sprite::createWithSpriteFrameName("springbox_piston.png");
    piston->setPosition(Vec2(ptm * 0.88f, 1.0f));
    piston->setAnchorPoint(Vec2(0.5f, 1.0f));
    _pad->addChild(piston);
    piston = Sprite::createWithSpriteFrameName("springbox_piston.png");
    piston->setPosition(Vec2(ptm * 4.72f, 1.0f));
    piston->setAnchorPoint(Vec2(0.5f, 1.0f));
    _pad->addChild(piston);

    Node* itemsNode = getLevelItemsNode();
    _pad->setPosition(Vec2(ptm * x, ptm * y));
    _pad->setRotation(rotation);
    _pad->setAnchorPoint(Vec2(0.5f, 0.086f));
    itemsNode->addChild(_pad);

    _mc = Sprite::createWithSpriteFrameName("springbox_base.png");
    _mc->setPosition(Vec2(ptm * x, ptm * y));
    _mc->setAnchorPoint(Vec2(0.5f, ptm * 0.064f / _mc->getTextureRect().size.height + 0.5f));
    _mc->setRotation(rotation);
    itemsNode->addChild(_mc);

    _delayTimeCounter = delay;
    _delayTimeTotal = delay;
    std::stringstream timeText;
    timeText << std::fixed << std::setprecision(2) << _delayTimeCounter;
    Vec2 labelPosition = _mc->convertToWorldSpace(Vec2(0.576f * ptm, 0.176f * ptm));
    labelPosition = getSession()->getLabelAtlasNode()->convertToNodeSpace(labelPosition);
    _timerText = LabelAtlas::create(timeText.str(), "fonts/springbox_font.png", 36, 30, '.');
    getSession()->getLabelAtlasNode()->addChild(_timerText);
    _timerText->setPosition(labelPosition);
    _timerText->setRotation(rotation);

    rotation = rotation * -0.0174532924f;
    createBody(b2Vec2(x, y), rotation);
    createJoint(rotation);  // inlined in the original

    _glow = Sprite::createWithSpriteFrameName("springbox_arrow.png");
    _glow->setPosition(Vec2(0.24f * ptm, 0.284f * ptm));
    _mc->addChild(_glow);
    Sprite* arrow = Sprite::createWithSpriteFrameName("springbox_arrow.png");
    arrow->setPosition(Vec2(ptm * 5.3608f - _glow->getPosition().x +
                                arrow->getTextureRect().size.width * 0.5f,
                            arrow->getTextureRect().size.height * 0.5f));
    _glow->addChild(arrow);
    _glow->setVisible(false);

    getLevel()->addToActions(this);
    addToPostSolve(_padShape);
    return true;
}

// @00632a04
void SpringBox::createBody(b2Vec2 position, float angle)
{
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = position;
    bodyDef.angle = angle;
    bodyDef.userData = _pad;
    _body = getWorld()->CreateBody(&bodyDef);

    b2PolygonShape shape;
    b2FixtureDef fixtureDef;
    fixtureDef.friction = 0.5f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 200.0f;
    fixtureDef.filter.categoryBits = 8;

    shape.SetAsBox(2.8f, 0.176f, b2Vec2(0.0f, 0.144f), 0.0f);
    fixtureDef.shape = &shape;
    _padShape = _body->CreateFixture(&fixtureDef);
    getLevel()->addToPaintBody(_body);

    shape.SetAsBox(0.208f, 0.144f, b2Vec2(1.92f, -0.176f), 0.0f);
    fixtureDef.shape = &shape;
    _body->CreateFixture(&fixtureDef);

    shape.SetAsBox(0.208f, 0.144f, b2Vec2(-1.92f, -0.176f), 0.0f);
    fixtureDef.shape = &shape;
    _body->CreateFixture(&fixtureDef);
    _body->ResetMassData();

    // base box on the level body, below the pad
    shape.SetAsBox(2.8f, 0.256f, _body->GetWorldPoint(b2Vec2(0.0f, -0.064f)), angle);
    fixtureDef.shape = &shape;
    getLevelBody()->CreateFixture(&fixtureDef);
}

// @00632c40
void SpringBox::createJoint(float angle)
{
    b2PrismaticJointDef jointDef;
    jointDef.Initialize(getLevelBody(), _body, _body->GetWorldCenter(),
                        b2Vec2(-sinf(angle), cosf(angle)));
    jointDef.lowerTranslation = 0.0f;
    jointDef.upperTranslation = 0.0f;
    jointDef.enableLimit = true;
    jointDef.enableMotor = false;
    jointDef.maxMotorForce = 1000000.0f;
    _joint = (b2PrismaticJoint*)getWorld()->CreateJoint(&jointDef);
}

// @00632d4c
void SpringBox::actions()
{
    if (!_hit)
    {
        if (_joint->IsMotorEnabled())
        {
            if (_joint->GetMotorSpeed() > 0.0f)
            {
                if (_joint->GetJointTranslation() > _translation)
                {
                    _joint->SetMotorSpeed(-1.0f);
                }
            }
            else if (_joint->GetMotorSpeed() < 0.0f && _joint->GetJointTranslation() < 0.0f)
            {
                // back home: re-arm
                _joint->EnableMotor(false);
                _joint->SetLimits(0.0f, 0.0f);
                _joint->SetMotorSpeed(0.0f);
                addToPostSolve(_padShape);
                std::stringstream timeText;
                timeText << std::fixed << std::setprecision(2) << _delayTimeTotal;
                _timerText->setString(timeText.str());
                _glow->setVisible(false);
            }
        }
    }
    else if (ceilf(_delayTimeCounter) == 0.0f)
    {
        // fire
        _glow->setVisible(true);
        _hit = false;
        _body->SetAwake(true);
        _joint->SetMotorSpeed(8.0f);
        _joint->SetLimits(0.0f, _translation);
        _joint->EnableMotor(true);
        _delayTimeCounter = _delayTimeTotal;
        _timerText->setString("0.00");
        createBodySound("SpringBoxBounce", _body, 1.0f, false);
    }
    else
    {
        std::stringstream timeText;
        timeText << std::fixed << std::setprecision(2) << _delayTimeCounter;
        _timerText->setString(timeText.str());
        _delayTimeCounter -= LevelItem::s_timeStep;
    }
}

// @006332a0
void SpringBox::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                          const b2ContactImpulse* impulse)
{
    removePostSolve(_padShape);
    _hit = true;
}
