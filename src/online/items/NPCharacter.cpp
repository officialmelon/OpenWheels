// ONLINE (PC addition): see NPCharacter.h. Port of com.totaljerkface.game.level.userspecials.
// NPCharacter (v1.87) with the skin layouts of editor/specials/npcsprites (npc/NPCSpriteData).
//
// Geometry is computed in Flash space exactly as NPCharacter.createBodies/createJoints do (the
// posed NPCSprite's display matrices, sprite scale +-0.5 inside the ref's x/y/rotation) and then
// mapped to the mobile world (y up): positions through flashToWorld, angles negated, revolute
// limits mirrored ([L, U] on the Flash relative angle -> [-U, -L]).
//
// Mobile adaptations (same as the mobile CharacterB2D's, so NPCs are as tough as the converted
// level's player, like in Flash where both share the formulas): smash limits x1.15 against
// Box2D 2.3 postSolve impulses, joint break limits x2.5 on |reaction force| at 60 Hz, joint
// pull-apart check 0.5 m^2. With gore disabled (Options) nothing breaks or bleeds.
#include "online/items/NPCharacter.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "cocos2d.h"

#include "BurstEmitter.h"
#include "ContactListener.h"
#include "DestructionListener.h"
#include "EmitterNode.h"
#include "FlowEmitter.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Session.h"
#include "Sound.h"
#include "online/FlashRuntime.h"
#include "online/items/npc/NPCSpriteData.h"

USING_NS_CC;

namespace online {

namespace {

FlashSpecialRegistration s_reg(17, [] { return (LevelItem*)new (std::nothrow) NPCharacter(); },
                               FlashSpecialUse::Missing, /*groupable*/ true);

const float kDeg = 0.017453292519943295f;
const float kCharacterScale = 125.0f;  // m_physScale * mc_scale
// CharacterB2D.DEF_* (Flash) masses / smash impulses.
const float kDefHeadMass = 0.05456f, kDefChestMass = 0.17171f, kDefPelvisMass = 0.0735f;
const float kDefUpperArmMass = 0.07019f, kDefLowerArmMass = 0.07784f;
const float kDefUpperLegMass = 0.14914f, kDefLowerLegMass = 0.10218f;
const float kDefHeadSmash = 3.0f, kDefChestSmash = 7.5f, kDefPelvisSmash = 5.5f;

// Flash NPCharacter.GROUP_ID_COUNT: every NPC takes two collision groups, counting down from -40.
int s_groupIdCount = -40;
// Draw order across NPCs (later ones on top, as Flash addChild).
int s_serial = 0;

// Flash display matrix (y down).
struct FM {
    double a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
    FM() = default;
    FM(const npc::Mat& m) : a(m.a), b(m.b), c(m.c), d(m.d), tx(m.tx), ty(m.ty) {}
    FM(double a_, double b_, double c_, double d_, double tx_, double ty_)
        : a(a_), b(b_), c(c_), d(d_), tx(tx_), ty(ty_) {}
    // this applied after o (parent * child)
    FM operator*(const FM& o) const
    {
        return FM(a * o.a + c * o.b, b * o.a + d * o.b, a * o.c + c * o.d, b * o.c + d * o.d,
                  a * o.tx + c * o.ty + tx, b * o.tx + d * o.ty + ty);
    }
    Vec2 apply(double x, double y) const { return Vec2((float)(a * x + c * y + tx), (float)(b * x + d * y + ty)); }
    double rotation() const { return std::atan2(b, a) / kDeg; }  // DisplayObject.rotation
    // DisplayObject.rotation setter (keeps scaleX / scaleY).
    FM rotated(double deg) const
    {
        const double sx = std::hypot(a, b), sy = std::hypot(c, d), r = deg * kDeg;
        return FM(sx * std::cos(r), sx * std::sin(r), -sy * std::sin(r), sy * std::cos(r), tx, ty);
    }
};

b2Vec2 toWorld(const Vec2& flashPx) { return flashToWorld(flashPx.x, flashPx.y); }

void cascadeOpacity(Node* node)
{
    node->setCascadeOpacityEnabled(true);
    for (Node* child : node->getChildren()) cascadeOpacity(child);
}

const char* const kPartNames[] = {"chest", "head", "pelvis", "upperArm1", "upperArm2", "lowerArm1",
                                  "lowerArm2", "upperLeg1", "upperLeg2", "lowerLeg1", "lowerLeg2"};

}  // namespace

bool isNPCharacterCentralBody(LevelItem* item, b2Body* body)
{
    NPCharacter* npc = dynamic_cast<NPCharacter*>(item);
    return npc && body && npc->centralBody() == body;
}

NPCharacter::~NPCharacter()
{
    for (FlowEmitter*& flow : _flow) {
        if (flow) flow->setEmitterDelegate(nullptr);
        flow = nullptr;
    }
    if (_voiceSound) _voiceSound->setFinishCallback(nullptr);
    for (Node* node : _partNode) {
        if (node) node->release();
    }
    for (Node* node : _extraNodes) node->release();
    if (_staticRoot) _staticRoot->release();
}

// ---------------------------------------------------------------------------------------------
// Set-up
// ---------------------------------------------------------------------------------------------

void NPCharacter::readPose(LevelDataElement* element)
{
    _x = num(element, "p0", 0.0f);
    _y = num(element, "p1", 0.0f);
    _angle = num(element, "p2", 0.0f);
    _charIndex = std::max(1, std::min(npc::kSpriteCount, inum(element, "p3", 1)));
    _sleeping = flag(element, "p4", false);
    _reversed = flag(element, "p5", false);
    _holdPose = flag(element, "p6", false);
    _interactive = flag(element, "p7", true);
    // NPCharacterRef setters: int properties clamped to the editor ranges.
    static const int lo[9] = {-20, -180, -180, -160, -160, -150, -150, 0, 0};
    static const int hi[9] = {20, 60, 60, 0, 0, 10, 10, 150, 150};
    for (int i = 0; i < 9; i++) {
        char key[8];
        snprintf(key, sizeof key, "p%d", 8 + i);
        _pose[i] = std::max(lo[i], std::min(hi[i], (int)num(element, key, 0.0f)));
    }
    _destroyJointsUponDeath = flag(element, "p17", false);
    static const char* const tags[17] = {"", "Char1", "Char2", "Char3", "Kid1", "Char4", "Char8",
                                         "Char9", "Char11", "Char2", "Santa", "Elf1", "Char12",
                                         "Char4", "Kid2", "Kid1", "Heli"};
    _tag = tags[_charIndex];
    _data = &npc::spriteData(_charIndex);
}

bool NPCharacter::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    (void)groupOffset;
    readPose(element);
    _showGore = !UserDefault::getInstance()->getBoolForKey("gore_disabled");
    _inGroup = groupBody != nullptr || element->stringAttribute("fg") != nullptr;
    s_serial++;
    if (!_interactive || _inGroup) {
        createStaticArt(_inGroup, element->stringAttribute("fg") != nullptr);
        return true;
    }
    // createFilters
    _defaultFilter.categoryBits = 260;
    _defaultFilter.maskBits = 0xffff;
    _defaultFilter.groupIndex = (int16)s_groupIdCount;
    _zeroFilter = _defaultFilter;
    _zeroFilter.groupIndex = 0;
    _lowerBodyFilter = _defaultFilter;
    _lowerBodyFilter.groupIndex = (int16)(s_groupIdCount - 1);
    s_groupIdCount -= 2;
    if (s_groupIdCount < -30000) s_groupIdCount = -40;

