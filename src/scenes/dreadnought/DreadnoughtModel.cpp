#include "scenes/dreadnought/DreadnoughtModel.h"

#include "gfx/CpuNoise.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

namespace demo {

namespace {

// ---------------------------------------------------------------------------------------------------------
// Shape description shared by the big forms and the detail placement

// Main hull cross-sections: z, half width, half height (ridge height above the center line).
struct HullRing {
    float z, halfWidth, halfHeight;
};
constexpr std::array<HullRing, 4> kHull = {{{-100.0f, 55.0f, 9.0f}, {-20.0f, 33.0f, 6.5f}, {60.0f, 11.0f, 2.8f}, {100.0f, 0.6f, 0.4f}}};

// Superstructure tiers: flat-topped wedges from `rear` to `front`, tapering to 2 units wide at the front.
struct Tier {
    float rear, front, rearHalfWidth, lift;
};
constexpr float kTierHalfHeight = 2.5f;
constexpr std::array<Tier, 3> kTiers = {{{-96.0f, 4.0f, 33.0f, 2.5f}, {-96.0f, -12.0f, 25.85f, 7.0f}, {-96.0f, -28.0f, 18.7f, 11.5f}}};

float hullProfile(float z, float HullRing::*field) {
    z = std::clamp(z, kHull.front().z, kHull.back().z);
    for (size_t i = 0; i + 1 < kHull.size(); ++i) {
        if (z <= kHull[i + 1].z) {
            const float t = (z - kHull[i].z) / (kHull[i + 1].z - kHull[i].z);
            return kHull[i].*field + (kHull[i + 1].*field - kHull[i].*field) * t;
        }
    }
    return kHull.back().*field;
}
float ridgeHeight(float z) { return hullProfile(z, &HullRing::halfHeight); }
float hullHalfWidth(float z) { return hullProfile(z, &HullRing::halfWidth); }

float tierHalfWidth(const Tier& tier, float z) {
    const float t = (z - tier.rear) / (tier.front - tier.rear);
    return tier.rearHalfWidth + (2.0f - tier.rearHalfWidth) * t;
}
float tierCenter(const Tier& tier, float z) { return ridgeHeight(z) + tier.lift; }
float tierTop(const Tier& tier, float z) { return tierCenter(tier, z) + kTierHalfHeight; }
bool onTier(const Tier& tier, float x, float z) {
    return z >= tier.rear && z <= tier.front && std::abs(x) < tierHalfWidth(tier, z);
}

// Height of the topmost surface at (x, z): the hull's ridged top or a tier deck. NaN off the ship.
float topSurface(float x, float z) {
    float best = std::numeric_limits<float>::quiet_NaN();
    const float w = hullHalfWidth(z);
    if (std::abs(x) < w) best = ridgeHeight(z) * (1.0f - 0.5f * std::abs(x) / w);
    for (const Tier& tier : kTiers)
        if (onTier(tier, x, z)) best = std::isnan(best) ? tierTop(tier, z) : std::max(best, tierTop(tier, z));
    return best;
}

// Underside (mirror of the hull top).
float bottomSurface(float x, float z) {
    const float w = hullHalfWidth(z);
    return -ridgeHeight(z) * (1.0f - 0.5f * std::min(std::abs(x) / w, 1.0f));
}

// The command tower stands here on the top deck; keep the city away from it.
bool inTowerFootprint(float x, float z) { return std::abs(x) < 7.0f && z > -91.0f && z < -75.0f; }

// ---------------------------------------------------------------------------------------------------------
// Primitive builders

// Axis-aligned box; `contactOcclusion` darkens the vertices touching the hull (the bottom ones, or the top
// ones for fittings hanging under the hull), giving a contact shadow that fades away from the hull.
MeshData box(const glm::vec3& center, const glm::vec3& halfSize, float material, float contactOcclusion = 1.0f,
             bool hanging = false) {
    MeshData mesh = makeLoft({{center.z - halfSize.z, {halfSize.x, halfSize.y}, {center.x, center.y}},
                              {center.z + halfSize.z, {halfSize.x, halfSize.y}, {center.x, center.y}}},
                             4, material)
                        .flattened();
    if (contactOcclusion < 1.0f)
        for (Vertex& v : mesh.vertices)
            v.occlusion = (hanging ? v.position.y > center.y : v.position.y < center.y) ? contactOcclusion : 1.0f;
    return mesh;
}

MeshData cylinder(const glm::vec2& center, float zBack, float zFront, float radiusBack, float radiusFront, float material,
                  int sides = 20) {
    return makeLoft({{zBack, {radiusBack, radiusBack}, center}, {zFront, {radiusFront, radiusFront}, center}}, sides, material)
        .flattened();
}

MeshData ellipsoid(const glm::vec3& center, const glm::vec3& radii, float material, int subdivisions = 3) {
    MeshData sphere = makeIcosphere(subdivisions);
    for (Vertex& v : sphere.vertices) v.material = material;
    MeshData result;
    result.append(sphere, glm::scale(glm::translate(glm::mat4(1.0f), center), radii));
    return result;
}

class Random {
public:
    explicit Random(std::uint32_t seed) : m_engine(seed) {}
    float operator()() { return m_unit(m_engine); }
    float range(float a, float b) { return a + (b - a) * (*this)(); }

private:
    std::mt19937 m_engine;
    std::uniform_real_distribution<float> m_unit{0.0f, 1.0f};
};

// ---------------------------------------------------------------------------------------------------------
// Big forms

void addBigForms(MeshData& ship) {
    // Dagger hull: hexagonal section (central ridge top and bottom, short vertical side walls).
    std::vector<LoftRing> hull;
    for (const HullRing& ring : kHull) hull.push_back({ring.z, {ring.halfWidth, ring.halfHeight}});
    ship.append(makeLoft(hull, 6, kDreadHull).flattened());

    // Stepped superstructure tiers, following the hull's slope toward the nose.
    for (const Tier& tier : kTiers) {
        auto ringAt = [&](float z) { return LoftRing{z, {tierHalfWidth(tier, z), kTierHalfHeight}, {0.0f, tierCenter(tier, z)}}; };
        std::vector<LoftRing> rings = {ringAt(tier.rear)};
        if (tier.front > -20.0f) rings.push_back(ringAt(-20.0f));
        rings.push_back(ringAt(tier.front));
        ship.append(makeLoft(rings, 4, kDreadHull).flattened());
    }

    // Ventral reactor bulb.
    ship.append(ellipsoid({0.0f, -ridgeHeight(-45.0f) - 0.5f, -45.0f}, {14.0f, 5.5f, 20.0f}, kDreadHull));

    // Engine block, three large and four small nozzles with glowing discs at the back.
    ship.append(box({0.0f, 0.0f, -103.5f}, {37.0f, 11.0f, 4.5f}, kDreadHull));
    for (const float x : {-19.0f, 0.0f, 19.0f}) {
        ship.append(cylinder({x, 0.0f}, -113.0f, -106.0f, 7.2f, 8.0f, kDreadHull));
        ship.append(cylinder({x, 0.0f}, -113.4f, -112.8f, 6.0f, 6.0f, kDreadEngine));
        ship.append(cylinder({x, 0.0f}, -113.1f, -112.9f, 7.6f, 7.6f, kDreadDark, 24));  // nozzle rim
    }
    for (const float x : {-30.0f, 30.0f})
        for (const float y : {-5.0f, 5.0f}) {
            ship.append(cylinder({x, y}, -110.5f, -106.0f, 3.2f, 3.6f, kDreadHull));
            ship.append(cylinder({x, y}, -110.8f, -110.3f, 2.6f, 2.6f, kDreadEngine));
        }
}

// ---------------------------------------------------------------------------------------------------------
// Detail layers

// Dense "city" of blocks on the decks: districts of different density from noise, mostly low blocks with
// some towers, long rows in industrial districts, some stacked. Blocks only go where all four footprint
// corners sit on the same level, so terraces stay clean.
void addCity(MeshData& ship, Random& random) {
    constexpr float kCell = 1.4f;
    for (float z = -97.0f; z < 90.0f; z += kCell) {
        for (float x = -54.0f; x < 54.0f; x += kCell) {
            const float cx = x + kCell * 0.5f, cz = z + kCell * 0.5f;
            if (std::abs(cx) > 0.9f * hullHalfWidth(cz) - 0.8f || inTowerFootprint(cx, cz)) continue;

            const float district = cpunoise::fbm(glm::vec3(cx * 0.045f, 0.0f, cz * 0.045f), 4242u, 3);
            const float density = 0.12f + 0.75f * glm::smoothstep(0.4f, 0.62f, district);
            if (random() > density) continue;

            float sx = kCell * random.range(0.45f, 0.95f);
            float sz = kCell * random.range(0.45f, 0.95f);
            if (district > 0.58f && random() < 0.25f) sz *= random.range(2.0f, 3.5f);  // long industrial rows
            float height = 0.25f + 1.6f * std::pow(random(), 2.2f) * (0.6f + district);
            if (random() < 0.03f) {  // occasional tower
                height = random.range(2.5f, 6.0f);
                sx *= 0.6f;
                sz *= 0.6f;
            }
            const float px = cx + random.range(-0.5f, 0.5f) * (kCell - sx);
            const float pz = cz + random.range(-0.5f, 0.5f) * (kCell - sz);

            float low = 1e9f, high = -1e9f;
            for (const float dx : {-0.5f, 0.5f})
                for (const float dz : {-0.5f, 0.5f}) {
                    const float h = topSurface(px + dx * sx, pz + dz * sz);
                    if (std::isnan(h)) {
                        low = -1e9f;
                        break;
                    }
                    low = std::min(low, h);
                    high = std::max(high, h);
                }
            if (low < -1e8f || high - low > 0.5f) continue;

            const float base = low - 0.15f;
            ship.append(box({px, base + height * 0.5f, pz}, {sx * 0.5f, height * 0.5f, sz * 0.5f}, kDreadHull, 0.4f));
            if (height < 2.0f && random() < 0.3f) {  // smaller block stacked on top
                const float topHeight = random.range(0.3f, 0.9f);
                const float shrink = random.range(0.55f, 0.8f);
                ship.append(box({px, base + height + topHeight * 0.5f, pz}, {sx * 0.5f * shrink, topHeight * 0.5f, sz * 0.5f * shrink},
                                kDreadHull, 0.65f));
            }
        }
    }
}

// Armored rim segments along the hull's top edges and every tier edge: crisp, readable silhouettes.
// Segment lengths, heights and the odd gap vary so the edge does not read as a regular comb.
void addRims(MeshData& ship, Random& random) {
    for (const float side : {-1.0f, 1.0f})
        for (float z = -98.0f; z < 92.0f;) {
            const float length = random.range(1.2f, 4.0f);
            const float w = hullHalfWidth(z);
            const float h = ridgeHeight(z);
            if (w < 2.0f) break;
            if (random() > 0.08f)
                ship.append(box({side * (w - 0.45f), 0.5f * h + 0.15f, z + length * 0.5f},
                                {0.45f, random.range(0.25f, 0.5f), length * 0.45f}, kDreadHull, 0.6f));
            z += length;
        }
    for (const Tier& tier : kTiers)
        for (const float side : {-1.0f, 1.0f})
            for (float z = tier.rear + 1.0f; z < tier.front - 2.0f;) {
                const float length = random.range(1.0f, 3.5f);
                const float w = tierHalfWidth(tier, z);
                if (w < 2.5f) break;
                if (random() > 0.08f)
                    ship.append(box({side * (w - 0.35f), tierTop(tier, z) + 0.2f, z + length * 0.5f},
                                    {0.35f, random.range(0.2f, 0.4f), length * 0.45f}, kDreadHull, 0.6f));
                z += length;
            }
}

// Turret: low base, dome, twin barrels pointing forward.
void addTurret(MeshData& ship, const glm::vec3& at, float scale) {
    ship.append(box(at + glm::vec3(0.0f, 0.25f, 0.0f) * scale, glm::vec3(0.9f, 0.25f, 0.9f) * scale, kDreadHull, 0.5f));
    ship.append(ellipsoid(at + glm::vec3(0.0f, 0.5f, 0.0f) * scale, glm::vec3(0.8f, 0.55f, 0.8f) * scale, kDreadDome, 1));
    for (const float side : {-0.25f, 0.25f})
        ship.append(box(at + glm::vec3(side, 0.6f, 1.2f) * scale, glm::vec3(0.08f, 0.08f, 1.0f) * scale, kDreadDark));
}

void addTurrets(MeshData& ship) {
    for (float z = -70.0f; z <= 50.0f; z += 10.0f) {
        const float x = 0.8f * hullHalfWidth(z);
        for (const float side : {-1.0f, 1.0f}) {
            const float y = topSurface(side * x, z);
            if (!std::isnan(y)) addTurret(ship, {side * x, y - 0.1f, z}, 1.2f);
        }
    }
    for (const Tier& tier : kTiers)
        for (float z = tier.rear + 8.0f; z < tier.front - 10.0f; z += 14.0f) {
            const float x = tierHalfWidth(tier, z) - 1.6f;
            if (x < 3.0f) break;
            for (const float side : {-1.0f, 1.0f}) addTurret(ship, {side * x, tierTop(tier, z) - 0.05f, z}, 0.9f);
        }
}

// Rows of lit windows (with dark runs) and hangar openings in the hull's side walls, windows on tier walls.
void addWalls(MeshData& ship) {
    auto windowRun = [](float z, float row, std::uint32_t seed) {
        return cpunoise::fbm(glm::vec3(z * 0.12f, row * 3.1f, 0.0f), seed, 2) > 0.5f;
    };
    for (float z = -98.0f; z < 85.0f; z += 0.9f) {
        const float w = hullHalfWidth(z);
        const float h = ridgeHeight(z);
        for (float y = -0.5f * h + 0.6f; y < 0.5f * h - 0.4f; y += 0.8f)
            if (windowRun(z, y, 77u))
                for (const float side : {-1.0f, 1.0f})
                    ship.append(box({side * (w + 0.02f), y, z}, {0.12f, 0.12f, 0.28f}, kDreadWindow));
    }
    for (float z = -80.0f; z <= 40.0f; z += 24.0f) {
        const float w = hullHalfWidth(z);
        const float h = ridgeHeight(z);
        for (const float side : {-1.0f, 1.0f}) {
            ship.append(box({side * w, -0.05f * h, z}, {0.6f, 0.33f * h, 3.0f}, kDreadDark));
            ship.append(box({side * (w + 0.05f), 0.28f * h, z}, {0.5f, 0.06f, 2.6f}, kDreadWindow));
        }
    }
    for (const Tier& tier : kTiers)
        for (float z = tier.rear + 1.0f; z < tier.front - 3.0f; z += 0.9f) {
            const float w = tierHalfWidth(tier, z);
            for (const float y : {-1.2f, 0.0f, 1.2f})
                if (windowRun(z, y + tier.lift, 91u))
                    for (const float side : {-1.0f, 1.0f})
                        ship.append(box({side * (w + 0.02f), tierCenter(tier, z) + y, z}, {0.12f, 0.12f, 0.28f}, kDreadWindow));
        }
}

bool inReactorBulb(float x, float z) { return std::abs(x) < 15.0f && z > -66.0f && z < -24.0f; }
bool inDockingBay(float x, float z) { return std::abs(x) < 10.5f && z > -10.0f && z < 20.0f; }

// Underside: irregular armor plates following the faceted belly (random spacing, size, thickness, gaps),
// segmented keel ribs, a sparse "city" of fittings hanging below, and a lit docking bay.
void addUnderside(MeshData& ship, Random& random) {
    for (float z = -96.0f; z < 85.0f; z += random.range(5.0f, 11.0f)) {
        const float w = hullHalfWidth(z);
        for (float x = -0.86f * w + random.range(0.0f, 2.0f); x < 0.86f * w - 1.0f; x += random.range(2.6f, 5.0f)) {
            if (inReactorBulb(x, z) || inDockingBay(x, z) || random() < 0.25f) continue;
            const glm::vec3 half(random.range(0.9f, 2.2f), random.range(0.12f, 0.35f), random.range(1.5f, 4.5f));
            ship.append(box({x, bottomSurface(x, z) - half.y + 0.05f, z}, half, kDreadHull));
            if (random() < 0.15f)  // second, smaller plate on top of some
                ship.append(box({x, bottomSurface(x, z) - 2.0f * half.y - 0.08f, z}, half * glm::vec3(0.6f, 0.5f, 0.6f), kDreadHull));
        }
    }
    for (const float x : {-24.0f, -12.0f, 12.0f, 24.0f})  // keel ribs, in segments that follow the belly slope
        for (float z = -95.0f; z < 70.0f; z += 10.0f) {
            if (std::abs(x) > 0.8f * hullHalfWidth(z) || inReactorBulb(x, z) || inDockingBay(x, z)) continue;
            ship.append(box({x, bottomSurface(x, z + 4.5f) - 0.35f, z + 4.5f}, {0.5f, 0.45f, 4.6f}, kDreadHull));
        }

    constexpr float kCell = 2.2f;  // hanging fittings
    for (float z = -97.0f; z < 80.0f; z += kCell)
        for (float x = -50.0f; x < 50.0f; x += kCell) {
            const float cx = x + random.range(0.2f, kCell - 0.2f), cz = z + random.range(0.2f, kCell - 0.2f);
            if (std::abs(cx) > 0.85f * hullHalfWidth(cz) - 1.0f || inReactorBulb(cx, cz) || inDockingBay(cx, cz)) continue;
            const float district = cpunoise::fbm(glm::vec3(cx * 0.06f, 5.0f, cz * 0.06f), 777u, 3);
            if (random() > 0.05f + 0.4f * glm::smoothstep(0.45f, 0.65f, district)) continue;
            const glm::vec3 half(random.range(0.3f, 0.9f), random.range(0.15f, 0.6f), random.range(0.3f, 1.2f));
            const float top = std::max(bottomSurface(cx - half.x, cz), bottomSurface(cx + half.x, cz));
            ship.append(box({cx, top + 0.1f - half.y, cz}, half, kDreadHull, 0.45f, true));
        }

    const float bay = bottomSurface(0.0f, 5.0f);
    ship.append(box({0.0f, bay - 0.15f, 5.0f}, {9.0f, 0.4f, 14.0f}, kDreadDark));
    for (const float side : {-1.0f, 1.0f})
        ship.append(box({side * 8.6f, bay - 0.55f, 5.0f}, {0.15f, 0.06f, 13.5f}, kDreadWindow));
}

// Command tower: neck, bridge with a lit window band and an upper level, twin domes on pylons, masts.
void addTower(MeshData& ship) {
    const float deck = tierTop(kTiers[2], -96.0f);
    ship.append(box({0.0f, deck + 5.0f, -83.0f}, {3.5f, 6.0f, 5.0f}, kDreadHull, 0.5f));
    for (const float side : {-1.0f, 1.0f})
        for (float y = deck + 1.5f; y < deck + 9.0f; y += 2.2f)
            ship.append(box({side * 3.7f, y, -83.0f}, {0.3f, 0.6f, 3.5f}, kDreadHull));
    ship.append(box({0.0f, deck + 11.5f, -83.0f}, {17.0f, 2.3f, 4.0f}, kDreadHull));
    ship.append(box({0.0f, deck + 11.6f, -78.95f}, {15.5f, 0.35f, 0.1f}, kDreadWindow));  // bridge windows
    ship.append(box({0.0f, deck + 14.3f, -83.6f}, {11.0f, 0.6f, 3.0f}, kDreadHull));      // upper bridge level
    for (const float x : {-13.0f, 13.0f}) {
        ship.append(box({x, deck + 13.4f, -83.5f}, {1.0f, 1.0f, 1.0f}, kDreadHull));       // dome pylon
        ship.append(ellipsoid({x, deck + 15.2f, -83.5f}, glm::vec3(2.6f), kDreadDome));
    }
    ship.append(box({0.0f, deck + 17.5f, -86.0f}, {1.2f, 2.6f, 1.0f}, kDreadHull));         // comms mast
    for (const float x : {-1.5f, 0.0f, 1.5f})
        ship.append(box({x, deck + 20.5f, -86.0f}, {0.12f, 1.8f + std::abs(x) * 0.4f, 0.12f}, kDreadHull));
}

// Engine block: radiator fins on top and vents on the sides.
void addEngineDetail(MeshData& ship) {
    for (float x = -34.0f; x <= 34.0f; x += 2.6f)
        ship.append(box({x, 11.8f, -103.5f}, {0.2f, 0.8f, 3.8f}, kDreadHull, 0.5f));
    for (const float side : {-1.0f, 1.0f})
        for (float y = -8.0f; y <= 8.0f; y += 2.0f) ship.append(box({side * 37.2f, y, -103.5f}, {0.3f, 0.35f, 3.6f}, kDreadDark));
}

}  // namespace

MeshData makeDreadnought() {
    MeshData ship;
    Random random(20261017u);
    addBigForms(ship);
    addCity(ship, random);
    addRims(ship, random);
    addTurrets(ship);
    addWalls(ship);
    addUnderside(ship, random);
    addTower(ship);
    addEngineDetail(ship);
    return ship;
}

}  // namespace demo
