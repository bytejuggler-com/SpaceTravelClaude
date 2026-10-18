#pragma once

#include "gfx/MeshData.h"

#include <glm/glm.hpp>

namespace demo {

// Material ids used by the dreadnought mesh (interpreted by shaders/scenes/dreadnought/hull.frag).
enum DreadnoughtMaterial { kDreadHull = 0, kDreadEngine = 1, kDreadDark = 2, kDreadDome = 3, kDreadWindow = 4 };

// Original wedge-shaped capital ship, 200 units long, nose along +z, top along +y, centered on the origin.
// Ridged dagger hull, three stepped superstructure tiers, command tower with bridge and twin domes, ventral
// reactor bulb, engine block with three large and four small nozzles (glowing discs at z ~ -113).
// Detail layers: a procedural "city" of blocks on the decks, armored rims along every edge, turret batteries,
// lit windows and hangar openings in the side walls, underside armor plates and a docking bay, tower and
// engine-block details. Blocks carry baked contact occlusion (darker at their base).
MeshData makeDreadnought();

// Where the engine glow sits, for lighting the rear hull.
inline constexpr glm::vec3 kDreadnoughtEngineCenter{0.0f, 0.0f, -112.0f};

}  // namespace demo
