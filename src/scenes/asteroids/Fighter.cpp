#include "scenes/asteroids/Fighter.h"

#include <glm/gtc/matrix_transform.hpp>

namespace demo {

namespace {

// Loft axes are (thickness, chord, span) = (x, y, z); these place a loft so its span runs along world x
// (wings) or world y (fins), with the chord along world z.
glm::mat4 spanAlongX(const glm::vec3& at) {
    glm::mat4 m(0.0f);
    m[0] = glm::vec4(0, 1, 0, 0);  // thickness -> y
    m[1] = glm::vec4(0, 0, 1, 0);  // chord -> z
    m[2] = glm::vec4(1, 0, 0, 0);  // span -> x
    m[3] = glm::vec4(at, 1);
    return m;
}

glm::mat4 spanAlongY(const glm::vec3& at) {
    glm::mat4 m(0.0f);
    m[0] = glm::vec4(1, 0, 0, 0);  // thickness -> x
    m[1] = glm::vec4(0, 0, 1, 0);  // chord -> z
    m[2] = glm::vec4(0, 1, 0, 0);  // span -> y
    m[3] = glm::vec4(at, 1);
    return m;
}

const glm::mat4 kMirrorX = glm::scale(glm::mat4(1.0f), glm::vec3(-1.0f, 1.0f, 1.0f));

}  // namespace

MeshData makeFighter() {
    MeshData ship;

    // Fuselage: hexagonal section, pointed nose, slightly narrower tail.
    ship.append(makeLoft({{2.3f, {0.03f, 0.02f}},
                          {1.4f, {0.30f, 0.20f}},
                          {0.2f, {0.48f, 0.32f}},
                          {-1.2f, {0.44f, 0.30f}},
                          {-1.7f, {0.34f, 0.24f}}},
                         6, kFighterHull)
                    .flattened());

    // Canopy bubble on top.
    ship.append(makeLoft({{1.15f, {0.02f, 0.02f}, {0.0f, 0.16f}},
                          {0.75f, {0.19f, 0.15f}, {0.0f, 0.22f}},
                          {0.05f, {0.21f, 0.14f}, {0.0f, 0.25f}},
                          {-0.45f, {0.06f, 0.05f}, {0.0f, 0.20f}}},
                         8, kFighterGlass)
                    .flattened());

    // Swept wings, thinning toward the tips.
    const MeshData wing = makeLoft({{0.0f, {0.07f, 0.75f}}, {2.3f, {0.035f, 0.28f}, {0.0f, -0.55f}}}, 4, kFighterHull).flattened();
    ship.append(wing, spanAlongX({0.3f, -0.05f, -0.5f}));
    ship.append(wing, kMirrorX * spanAlongX({0.3f, -0.05f, -0.5f}));

    // Engine pods at the wing roots and their glowing nozzles, plus the main engine.
    const MeshData pod = makeLoft({{0.1f, {0.12f, 0.12f}}, {-0.3f, {0.2f, 0.2f}}, {-1.8f, {0.2f, 0.2f}}, {-1.92f, {0.17f, 0.17f}}},
                                  10, kFighterHull)
                             .flattened();
    const MeshData nozzle = makeLoft({{-1.9f, {0.15f, 0.15f}}, {-1.98f, {0.15f, 0.15f}}}, 10, kFighterEngine).flattened();
    for (const glm::mat4& side : {glm::mat4(1.0f), kMirrorX}) {
        const glm::mat4 at = side * glm::translate(glm::mat4(1.0f), {0.85f, -0.05f, 0.0f});
        ship.append(pod, at);
        ship.append(nozzle, at);
    }
    ship.append(makeLoft({{-1.68f, {0.26f, 0.18f}}, {-1.78f, {0.26f, 0.18f}}}, 6, kFighterEngine).flattened());

    // Twin tail fins, canted outward.
    const MeshData fin = makeLoft({{0.0f, {0.03f, 0.45f}}, {0.7f, {0.02f, 0.2f}, {0.0f, -0.3f}}}, 4, kFighterHull).flattened();
    const glm::mat4 cant = glm::rotate(glm::mat4(1.0f), -0.35f, {0.0f, 0.0f, 1.0f});
    ship.append(fin, glm::translate(glm::mat4(1.0f), {0.25f, 0.18f, -1.2f}) * cant * spanAlongY({0.0f, 0.0f, 0.0f}));
    ship.append(fin, kMirrorX * glm::translate(glm::mat4(1.0f), {0.25f, 0.18f, -1.2f}) * cant * spanAlongY({0.0f, 0.0f, 0.0f}));

    // Navigation lights at the wing tips. With the nose along +z and up +y, +x is the port (left) side: red.
    const MeshData redLight = makeLoft({{-0.06f, {0.05f, 0.05f}}, {0.06f, {0.05f, 0.05f}}}, 4, kFighterNavRed).flattened();
    MeshData greenLight = redLight;
    for (Vertex& v : greenLight.vertices) v.material = kFighterNavGreen;
    ship.append(redLight, glm::translate(glm::mat4(1.0f), {2.62f, -0.05f, -1.05f}));
    ship.append(greenLight, glm::translate(glm::mat4(1.0f), {-2.62f, -0.05f, -1.05f}));

    return ship;
}

}  // namespace demo