    createBodies();
    for (b2Body* b : _body) registerGrindBody(b);  // Lawnmower Man blade
    createJoints();
    createArt();
    setBreakLimits();
    // createDictionaries: hit sounds; contact listeners
    _contactAddSounds[_fixture[kHead]] = "Thud1";
    _contactAddSounds[_fixture[kChest]] = "Thud2";
    _contactAddSounds[_fixture[kPelvis]] = "Thud2";
    for (Part p : {kChest, kHead, kPelvis}) {
        addToPostSolve(_fixture[p]);
        addToBeginContact(_fixture[p]);
    }
    getLevel()->addToActions(this);
    return true;
}

void NPCharacter::createBodies()
{
    const npc::SpriteData& s = *_data;
    const double sgn = _reversed ? -1.0 : 1.0;
    const double t = _angle * kDeg;
    const FM R(std::cos(t), std::sin(t), -std::sin(t), std::cos(t), _x, _y);
    const FM RS = R * FM(sgn * 0.5, 0, 0, 0.5, 0, 0);

    b2World* world = getWorld();
    LevelB2D* level = getLevel();
    auto make = [&](Part p, const Vec2& posPx, double flashDeg, const float* shape, bool circle, int material) {
        b2BodyDef bd;
        bd.type = b2_dynamicBody;
        bd.awake = !_sleeping;
        bd.position = toWorld(posPx);
        bd.angle = flashAngle((float)flashDeg);
        b2Body* body = world->CreateBody(&bd);
        b2PolygonShape box;
        b2CircleShape circ;
        b2FixtureDef fd;
        fd.density = 1.0f;
        fd.friction = 0.3f;
        fd.restitution = 0.1f;
        fd.filter = _defaultFilter;
        fd.userData = static_cast<LevelItem*>(this);
        // Box2D 2.3 wakes both bodies when a contact pair is created (b2ContactManager::AddPair),
        // Flash's 2.0 does not: a sleeping NPC that starts overlapping the ground or another NPC
        // would wake up at once. Sleeping NPCs start as sensors (pairs between sensors don't wake)
        // and become solid, still asleep, on the first step (wakeSensors).
        fd.isSensor = _sleeping;
        if (circle) {
            circ.m_radius = shape[0] * 50.0f / kCharacterScale;
            fd.shape = &circ;
        } else {
            box.SetAsBox(shape[0] * 50.0f / kCharacterScale, shape[1] * 50.0f / kCharacterScale);
            fd.shape = &box;
        }
        _fixture[p] = body->CreateFixture(&fd);
        level->addFixtureMaterial(_fixture[p], material);
        _body[p] = body;
    };

    // chest at the ref's origin
    make(kChest, Vec2(_x, _y), _angle, s.chestShape, false, 2);
    const FM ho = FM(s.headOuter).rotated(_pose[0]);
    make(kHead, (RS * ho).apply(s.head.tx, s.head.ty), _angle + sgn * _pose[0], s.headShape, true, 2);
    make(kPelvis, RS.apply(s.pelvis.tx, s.pelvis.ty), _angle + sgn * FM(s.pelvis).rotation(), s.pelvisShape,
         false, 2);
    FM arm[2], lao[2], leg[2], llo[2];
    for (int k = 0; k < 2; k++) {
        arm[k] = FM(s.arm[k]).rotated(_pose[1 + k]);
        lao[k] = FM(s.lowerArmOuter[k]).rotated(_pose[3 + k]);
        leg[k] = FM(s.leg[k]).rotated(_pose[5 + k]);
        llo[k] = FM(s.lowerLegOuter[k]).rotated(_pose[7 + k]);
    }
    for (int k = 0; k < 2; k++)
        make((Part)(kUpperArm1 + k), (RS * arm[k]).apply(s.upperArm[k].tx, s.upperArm[k].ty),
             _angle + sgn * _pose[1 + k], s.upperArmShape[k], false, 1);
    for (int k = 0; k < 2; k++)
        make((Part)(kLowerArm1 + k), (RS * arm[k] * lao[k]).apply(s.lowerArm[k].tx, s.lowerArm[k].ty),
             _angle + sgn * (_pose[1 + k] + _pose[3 + k]), s.lowerArmShape[k], false, 1);
    for (int k = 0; k < 2; k++)
        make((Part)(kUpperLeg1 + k), (RS * leg[k]).apply(s.upperLeg[k].tx, s.upperLeg[k].ty),
             _angle + sgn * _pose[5 + k], s.upperLegShape[k], false, 1);
    for (int k = 0; k < 2; k++)
        make((Part)(kLowerLeg1 + k), (RS * leg[k] * llo[k]).apply(s.lowerLeg[k].tx, s.lowerLeg[k].ty),
             _angle + sgn * (_pose[5 + k] + _pose[7 + k]), s.lowerLegShape[k], false, 1);
    for (b2Body* b : _body) b->ResetMassData();

    // Flash rounds the chunk radii (px) from the head shape for all three.
    _headChunkRadius = std::round(s.headShape[0] * 50.0f * 0.5f);
    _pelvisChunkRadius = std::round(s.headShape[0] * 50.0f * 0.75f);
    _chestChunkRadius = std::round(s.headShape[0] * 50.0f * 0.95f);
}

void NPCharacter::setAbsLimits(JointId id, float lowerDeg, float upperDeg)
{
    b2RevoluteJoint* j = _joint[id];
    if (!j) return;
    const float ref = j->GetReferenceAngle();
    j->SetLimits(-upperDeg * kDeg - ref, -lowerDeg * kDeg - ref);
}

void NPCharacter::createJoints()
{
    const npc::SpriteData& s = *_data;
    const double sgn = _reversed ? -1.0 : 1.0;
    const double t = _angle * kDeg;
    const FM R(std::cos(t), std::sin(t), -std::sin(t), std::cos(t), _x, _y);
    const FM RS = R * FM(sgn * 0.5, 0, 0, 0.5, 0, 0);
    const bool motors = _holdPose && flashVersion() < 1.75f;
    b2World* world = getWorld();

    auto make = [&](JointId id, Part a, Part b, const Vec2& anchorPx, float lo, float hi) {
        b2RevoluteJointDef jd;
        jd.Initialize(_body[a], _body[b], toWorld(anchorPx));
        jd.enableLimit = true;
        jd.maxMotorTorque = 4.0f;
        jd.enableMotor = motors;
        jd.motorSpeed = 0.0f;
        // Flash: lowerAngle = L - rel; mirrored for the y-up world.
        jd.lowerAngle = -hi * kDeg - jd.referenceAngle;
        jd.upperAngle = -lo * kDeg - jd.referenceAngle;
        _joint[id] = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jd));
    };
    const bool r = _reversed;
    make(kNeck, kChest, kHead, RS.apply(s.headOuter.tx, s.headOuter.ty), -20, 20);
    make(kWaist, kChest, kPelvis, RS.apply(s.pelvis.tx, s.pelvis.ty - s.pelvisHeight * 0.5), -5, 5);
    for (int k = 0; k < 2; k++)
        make((JointId)(kShoulder1 + k), kChest, (Part)(kUpperArm1 + k), RS.apply(s.arm[k].tx, s.arm[k].ty),
             r ? -60 : -180, r ? 180 : 60);
    for (int k = 0; k < 2; k++) {
        const FM arm = FM(s.arm[k]).rotated(_pose[1 + k]);
        make((JointId)(kElbow1 + k), (Part)(kUpperArm1 + k), (Part)(kLowerArm1 + k),
             (RS * arm).apply(s.lowerArmOuter[k].tx, s.lowerArmOuter[k].ty), r ? 0 : -160, r ? 160 : 0);
    }
    for (int k = 0; k < 2; k++)
        make((JointId)(kHip1 + k), kPelvis, (Part)(kUpperLeg1 + k), RS.apply(s.leg[k].tx, s.leg[k].ty),
             r ? -10 : -150, r ? 150 : 10);
    for (int k = 0; k < 2; k++) {
        const FM leg = FM(s.leg[k]).rotated(_pose[5 + k]);
        make((JointId)(kKnee1 + k), (Part)(kUpperLeg1 + k), (Part)(kLowerLeg1 + k),
             (RS * leg).apply(s.lowerLegOuter[k].tx, s.lowerLegOuter[k].ty), r ? -150 : 0, r ? 0 : 150);
    }
    _elbow1LocalAnchorB = _joint[kElbow1]->GetLocalAnchorB();
    if (_holdPose && flashVersion() >= 1.75f) lockPose();
}

