#pragma once

#include <glm/glm.hpp>

#include <array>

namespace demo {

// View-frustum planes extracted from a view-projection matrix (Gribb/Hartmann), for culling bounding spheres.
struct Frustum {
    std::array<glm::vec4, 6> planes{};

    static Frustum fromMatrix(const glm::mat4& m) {
        auto row = [&](int i) { return glm::vec4(m[0][i], m[1][i], m[2][i], m[3][i]); };
        Frustum f;
        f.planes = {row(3) + row(0), row(3) - row(0), row(3) + row(1),
                    row(3) - row(1), row(3) + row(2), row(3) - row(2)};
        for (glm::vec4& plane : f.planes) plane /= glm::length(glm::vec3(plane));
        return f;
    }

    bool intersectsSphere(const glm::vec3& center, float radius) const {
        for (const glm::vec4& plane : planes)
            if (glm::dot(glm::vec3(plane), center) + plane.w < -radius) return false;
        return true;
    }
};

}  // namespace demo
