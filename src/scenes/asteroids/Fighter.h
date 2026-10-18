#pragma once

#include "gfx/MeshData.h"

namespace demo {

// Material ids used by the fighter mesh (interpreted by shaders/scenes/asteroids/ship.frag).
enum FighterMaterial { kFighterHull = 0, kFighterGlass = 1, kFighterEngine = 2, kFighterNavRed = 3, kFighterNavGreen = 4 };

// Small space fighter, ~4.3 units long, nose along +z, flat-shaded hard-surface panels.
MeshData makeFighter();

}  // namespace demo