void NPCharacter::setBreakLimits()
{
    const float head = _body[kHead]->GetMass() / kDefHeadMass;
    const float chest = _body[kChest]->GetMass() / kDefChestMass;
    const float pelvis = _body[kPelvis]->GetMass() / kDefPelvisMass;
    const float upperArm = _body[kUpperArm1]->GetMass() / kDefUpperArmMass;
    const float lowerArm = _body[kLowerArm1]->GetMass() / kDefLowerArmMass;
    const float upperLeg = _body[kUpperLeg1]->GetMass() / kDefUpperLegMass;
    const float lowerLeg = _body[kLowerLeg1]->GetMass() / kDefLowerLegMass;
    // Flash: impulse > DEF_*_SMASH * ratio; |F|^2 > round(N * ratio)^2. Mobile scale (see top).
    _headSmashLimit = kDefHeadSmash * head * 1.15f;
    _chestSmashLimit = kDefChestSmash * chest * 1.15f;
    _pelvisSmashLimit = kDefPelvisSmash * pelvis * 1.15f;
    _jointLimit[kNeck] = std::round(85.0f * head) * 2.5f;
    _jointLimit[kWaist] = std::round(180.0f * pelvis) * 2.5f;
    _jointLimit[kShoulder1] = _jointLimit[kShoulder2] = std::round(75.0f * upperArm) * 2.5f;
    _jointLimit[kHip1] = _jointLimit[kHip2] = std::round(95.0f * upperLeg) * 2.5f;
    _jointLimit[kElbow1] = _jointLimit[kElbow2] = std::round(70.0f * lowerArm) * 2.5f;
    _jointLimit[kKnee1] = _jointLimit[kKnee2] = std::round(80.0f * lowerLeg) * 2.5f;
}

// ---------------------------------------------------------------------------------------------
// Art
// ---------------------------------------------------------------------------------------------

std::string NPCharacter::partName(Part part) const
{
    return "npc" + std::to_string(_charIndex) + "_" + kPartNames[part];
}

// A part's art in its clip's own units (Flash px of the symbol), or a plain shape when the SWF
// art was not extracted.
Node* NPCharacter::makePartNode(Part part, int frame)
{
    Sprite* sprite = createFlashSprite(partName(part) + "_" + std::to_string(frame));
    if (sprite) return sprite;
    const npc::SpriteData& s = *_data;
    const float* shape = part == kChest ? s.chestShape : part == kHead ? s.headShape
                        : part == kPelvis ? s.pelvisShape
                        : part <= kUpperArm2 ? s.upperArmShape[part - kUpperArm1]
                        : part <= kLowerArm2 ? s.lowerArmShape[part - kLowerArm1]
                        : part <= kUpperLeg2 ? s.upperLegShape[part - kUpperLeg1]
                                             : s.lowerLegShape[part - kLowerLeg1];
    const float ppf = pointsPerFlashPx();
    const float hw = shape[0] * 50.0f * ppf, hh = shape[1] * 50.0f * ppf;
    DrawNode* draw = DrawNode::create();
    const Color4F skin(0.93f, 0.78f, 0.62f, 1.0f), cloth(0.35f, 0.45f, 0.65f, 1.0f);
    const Color4F line(0.2f, 0.15f, 0.1f, 1.0f);
    if (part == kHead) {
        draw->drawSolidCircle(Vec2::ZERO, hw, 0.0f, 24, skin);
    } else {
        Vec2 v[4] = {Vec2(-hw, -hh), Vec2(hw, -hh), Vec2(hw, hh), Vec2(-hw, hh)};
        draw->drawPolygon(v, 4, part == kChest || part == kPelvis ? cloth : skin, 1.0f, line);
    }
    return draw;
}

void NPCharacter::setPartFrame(Part part, int frame)
{
    Node* node = _partNode[part];
    if (!node || frame == _partFrame[part]) return;
    _partFrame[part] = frame;
    Node* inner = node->getChildByTag(1);
    if (!inner) return;
    inner->removeAllChildren();
    inner->addChild(makePartNode(part, frame));
}

Node* NPCharacter::chunkNode(const std::string& name, float fallbackRadiusPx)
{
    Node* node = Node::create();
    Node* inner = Node::create();
    inner->setScale(0.5f);  // Flash chunk.scaleX = scaleY = 0.5 (never mirrored)
    node->addChild(inner);
    if (Sprite* sprite = createFlashSprite(name)) {
        inner->addChild(sprite);
    } else {
        DrawNode* draw = DrawNode::create();
        draw->drawSolidCircle(Vec2::ZERO, fallbackRadiusPx * 2.0f * pointsPerFlashPx(), 0.0f, 12,
                              Color4F(0.7f, 0.05f, 0.05f, 1.0f));
        inner->addChild(draw);
    }
    return node;
}

void NPCharacter::createArt()
{
    // createMovieClips: the parts go to the character layer in the skin's child order (arm k:
    // upper then lower arm, the same for legs), mirrored with scaleX -0.5 when reversed.
    _layer = getSession()->getCharacterBackground();
    const int base = -30000 + (s_serial % 700) * 40;
    int z = base;
    auto add = [&](Part p) {
        Node* node = Node::create();
        Node* inner = Node::create();
        inner->setTag(1);
        inner->setScale(_reversed ? -0.5f : 0.5f, 0.5f);
        node->addChild(inner);
        node->retain();
        _partNode[p] = node;
        _partFrame[p] = 1;
        inner->addChild(makePartNode(p, 1));
        _partZ[p] = z;
        if (_layer) _layer->addChild(node, z);
        z++;
        _body[p]->SetUserData(node);
        getLevel()->addToPaintBody(_body[p]);
    };
    for (int top : _data->order) {
        switch (top) {
        case 0: add(kHead); break;
        case 1: add(kChest); break;
        case 2: add(kPelvis); break;
        case 3: add(kUpperArm1); add(kLowerArm1); break;
        case 4: add(kUpperArm2); add(kLowerArm2); break;
        case 5: add(kUpperLeg1); add(kLowerLeg1); break;
        case 6: add(kUpperLeg2); add(kLowerLeg2); break;
        }
    }
    for (int p = 0; p < kPartCount; p++) {
        const b2Vec2 pos = _body[p]->GetPosition();
        _partNode[p]->setPosition(Vec2(pos.x * getPtm(), pos.y * getPtm()));
        _partNode[p]->setRotation(_body[p]->GetAngle() * -57.29578f);
    }
}

