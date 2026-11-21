#pragma once
// ACCESSIBILITY (PC addition): the colour filter (qol::ColorFilter). After the frame is drawn, the
// screen is copied into a texture and drawn back through a 3x3 colour matrix. Nothing is created
// or run while the filter is off.

namespace qol {

// Registers the per-frame hook (call once, after the GL view exists).
void installColorFilter();

}  // namespace qol
