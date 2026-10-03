#pragma once
// Liquid / realistic blood (qol::BloodStyle::Liquid / Realistic), after the browser game's blood
// settings 3 and 4: each frame the blood particles of a layer are drawn into an offscreen texture,
// which is then composited with a shader that blurs and thresholds the particle cloud into solid
// blobs (setting 3) and, for setting 4, lights the blob surface like a bevel (gloss + shading).

#include <vector>

#include "cocos2d.h"

namespace qol {

class BloodCompositor {
public:
    // True while the current blood style needs compositing.
    static bool active();
    // Draws `blood` (children of `layer`) through the compositor. layerTransform is the layer's
    // model-view transform for this frame (children are visited with it as their parent).
    static void draw(cocos2d::Node* layer, const std::vector<cocos2d::Node*>& blood, cocos2d::Renderer* renderer,
                     const cocos2d::Mat4& layerTransform, uint32_t flags);
};

}  // namespace qol
