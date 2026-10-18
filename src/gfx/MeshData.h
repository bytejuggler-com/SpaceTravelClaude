#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace demo {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    float material;          // small integer id interpreted by the fragment shader (hull, glass, engine, ...)
    float occlusion = 1.0f;  // baked ambient occlusion, 1 = open, lower = crevice (interpolated across faces)
};

// Triangle mesh on the CPU, built procedurally and then uploaded as a Mesh.
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    // Appends `other` transformed by `transform` (normals included); mirrored transforms flip the winding.
    void append(const MeshData& other, const glm::mat4& transform = glm::mat4(1.0f));
    void computeSmoothNormals();
    // Fixes winding so every face points away from the mesh centroid (correct for convex-ish parts).
    void orientOutward();
    // Unshared vertices with face normals: crisp hard-surface shading.
    MeshData flattened() const;
};

struct LoftRing {
    float z;                       // position along the loft axis
    glm::vec2 scale;               // half extents of the cross-section
    glm::vec2 offset{0.0f, 0.0f};  // cross-section center
};

// Unit sphere, subdivided icosahedron (subdivisions 0..5; 2 = 320 triangles, 4 = 5120).
MeshData makeIcosphere(int subdivisions);

// Extrudes a regular polygon (stretched to fill [-1, 1]^2, so 4 sides = a box) through rings along +z,
// with end caps. Faces point outward.
MeshData makeLoft(const std::vector<LoftRing>& rings, int sides, float material);

// Lumpy, cratered, slightly elongated rock with bounding radius ~1. Same seed gives the same shape at
// any subdivision level, so LODs match.
MeshData makeAsteroid(std::uint32_t seed, int subdivisions);

}  // namespace demo
