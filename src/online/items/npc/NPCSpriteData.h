#pragma once
// ONLINE (PC addition): layout of the browser game's NPC skins (editor/specials/npcsprites/
// NPCSprite1..16, the posable "non-player character" ragdolls), read from the SWF by
// tools/assets/gen_npc_tables.py into NPCSpriteData.inc. Numbers only, no art.
//
// NPCSprite display tree (NPCSprite.as): headOuter > head, chest, pelvis,
// arm<k> > upperArm<k> + lowerArmOuter<k> > lowerArm<k>, leg<k> > upperLeg<k> + lowerLegOuter<k>
// > lowerLeg<k>; every body part holds an invisible 100x100 "shape" guide whose scale is the
// Box2D shape (box; the head's is a circle of diameter 100).

namespace online {
namespace npc {

// Flash placement matrix (x' = a x + c y + tx, y' = b x + d y + ty; Flash px, y down).
struct Mat {
    float a, b, c, d, tx, ty;
};

struct SpriteData {
    Mat headOuter, head, chest, pelvis;
    Mat arm[2], upperArm[2], lowerArmOuter[2], lowerArm[2];
    Mat leg[2], upperLeg[2], lowerLegOuter[2], lowerLeg[2];
    // shape guide scaleX / scaleY
    float headShape[2], chestShape[2], pelvisShape[2];
    float upperArmShape[2][2], lowerArmShape[2][2], upperLegShape[2][2], lowerLegShape[2][2];
    float pelvisHeight;  // pelvis.height in NPCSprite space (frame 1, guide included)
    float spineRef[2];   // chest's spineRef position (chest frame 2)
    // Top-level children bottom to top: 0 headOuter 1 chest 2 pelvis 3 arm1 4 arm2 5 leg1 6 leg2
    int order[7];
};

const int kSpriteCount = 16;

// charIndex 1..16 (clamped).
const SpriteData& spriteData(int charIndex);

}  // namespace npc
}  // namespace online
