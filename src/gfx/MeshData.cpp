#include "gfx/MeshData.h"

#include "gfx/CpuNoise.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <random>
#include <utility>

namespace demo {

void MeshData::append(const MeshData& other, const glm::mat4& transform) {
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
    const bool mirrored = glm::determinant(glm::mat3(transform)) < 0.0f;
    const auto base = static_cast<std::uint32_t>(vertices.size());

    for (const Vertex& v : other.vertices)
        vertices.push_back({glm::vec3(transform * glm::vec4(v.position, 1.0f)),
                            glm::normalize(normalMatrix * v.normal), v.material, v.occlusion});
    for (size_t i = 0; i + 2 < other.indices.size(); i += 3) {
        const std::uint32_t a = other.indices[i] + base, b = other.indices[i + 1] + base, c = other.indices[i + 2] + base;
        indices.insert(indices.end(), {a, mirrored ? c : b, mirrored ? b : c});
    }
}

void MeshData::computeSmoothNormals() {
    for (Vertex& v : vertices) v.normal = glm::vec3(0.0f);
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        Vertex& a = vertices[indices[i]];
        Vertex& b = vertices[indices[i + 1]];
        Vertex& c = vertices[indices[i + 2]];
        const glm::vec3 faceNormal = glm::cross(b.position - a.position, c.position - a.position);  // area-weighted
        a.normal += faceNormal;
        b.normal += faceNormal;
        c.normal += faceNormal;
    }
    for (Vertex& v : vertices) {
        const float length = glm::length(v.normal);
        v.normal = length > 0.0f ? v.normal / length : glm::vec3(0.0f, 1.0f, 0.0f);
    }
}

void MeshData::orientOutward() {
    glm::vec3 centroid(0.0f);
    for (const Vertex& v : vertices) centroid += v.position;
    centroid /= std::max<size_t>(vertices.size(), 1);

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const glm::vec3 a = vertices[indices[i]].position;
        const glm::vec3 b = vertices[indices[i + 1]].position;
        const glm::vec3 c = vertices[indices[i + 2]].position;
        const glm::vec3 faceNormal = glm::cross(b - a, c - a);
        if (glm::dot(faceNormal, (a + b + c) / 3.0f - centroid) < 0.0f) std::swap(indices[i + 1], indices[i + 2]);
    }
}

MeshData MeshData::flattened() const {
    MeshData flat;
    flat.vertices.reserve(indices.size());
    flat.indices.reserve(indices.size());
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const Vertex& a = vertices[indices[i]];
        const Vertex& b = vertices[indices[i + 1]];
        const Vertex& c = vertices[indices[i + 2]];
        const glm::vec3 cross = glm::cross(b.position - a.position, c.position - a.position);
        const float length = glm::length(cross);
        if (length < 1e-12f) continue;  // degenerate (e.g. collapsed nose ring)
        const glm::vec3 normal = cross / length;
        for (const Vertex* v : {&a, &b, &c}) {
            flat.indices.push_back(static_cast<std::uint32_t>(flat.vertices.size()));
            flat.vertices.push_back({v->position, normal, v->material, v->occlusion});
        }
    }
    return flat;
}

MeshData makeIcosphere(int subdivisions) {
    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
    std::vector<glm::vec3> positions = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t},
                                        {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
    for (glm::vec3& p : positions) p = glm::normalize(p);
    std::vector<std::uint32_t> faces = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4,
                                        11, 10, 2, 10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8,
                                        3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};

    for (int level = 0; level < subdivisions; ++level) {
        std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> midpoints;
        auto midpoint = [&](std::uint32_t a, std::uint32_t b) {
            const auto key = std::minmax(a, b);
            const auto it = midpoints.find(key);
            if (it != midpoints.end()) return it->second;
            positions.push_back(glm::normalize(positions[a] + positions[b]));
            const auto index = static_cast<std::uint32_t>(positions.size() - 1);
            midpoints.emplace(key, index);
            return index;
        };
        std::vector<std::uint32_t> next;
        next.reserve(faces.size() * 4);
        for (size_t i = 0; i < faces.size(); i += 3) {
            const std::uint32_t a = faces[i], b = faces[i + 1], c = faces[i + 2];
            const std::uint32_t ab = midpoint(a, b), bc = midpoint(b, c), ca = midpoint(c, a);
            next.insert(next.end(), {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca});
        }
        faces = std::move(next);
    }

    MeshData mesh;
    for (const glm::vec3& p : positions) mesh.vertices.push_back({p, p, 0.0f});
    mesh.indices = std::move(faces);
    mesh.orientOutward();
    return mesh;
}