// Non-interactive NPCs: the posed NPCSprite itself (art only), in the character layer, or in its
// group's layer (positioned by paintWithOffsetPoints).
void NPCharacter::createStaticArt(bool inGroup, bool foreground)
{
    const npc::SpriteData& s = *_data;
    const float ppf = pointsPerFlashPx();
    _staticRoot = Node::create();
    _staticRoot->retain();
    Node* spr = Node::create();
    spr->setScale(_reversed ? -0.5f : 0.5f, 0.5f);
    _staticRoot->addChild(spr);
    auto leaf = [&](Part p, const FM& m) {
        Node* node = Node::create();
        node->setPosition(Vec2((float)m.tx * ppf, (float)-m.ty * ppf));
        node->setRotation((float)m.rotation());
        const float sx = (float)std::hypot(m.a, m.b);
        node->setScale(sx, sx > 0 ? (float)((m.a * m.d - m.b * m.c) / sx) : 1.0f);
        node->addChild(makePartNode(p, 1));
        spr->addChild(node);
    };
    FM arm[2], lao[2], leg[2], llo[2];
    for (int k = 0; k < 2; k++) {
        arm[k] = FM(s.arm[k]).rotated(_pose[1 + k]);
        lao[k] = FM(s.lowerArmOuter[k]).rotated(_pose[3 + k]);
        leg[k] = FM(s.leg[k]).rotated(_pose[5 + k]);
        llo[k] = FM(s.lowerLegOuter[k]).rotated(_pose[7 + k]);
    }
    for (int top : s.order) {
        switch (top) {
        case 0: leaf(kHead, FM(s.headOuter).rotated(_pose[0]) * FM(s.head)); break;
        case 1: leaf(kChest, FM(s.chest)); break;
        case 2: leaf(kPelvis, FM(s.pelvis)); break;
        case 3: case 4: {
            const int k = top - 3;
            leaf((Part)(kUpperArm1 + k), arm[k] * FM(s.upperArm[k]));
            leaf((Part)(kLowerArm1 + k), arm[k] * lao[k] * FM(s.lowerArm[k]));
            break;
        }
        default: {
            const int k = top - 5;
            leaf((Part)(kUpperLeg1 + k), leg[k] * FM(s.upperLeg[k]));
            leaf((Part)(kLowerLeg1 + k), leg[k] * llo[k] * FM(s.lowerLeg[k]));
            break;
        }
        }
    }
    cascadeOpacity(_staticRoot);
    if (inGroup) {
        Node* layer = foreground ? flashForegroundLayer() : flashBackgroundLayer();
        if (layer) layer->addChild(_staticRoot);
    } else {
        _layer = getSession()->getCharacterBackground();
        if (_layer) _layer->addChild(_staticRoot, -30000 + (s_serial % 700) * 40);
        const b2Vec2 m = flashToWorld(_x, _y);
        _staticRoot->setPosition(Vec2(m.x * getPtm(), m.y * getPtm()));
        _staticRoot->setRotation(_angle);
    }
}

void NPCharacter::paintWithOffsetPoints(Vec2 offset, float rotation)
{
    if (!_staticRoot) return;
    _staticRoot->setPosition(offset);
    _staticRoot->setRotation(rotation);
}

void NPCharacter::setOpacity(float opacity)
{
    if (_staticRoot) _staticRoot->setOpacity((GLubyte)std::lround(std::max(0.0f, std::min(1.0f, opacity)) * 255.0f));
}

// ---------------------------------------------------------------------------------------------
// Per step
// ---------------------------------------------------------------------------------------------

void NPCharacter::actions()
{
    if (!_userJointsScanned) {
        scanUserJoints();
        if (_sleeping) {
            for (b2Fixture* f : _fixture)
                if (f) f->SetSensor(false);
            for (b2Body* b : _body)
                if (b) b->SetAwake(false);
        }
    }
    handleContactResults();
    handleContactAdds();
    if (_showGore) checkJoints();
    if (!_dead) checkVocals();
    for (std::string& v : _voiceArray) v.clear();
    _voiceTop = -1;
}

// Flash NPCharacter.addUserJoint (called for every level joint to the NPC): here the joints the
// level created at load are found on the first step (level joints carry no user data).
void NPCharacter::scanUserJoints()
{
    _userJointsScanned = true;
    for (b2Body* body : _body) {
        if (!body) continue;
        for (b2JointEdge* e = body->GetJointList(); e; e = e->next) {
            b2Joint* j = e->joint;
            if (j->GetUserData() != nullptr) continue;
            if (std::find(std::begin(_joint), std::end(_joint), j) != std::end(_joint)) continue;
            if (std::find(_userJoints.begin(), _userJoints.end(), j) != _userJoints.end()) continue;
            _userJoints.push_back(j);
            getSession()->getDestructionListener()->addJointListener(j, this);
        }
    }
}

void NPCharacter::jointWillBeDestroyed(b2Joint* joint)
{
    _userJoints.erase(std::remove(_userJoints.begin(), _userJoints.end(), joint), _userJoints.end());
    for (int i = 0; i < kJointCount; i++) {
        if (_joint[i] == joint) {
            _joint[i] = nullptr;
            _broken[i] = true;
        }
    }
}

void NPCharacter::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                            const b2ContactImpulse* impulse)
{
    (void)otherFixture;
    float maxImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2) maxImpulse = std::max(maxImpulse, impulse->normalImpulses[1]);
    float limit;
    if (fixture == _fixture[kHead]) limit = _headSmashLimit;
    else if (fixture == _fixture[kChest]) limit = _chestSmashLimit;
    else if (fixture == _fixture[kPelvis]) limit = _pelvisSmashLimit;
    else return;
    LevelItemContact& c = _contactResultBufferDict[fixture];
    if (maxImpulse > limit && maxImpulse > c.impulse) {
        c.fixture = fixture;
        c.otherFixture = otherFixture;
        c.impulse = maxImpulse;
    }
}

void NPCharacter::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    // Flash contactAddHandler: Thud sounds for hits faster than 4 m/s.
    contactSoundHandler(fixture, otherFixture, contact, nullptr);
}

void NPCharacter::handleContactResults()
{
    for (Part p : {kHead, kChest, kPelvis}) {
        b2Fixture* f = _fixture[p];
        if (!f || _ground[p]) continue;
        auto it = _contactResultBufferDict.find(f);
        if (it == _contactResultBufferDict.end() || it->second.impulse <= 0.0f) continue;
        _contactResultBufferDict.erase(it);
        _contactAddBufferDict.erase(f);
        if (p == kHead) headSmash();
        else if (p == kChest) chestSmash();
        else pelvisSmash();
    }
}

void NPCharacter::checkJoints()
{
    static const JointId order[] = {kWaist, kNeck, kShoulder1, kShoulder2, kElbow1, kElbow2,
                                    kHip1, kHip2, kKnee1, kKnee2};
    for (JointId id : order) checkRevJoint(id, _jointLimit[id]);
}

