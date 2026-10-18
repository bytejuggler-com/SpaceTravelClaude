#pragma once

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>

// Value noise on the CPU, for shaping procedural meshes at load time (seeded, deterministic).
namespace demo::cpunoise {

inline float hash(int x, int y, int z, std::uint32_t seed) {
    std::uint32_t h = seed;
    h ^= static_cast<std::uint32_t>(x) * 73856093u;
    h ^= static_cast<std::uint32_t>(y) * 19349663u;
    h ^= static_cast<std::uint32_t>(z) * 83492791u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return static_cast<float>(h & 0xFFFFFFu) / 16777215.0f;
}

inline float valueNoise(const glm::vec3& p, std::uint32_t seed) {
    const glm::vec3 cell = glm::floor(p);
    const glm::vec3 f = p - cell;
    const glm::vec3 u = f * f * (3.0f - 2.0f * f);
    const int x = static_cast<int>(cell.x), y = static_cast<int>(cell.y), z = static_cast<int>(cell.z);
    auto corner = [&](int dx, int dy, int dz) { return hash(x + dx, y + dy, z + dz, seed); };
    const float x00 = glm::mix(corner(0, 0, 0), corner(1, 0, 0), u.x);
    const float x10 = glm::mix(corner(0, 1, 0), corner(1, 1, 0), u.x);
    const float x01 = glm::mix(corner(0, 0, 1), corner(1, 0, 1), u.x);
    const float x11 = glm::mix(corner(0, 1, 1), corner(1, 1, 1), u.x);
    return glm::mix(glm::mix(x00, x10, u.y), glm::mix(x01, x11, u.y), u.z);
}

// Fractal noise in roughly [0, 1].
inline float fbm(glm::vec3 p, std::uint32_t seed, int octaves) {
    float value = 0.0f, amplitude = 0.5f, total = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        value += amplitude * valueNoise(p, seed + static_cast<std::uint32_t>(i) * 101u);
        total += amplitude;
        p *= 2.03f;
        amplitude *= 0.5f;
    }
    return value / total;
}

}  // namespace demo::cpunoise