MeshData makeLoft(const std::vector<LoftRing>& rings, int sides, float material) {
    // Cross-section polygon, rotated half a step (flat bottom) and stretched to fill [-1, 1]^2.
    std::vector<glm::vec2> polygon(sides);
    glm::vec2 extent(0.0f);
    for (int i = 0; i < sides; ++i) {
        const float angle = 2.0f * std::numbers::pi_v<float> * (static_cast<float>(i) + 0.5f) / static_cast<float>(sides);
        polygon[i] = glm::vec2(std::cos(angle), std::sin(angle));
        extent = glm::max(extent, glm::abs(polygon[i]));
    }
    for (glm::vec2& p : polygon) p /= extent;

    MeshData mesh;
    for (const LoftRing& ring : rings)
        for (const glm::vec2& p : polygon)
            mesh.vertices.push_back({glm::vec3(ring.offset + p * ring.scale, ring.z), glm::vec3(0.0f), material});

    const auto n = static_cast<std::uint32_t>(sides);
    for (std::uint32_t r = 0; r + 1 < rings.size(); ++r) {
        for (std::uint32_t i = 0; i < n; ++i) {
            const std::uint32_t a = r * n + i, b = r * n + (i + 1) % n;
            const std::uint32_t c = a + n, d = b + n;
            mesh.indices.insert(mesh.indices.end(), {a, b, d, a, d, c});
        }
    }

    // End caps as fans around a center vertex.
    for (const bool last : {false, true}) {
        const LoftRing& ring = last ? rings.back() : rings.front();
        const std::uint32_t first = last ? static_cast<std::uint32_t>(rings.size() - 1) * n : 0;
        const auto center = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({glm::vec3(ring.offset, ring.z), glm::vec3(0.0f), material});
        for (std::uint32_t i = 0; i < n; ++i) mesh.indices.insert(mesh.indices.end(), {center, first + i, first + (i + 1) % n});
    }

    mesh.orientOutward();
    mesh.computeSmoothNormals();
    return mesh;
}

MeshData makeAsteroid(std::uint32_t seed, int subdivisions) {
    MeshData mesh = makeIcosphere(subdivisions);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const glm::vec3 stretch(1.0f + 0.5f * unit(rng), 0.7f + 0.3f * unit(rng), 0.8f + 0.4f * unit(rng));

    struct Crater {
        glm::vec3 center;
        float radius;
        float depth;
    };
    std::vector<Crater> craters(6 + static_cast<int>(unit(rng) * 9.0f));
    for (Crater& crater : craters) {
        crater.center = glm::normalize(glm::vec3(unit(rng), unit(rng), unit(rng)) * 2.0f - 1.0f);
        crater.radius = 0.12f + 0.38f * unit(rng) * unit(rng);
        crater.depth = crater.radius * (0.25f + 0.25f * unit(rng));
    }

    float maxRadius = 0.0f;
    for (Vertex& v : mesh.vertices) {
        const glm::vec3 d = glm::normalize(v.position);
        float h = (cpunoise::fbm(d * 1.4f, seed, 4) - 0.5f) * 0.7f + (cpunoise::fbm(d * 5.0f, seed + 7u, 3) - 0.5f) * 0.15f;
        for (const Crater& crater : craters) {
            const float x = std::acos(std::clamp(glm::dot(d, crater.center), -1.0f, 1.0f)) / crater.radius;
            if (x < 1.0f) h -= crater.depth * (1.0f - x * x);                       // bowl
            if (x < 1.7f) {
                const float rim = (x - 1.05f) / 0.2f;
                h += crater.depth * 0.35f * std::exp(-rim * rim);                  // raised rim
            }
        }
        v.position = d * (1.0f + h) * stretch;
        maxRadius = std::max(maxRadius, glm::length(v.position));
    }
    for (Vertex& v : mesh.vertices) v.position /= maxRadius;
    mesh.computeSmoothNormals();
    return mesh;
}

}  // namespace demo