void NPCharacter::checkRevJoint(JointId id, float limit)
{
    b2RevoluteJoint* j = _joint[id];
    if (!j || _broken[id]) return;
    const float force = j->GetReactionForce(getTimeStepInverse()).Length();
    if (force > limit || (j->GetAnchorB() - j->GetAnchorA()).LengthSquared() > 0.5f) breakJoint(id);
}

void NPCharacter::breakJoint(JointId id)
{
    switch (id) {
    case kWaist: torsoBreak(); break;
    case kNeck: neckBreak(); break;
    case kShoulder1: shoulderBreak(0); break;
    case kShoulder2: shoulderBreak(1); break;
    case kElbow1: elbowBreak(0); break;
    case kElbow2: elbowBreak(1); break;
    case kHip1: hipBreak(0); break;
    case kHip2: hipBreak(1); break;
    case kKnee1: kneeBreak(0); break;
    case kKnee2: kneeBreak(1); break;
    default: break;
    }
}

// ---------------------------------------------------------------------------------------------
// Death / pose
// ---------------------------------------------------------------------------------------------

void NPCharacter::setDead()
{
    _dead = true;
    if (_voiceSound) {
        _voiceSound->setFinishCallback(nullptr);
        _voiceSound->soundFinishedPlaying();
        _voiceSound = nullptr;
    }
    if (_destroyJointsUponDeath) destroyUserJoints();
    if (flashVersion() < 1.75f) cancelPose();
    else releasePose();
}

void NPCharacter::destroyUserJoints()
{
    if (!_userJointsScanned) scanUserJoints();
    std::vector<b2Joint*> joints;
    joints.swap(_userJoints);
    for (b2Joint* j : joints) destroyJointExternal(j);
}

void NPCharacter::cancelPose()
{
    for (JointId id : {kNeck, kShoulder1, kShoulder2, kElbow1, kElbow2, kHip1, kHip2, kKnee1, kKnee2})
        if (_joint[id]) _joint[id]->EnableMotor(false);
}

void NPCharacter::lockPose()
{
    std::vector<bool> awake = awakeStates();
    for (b2RevoluteJoint* j : _joint) {
        if (!j) continue;
        const float a = j->GetJointAngle();
        j->SetLimits(a, a);
    }
    restoreAwake(awake);
}

// b2RevoluteJoint::SetLimits wakes both bodies; Flash only writes the limits.
std::vector<bool> NPCharacter::awakeStates() const
{
    std::vector<bool> awake;
    for (b2Body* b : _body) awake.push_back(b && b->IsAwake());
    return awake;
}

void NPCharacter::restoreAwake(const std::vector<bool>& awake)
{
    for (int i = 0; i < kPartCount; i++)
        if (_body[i] && !awake[i] && _body[i]->IsAwake()) _body[i]->SetAwake(false);
}

void NPCharacter::releasePose()
{
    std::vector<bool> awake = awakeStates();
    const bool r = _reversed;
    setAbsLimits(kNeck, -20, 20);
    setAbsLimits(kWaist, -5, 5);
    for (int k = 0; k < 2; k++) {
        setAbsLimits((JointId)(kShoulder1 + k), r ? -60 : -180, r ? 180 : 60);
        setAbsLimits((JointId)(kElbow1 + k), r ? 0 : -160, r ? 160 : 0);
        setAbsLimits((JointId)(kHip1 + k), r ? -10 : -150, r ? 150 : 10);
        setAbsLimits((JointId)(kKnee1 + k), r ? -150 : 0, r ? 0 : 150);
    }
    restoreAwake(awake);
}

// ---------------------------------------------------------------------------------------------
// Injuries
// ---------------------------------------------------------------------------------------------

void NPCharacter::setFilter(b2Body* body, const b2Filter& filter)
{
    if (!body) return;
    b2Fixture* f = body->GetFixtureList();
    if (f && f->GetFilterData().groupIndex != -3) f->SetFilterData(filter);
}

void NPCharacter::startFlow(Flow flow, float minSpeed, float maxSpeed, int count, b2Body* body, b2Vec2 local,
                            float flashAngleDeg)
{
    if (!_showGore || !body) return;
    stopFlow(flow);
    EmitterNode* particles = getSession()->getParticlesMidground();
    if (!particles) return;
    FlowEmitter* f = FlowEmitter::createBloodFlow(minSpeed, maxSpeed, count, body, local, -flashAngleDeg);
    if (!f) return;
    f->setEmitterDelegate(this);
    particles->addChildEmitter(f);
    _flow[flow] = f;
    _flowBody[flow] = body;
}

void NPCharacter::stopFlow(Flow flow)
{
    if (FlowEmitter* f = _flow[flow]) {
        f->stop();
        f->setEmitterDelegate(nullptr);
        _flow[flow] = nullptr;
        _flowBody[flow] = nullptr;
    }
}

void NPCharacter::emitterComplete(Emitter* emitter)
{
    for (int i = 0; i < kFlowCount; i++) {
        if (_flow[i] == emitter) {
            _flow[i] = nullptr;
            _flowBody[i] = nullptr;
        }
    }
}

void NPCharacter::burst(b2Body* body, b2Vec2 local, int count)
{
    if (!_showGore || !body) return;
    EmitterNode* particles = getSession()->getParticlesForeground();
    if (!particles) return;
    if (BurstEmitter* b = BurstEmitter::createBloodBurst(5.0f, 15.0f, body, local, count)) particles->addChild(b);
}

void NPCharacter::playSound(const std::string& name, b2Body* body)
{
    if (body) createBodySound(name, body, 1.0f, false);
}

void NPCharacter::destroyJoint(JointId id)
{
    _broken[id] = true;
    if (_joint[id]) {
        getWorld()->DestroyJoint(_joint[id]);
        _joint[id] = nullptr;
    }
}

// A joint other items may hold (level joints, spikes...): tell the destruction listener first.
void NPCharacter::destroyJointExternal(b2Joint* joint)
{
    getSession()->getDestructionListener()->SayGoodbye(joint);
    _userJoints.erase(std::remove(_userJoints.begin(), _userJoints.end(), joint), _userJoints.end());
    getWorld()->DestroyJoint(joint);
}

void NPCharacter::destroyPartBody(Part part)
{
    b2Body* body = _body[part];
    if (!body) return;
    for (int i = 0; i < kJointCount; i++) {
        if (_joint[i] && (_joint[i]->GetBodyA() == body || _joint[i]->GetBodyB() == body)) {
            _joint[i] = nullptr;  // destroyed with the body
            _broken[i] = true;
        }
    }
    for (int i = 0; i < kFlowCount; i++)
        if (_flow[i] && _flowBody[i] == body) stopFlow((Flow)i);
    stopSoundsForBody(body);
    b2Fixture* f = _fixture[part];
    removePostSolve(f);
    removeBeginContact(f);
    _contactResultBufferDict.erase(f);
    _contactAddBufferDict.erase(f);
    _contactAddSounds.erase(f);
    getLevel()->removeFixtureMaterial(f);
    getLevel()->removeFromPaintBody(body);
    if (_partNode[part]) _partNode[part]->setVisible(false);
    _body[part] = nullptr;
    _fixture[part] = nullptr;
    unregisterGrindBody(body);
    getWorld()->DestroyBody(body);
}

b2Body* NPCharacter::createChunk(const std::string& art, float radiusM, b2Vec2 position, float angle,
                                 b2Body* source, Node* nearNode, float fallbackRadiusPx)
{
    b2BodyDef bd;
    bd.type = b2_dynamicBody;
    bd.position = position;
    bd.angle = angle;
    Node* node = chunkNode(art, fallbackRadiusPx);
    node->retain();
    _extraNodes.push_back(node);
    if (_layer) _layer->addChild(node, nearNode ? nearNode->getLocalZOrder() : 0);
    bd.userData = node;
    b2Body* body = getWorld()->CreateBody(&bd);
    b2CircleShape circle;
    circle.m_radius = radiusM;
    b2FixtureDef fd;
    fd.shape = &circle;
    fd.density = 1.0f;
    fd.friction = 0.3f;
    fd.restitution = 0.1f;
    fd.filter = _zeroFilter;
    b2Fixture* fixture = body->CreateFixture(&fd);
    getLevel()->addFixtureMaterial(fixture, 1);
    body->ResetMassData();
    body->SetLinearVelocity(source->GetLinearVelocityFromWorldPoint(position));
    body->SetAngularVelocity(source->GetAngularVelocity());
    getLevel()->addToPaintBody(body);
    node->setPosition(Vec2(position.x * getPtm(), position.y * getPtm()));
    node->setRotation(angle * -57.29578f);
    return body;
}

void NPCharacter::headSmash()
{
    setDead();
    if (!_showGore) return;
    b2Body* head = _body[kHead];
    if (!head) return;
    if (!_broken[kNeck]) {
        _broken[kNeck] = true;  // the joint goes with the head body
        setPartFrame(kChest, _partFrame[kChest] + 1);
        const float sgn = _reversed ? -1.0f : 1.0f;
        startFlow(kNeckFlow, 2.5f, 4.0f, 500, _body[kChest],
                  b2Vec2(sgn * _data->spineRef[0] / kCharacterScale, -_data->spineRef[1] / kCharacterScale), 270.0f);
    } else {
        stopFlow(kHeadFlow);
    }
    const b2Vec2 pos = head->GetPosition();
    float a = -head->GetAngle();  // Flash angle
    const float d = 4.0f / kCharacterScale;
    const std::string prefix = partName(kHead) + "_";
    for (int i = 1; i < 5; i++) {
        // Flash head chunks keep angle 0
        createChunk(prefix + "chunk" + std::to_string(i), _headChunkRadius / kCharacterScale,
                    pos + b2Vec2(std::sin(a) * d, -std::cos(a) * d), 0.0f, head, _partNode[kHead],
                    _headChunkRadius);
        a += b2_pi / 2;
    }
    b2Body* brain = createChunk(prefix + "brain", 12.0f / kCharacterScale, pos, head->GetAngle(), head,
                                _partNode[kHead], 12.0f);
    brain->SetLinearVelocity(head->GetLinearVelocity());
    burst(head, b2Vec2_zero, 200);
    playSound("HeadSmash", brain);
    destroyPartBody(kHead);
}

void NPCharacter::chestSmash()
{
    setDead();
    if (!_showGore) return;
    if (!_broken[kNeck]) neckBreak(false, false);
    if (!_broken[kShoulder1]) shoulderBreak(0, false);
    if (!_broken[kShoulder2]) shoulderBreak(1, false);
    if (!_broken[kWaist]) torsoBreak(false);
    b2Body* chest = _body[kChest];
    if (!chest) return;
    const b2Vec2 pos = chest->GetPosition();
    float a = -chest->GetAngle() + b2_pi / 4;
    const float d = 5.0f / kCharacterScale;
    const std::string prefix = partName(kChest) + "_";
    for (int i = 1; i < 5; i++) {
        createChunk(prefix + "chunk" + std::to_string(i), _chestChunkRadius / kCharacterScale,
                    pos + b2Vec2(std::sin(a) * d, -std::cos(a) * d), chest->GetAngle(), chest,
                    _partNode[kChest], _chestChunkRadius);
        a += b2_pi / 2;
    }
    _heartBody = createChunk(prefix + "heart", _headChunkRadius / kCharacterScale, pos, chest->GetAngle(), chest,
                             _partNode[kChest], _headChunkRadius);
    _heartBody->SetLinearVelocity(chest->GetLinearVelocity());
    for (Flow f : {kStomachFlow, kNeckFlow, kShoulder1Flow, kShoulder2Flow}) stopFlow(f);
    burst(chest, b2Vec2_zero, 300);
    playSound("ChestSmash", _heartBody);
    destroyPartBody(kChest);
}

void NPCharacter::pelvisSmash()
{
    if (!_showGore) return;
    if (!_broken[kHip1]) hipBreak(0, false);
    if (!_broken[kHip2]) hipBreak(1, false);
    if (!_broken[kWaist]) torsoBreak();
    stopFlow(kHip1Flow);
    stopFlow(kHip2Flow);
    b2Body* pelvis = _body[kPelvis];
    if (!pelvis) return;
    const b2Vec2 pos = pelvis->GetPosition();
    float a = -pelvis->GetAngle();
    const float d = 3.0f / kCharacterScale;
    const std::string prefix = partName(kPelvis) + "_";
    b2Body* last = nullptr;
    for (int i = 1; i < 4; i++) {
        last = createChunk(prefix + "chunk" + std::to_string(i), _pelvisChunkRadius / kCharacterScale,
                           pos + b2Vec2(std::sin(a) * d, -std::cos(a) * d), pelvis->GetAngle(), pelvis,
                           _partNode[kPelvis], _pelvisChunkRadius);
        a += b2_pi * 2 / 3;
    }
    burst(pelvis, b2Vec2_zero, 200);
    playSound("PelvisSmash", last);
    destroyPartBody(kPelvis);
    addVocals("Pelvis", 5);
}

void NPCharacter::torsoBreak(bool blood, bool sound)
{
    destroyJoint(kWaist);
    setPartFrame(kChest, _partFrame[kChest] + 4);
    setPartFrame(kPelvis, _partFrame[kPelvis] + 1);
    setFilter(_body[kPelvis], _lowerBodyFilter);
    for (Part p : {kUpperLeg1, kUpperLeg2, kLowerLeg1, kLowerLeg2}) setFilter(_body[p], _lowerBodyFilter);
    if (blood) startFlow(kStomachFlow, 2.0f, 3.0f, 500, _body[kChest], b2Vec2_zero, 90.0f);
    if (sound) playSound("LimbRip1", _body[kPelvis]);
    addVocals("Torso", 5);
}

void NPCharacter::neckBreak(bool blood, bool sound)
{
    setDead();
    destroyJoint(kNeck);
    setPartFrame(kHead, 2);
    setPartFrame(kChest, _partFrame[kChest] + 1);
    setFilter(_body[kHead], _zeroFilter);
    startFlow(kHeadFlow, 2.5f, 4.0f, 150, _body[kHead], b2Vec2_zero, 90.0f);
    if (blood) {
        const float sgn = _reversed ? -1.0f : 1.0f;
        startFlow(kNeckFlow, 2.5f, 4.0f, 500, _body[kChest],
                  b2Vec2(sgn * _data->spineRef[0] / kCharacterScale, -_data->spineRef[1] / kCharacterScale), 270.0f);
    }
    if (sound) playSound("NeckBreak", _body[kHead]);
}

void NPCharacter::shoulderBreak(int k, bool blood)
{
    const JointId id = (JointId)(kShoulder1 + k);
    b2RevoluteJoint* j = _joint[id];
    const b2Vec2 anchorA = j ? j->GetLocalAnchorA() : b2Vec2_zero;
    const b2Vec2 anchorB = j ? j->GetLocalAnchorB() : b2Vec2_zero;
    destroyJoint(id);
    const Part upper = (Part)(kUpperArm1 + k), lower = (Part)(kLowerArm1 + k);
    if (b2Body* ua = _body[upper]) {
        b2Fixture* f = ua->GetFixtureList();
        if (f->GetFilterData().groupIndex != -3) {
            f->SetFilterData(_zeroFilter);
            getLevel()->addFixtureMaterial(f, 2);
            startFlow(k == 0 ? kArm1Flow : kArm2Flow, 1.0f, 3.0f, 200, ua, anchorB, 270.0f);
        }
    }
    setFilter(_body[lower], _zeroFilter);
    setPartFrame(upper, _partFrame[upper] + 1);
    if (k == 0) setPartFrame(kChest, _partFrame[kChest] + 2);
    if (blood) {
        startFlow(k == 0 ? kShoulder1Flow : kShoulder2Flow, 0.0f, 1.0f, 500, _body[kChest], anchorA, 270.0f);
        playSound("LimbRip2", _body[upper]);
    }
    addVocals(k == 0 ? "Shoulder1" : "Shoulder2", 3);
}

// elbowBreak / kneeBreak: the lower limb also loses every other (non-sensor) joint, user joints
// and impalements included.
void NPCharacter::elbowBreak(int k)
{
    const JointId id = (JointId)(kElbow1 + k);
    // Flash elbowBreak2 reads elbowJoint1's anchor (sic).
    const b2Vec2 anchor = k == 0 && _joint[id] ? _joint[id]->GetLocalAnchorB() : _elbow1LocalAnchorB;
    destroyJoint(id);
    const Part upper = (Part)(kUpperArm1 + k), lower = (Part)(kLowerArm1 + k);
    b2Body* la = _body[lower];
    if (la) {
        std::vector<b2Joint*> others;
        for (b2JointEdge* e = la->GetJointList(); e; e = e->next) {
            b2Fixture* of = e->other->GetFixtureList();
            if (of && !of->IsSensor()) others.push_back(e->joint);
        }
        for (b2Joint* j : others) destroyJointExternal(j);
    }
    setFilter(la, _zeroFilter);
    setPartFrame(lower, 2);
    setPartFrame(upper, _partFrame[upper] + 2);
    burst(la, anchor, 50);
    playSound("BoneBreak" + std::to_string(1 + std::rand() % 4), la);
    addVocals(k == 0 ? "Elbow1" : "Elbow2", 1);
}

void NPCharacter::hipBreak(int k, bool blood)
{
    const JointId id = (JointId)(kHip1 + k);
    b2RevoluteJoint* j = _joint[id];
    const b2Vec2 anchorB = j ? j->GetLocalAnchorB() : b2Vec2_zero;
    destroyJoint(id);
    const Part upper = (Part)(kUpperLeg1 + k), lower = (Part)(kLowerLeg1 + k);
    if (b2Body* ul = _body[upper]) {
        b2Fixture* f = ul->GetFixtureList();
        if (f->GetFilterData().groupIndex != -3) {
            f->SetFilterData(_zeroFilter);
            getLevel()->addFixtureMaterial(f, 2);
            startFlow(k == 0 ? kThigh1Flow : kThigh2Flow, 1.0f, 3.0f, 200, ul, anchorB, 270.0f);
        }
    }
    setFilter(_body[lower], _zeroFilter);
    setPartFrame(upper, _partFrame[upper] + 1);
    if (k == 0) setPartFrame(kPelvis, _partFrame[kPelvis] + 2);
    if (blood) {
        startFlow(k == 0 ? kHip1Flow : kHip2Flow, 0.0f, 1.0f, 500, _body[kPelvis], b2Vec2_zero, 90.0f);
        playSound(k == 0 ? "LimbRip3" : "LimbRip4", _body[upper]);
    }
    addVocals(k == 0 ? "Hip1" : "Hip2", 4);
}

void NPCharacter::kneeBreak(int k)
{
    const JointId id = (JointId)(kKnee1 + k);
    const b2Vec2 anchor = _joint[id] ? _joint[id]->GetLocalAnchorB() : b2Vec2_zero;
    destroyJoint(id);
    const Part upper = (Part)(kUpperLeg1 + k), lower = (Part)(kLowerLeg1 + k);
    b2Body* ll = _body[lower];
    if (ll) {
        std::vector<b2Joint*> others;
        for (b2JointEdge* e = ll->GetJointList(); e; e = e->next) {
            b2Fixture* of = e->other->GetFixtureList();
            if (of && !of->IsSensor()) others.push_back(e->joint);
        }
        for (b2Joint* j : others) destroyJointExternal(j);
        if (b2Fixture* f = ll->GetFixtureList()) {
            if (f->GetFilterData().groupIndex != -3) {
                f->SetFilterData(_zeroFilter);
                f->SetSensor(false);
            }
        }
    }
    setPartFrame(lower, 2);
    setPartFrame(upper, _partFrame[upper] + 2);
    burst(ll, anchor, 50);
    playSound("BoneBreak" + std::to_string(1 + std::rand() % 4), _body[upper]);
    addVocals(k == 0 ? "Knee1" : "Knee2", 2);
}

// ---------------------------------------------------------------------------------------------
// Voice
// ---------------------------------------------------------------------------------------------

void NPCharacter::addVocals(const char* name, int priority)
{
    _voiceArray[priority] = name;
    _voiceTop = std::max(_voiceTop, priority);
}

void NPCharacter::checkVocals()
{
    const int top = _voiceTop;
    if (_voiceSound) {
        if (_voiceSound->isPlaying()) {
            if (top > _voicePriority) {
                _voiceSound->setFinishCallback(nullptr);
                _voiceSound->soundFinishedPlaying();
                _voiceSound = nullptr;
            } else {
                return;
            }
        } else {
            _voiceSound->setFinishCallback(nullptr);
            _voiceSound = nullptr;
            _voicePriority = -1;
        }
    }
    if (top < 0 || !_body[kHead]) return;
    _voiceSound = createBodySound(_tag + _voiceArray[top], _body[kHead], 1.0f, false);
    _voicePriority = top;
    if (_voiceSound) {
        _voiceSound->setFinishCallback([this](int&) {
            _voiceSound = nullptr;
            _voicePriority = -1;
        });
    }
}

void NPCharacter::randomVocals(b2Fixture* fixture)
{
    static const char* const names[] = {"Elbow1", "Elbow2", "Knee1", "Knee2", "Shoulder1",
                                        "Shoulder2", "Hip1", "Hip2", "Knee1", "Spikes"};
    // Flash: the last digit of the body's x position.
    const int index = (int)std::fmod(std::floor(std::fabs(fixture->GetBody()->GetPosition().x) * 10000.0f), 10.0f);
    addVocals(names[std::max(0, std::min(9, index))], 5);
}

// ---------------------------------------------------------------------------------------------
// Interaction with other items
// ---------------------------------------------------------------------------------------------

int NPCharacter::getFluidType()
{
    return _showGore ? 1 : 0;
}

int NPCharacter::shapeImpale(b2Fixture* fixture, bool impale, b2Vec2 point, float radius)
{
    if (fixture == _fixture[kChest] || fixture == _fixture[kPelvis]) {
        randomVocals(fixture);
    } else if (fixture && fixture == _fixture[kHead]) {
        const bool noPoint = point.x == INFINITY || point.y == INFINITY;
        if (impale && (noPoint || (point - fixture->GetBody()->GetWorldCenter()).LengthSquared() < radius)) {
            setDead();
        } else {
            randomVocals(fixture);
        }
    }
    return getFluidType();
}

void NPCharacter::explodeShape(b2Fixture* fixture, float force)
{
    if (!fixture) return;
    if (fixture == _fixture[kHead]) {
        if (force > 0.85f) {
            _contactResultBufferDict.erase(fixture);
            _contactAddBufferDict.erase(fixture);
            headSmash();
        }
    } else if (fixture == _fixture[kChest]) {
        if (force > std::max(1.0f - 0.15f * _body[kChest]->GetMass() / kDefChestMass, 0.7f)) {
            _contactResultBufferDict.erase(fixture);
            _contactAddBufferDict.erase(fixture);
            chestSmash();
        }
    } else if (fixture == _fixture[kPelvis]) {
        if (force > std::max(1.0f - 0.15f * _body[kPelvis]->GetMass() / kDefPelvisMass, 0.7f)) {
            _contactResultBufferDict.erase(fixture);
            _contactAddBufferDict.erase(fixture);
            pelvisSmash();
        }
    }
}

// Flash getJointBody: the part whose centre is nearest to the anchor; a lower limb that already
// has more than one joint is skipped.
b2Body* NPCharacter::getJointBody(b2Vec2 point)
{
    if (!_interactive || _inGroup) return nullptr;
    b2Body* best = nullptr;
    float bestD = 10000000.0f;
    for (int p = 0; p < kPartCount; p++) {
        b2Body* body = _body[p];
        if (!body) continue;
        const float d = (point - body->GetWorldCenter()).LengthSquared();
        if (d >= bestD) continue;
        int joints = 0;
        for (b2JointEdge* e = body->GetJointList(); e; e = e->next) joints++;
        const bool lower = p == kLowerArm1 || p == kLowerArm2 || p == kLowerLeg1 || p == kLowerLeg2;
        if (lower && joints > 1) continue;
        bestD = d;
        best = body;
    }
    return best;
}

std::vector<b2Body*> NPCharacter::getBodyList()
{
    std::vector<b2Body*> list;
    if (_body[kChest]) list.push_back(_body[kChest]);
    if (_body[kHead]) list.push_back(_body[kHead]);
    return list;
}

void NPCharacter::triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties)
{
    (void)trigger;
    if (!_interactive || _inGroup) return;
    if (action == 0) {  // wake from sleep
        if (_body[kChest]) _body[kChest]->SetAwake(true);
        return;
    }
    if (_destroyJointsUponDeath && _dead) return;
    if (action == 1) {  // apply impulse: velocity change (Flash px/s y down -> m/s y up), spin
        auto prop = [&](size_t i) {
            return i < properties.size() && std::isfinite(properties[i]) ? properties[i] : 0.0f;
        };
        const float ix = prop(0), iy = prop(1), spin = prop(2);
        std::vector<b2Body*> bodies = {_body[kChest], _body[kHead], _body[kPelvis]};
        for (int k = 0; k < 2; k++) {
            if (!_broken[kShoulder1 + k]) {
                bodies.push_back(_body[kUpperArm1 + k]);
                if (!_broken[kElbow1 + k]) bodies.push_back(_body[kLowerArm1 + k]);
            }
        }
        for (int k = 0; k < 2; k++) {
            if (!_broken[kHip1 + k]) {
                bodies.push_back(_body[kUpperLeg1 + k]);
                if (!_broken[kKnee1 + k]) bodies.push_back(_body[kLowerLeg1 + k]);
            }
        }
        for (b2Body* b : bodies) {
            if (!b) continue;
            const float m = b->GetMass();
            b->ApplyLinearImpulse(b2Vec2(ix * m, -iy * m), b->GetWorldCenter(), true);
        }
        if (spin != 0.0f && _body[kChest])
            _body[kChest]->SetAngularVelocity(_body[kChest]->GetAngularVelocity() - spin * 10.0f);
        return;
    }
    if (action == 2) lockPose();  // hold pose
    else releasePose();           // release pose (and anything else, as Flash)
}

}  // namespace online

namespace online {

// ---------------------------------------------------------------------------------------------
// Lawnmower Man blade (Flash NPCharacter.grindShape / removeBody).

void NPCharacter::grindFixture(b2Fixture* fixture)
{
    int part = -1;
    for (int p = 0; p < kPartCount; p++) {
        if (_fixture[p] == fixture) part = p;
    }
    if (part < 0) return;
    if (part == kHead || part == kChest || part == kPelvis) {
        _ground[part] = true;
        _contactResultBufferDict.erase(fixture);
        _contactAddBufferDict.erase(fixture);
        removePostSolve(fixture);
        removeBeginContact(fixture);
    }
    switch (part) {
    case kHead: stopFlow(kHeadFlow); break;
    case kChest:
        stopFlow(kNeckFlow);
        stopFlow(kShoulder1Flow);
        stopFlow(kShoulder2Flow);
        stopFlow(kStomachFlow);
        break;
    case kPelvis:
        stopFlow(kHip1Flow);
        stopFlow(kHip2Flow);
        break;
    case kUpperArm1: stopFlow(kArm1Flow); break;
    case kUpperArm2: stopFlow(kArm2Flow); break;
    case kUpperLeg1: stopFlow(kThigh1Flow); break;
    case kUpperLeg2: stopFlow(kThigh2Flow); break;
    default: break;
    }
}

void NPCharacter::grindBody(b2Body* body)
{
    int part = -1;
    for (int p = 0; p < kPartCount; p++) {
        if (_body[p] == body) part = p;
    }
    if (part < 0) return;
    // Flash skips the blood of a break when the torso part it would bleed from is being ground.
    const bool chestBlood = !_ground[kChest];
    const bool pelvisBlood = !_ground[kPelvis];
    switch (part) {
    case kHead:
        if (!_broken[kNeck]) neckBreak(chestBlood, true);
        break;
    case kChest:
        if (!_broken[kNeck]) neckBreak(false, true);
        if (!_broken[kShoulder1]) shoulderBreak(0, false);
        if (!_broken[kShoulder2]) shoulderBreak(1, false);
        if (!_broken[kWaist]) torsoBreak(false, true);
        break;
    case kPelvis:
        if (!_broken[kHip1]) hipBreak(0, false);
        if (!_broken[kHip2]) hipBreak(1, false);
        if (!_broken[kWaist]) torsoBreak(false, true);
        break;
    case kUpperArm1:
        if (!_broken[kShoulder1]) shoulderBreak(0, chestBlood);
        if (!_broken[kElbow1]) elbowBreak(0);
        break;
    case kUpperArm2:
        if (!_broken[kShoulder2]) shoulderBreak(1, chestBlood);
        if (!_broken[kElbow2]) elbowBreak(1);
        break;
    case kUpperLeg1:
        if (!_broken[kHip1]) hipBreak(0, pelvisBlood);
        if (!_broken[kKnee1]) kneeBreak(0);
        break;
    case kUpperLeg2:
        if (!_broken[kHip2]) hipBreak(1, pelvisBlood);
        if (!_broken[kKnee2]) kneeBreak(1);
        break;
    default: break;
    }
    // The mower deactivates and hides the body; nothing of ours may move it any more.
    unregisterGrindBody(body);
}

}  // namespace online
