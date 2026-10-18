#pragma once

#include "core/Shader.h"

#include <glm/glm.hpp>

#include <cmath>

namespace demo {

// Pinhole camera for raymarched scenes; matches shaders/common/camera.glsl.
struct Camera {
    glm::vec3 position{0.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    float focal = 1.0f;  // distance to the image plane, in screen heights (larger = narrower field of view)

    static Camera lookAt(const glm::vec3& position, const glm::vec3& target, float focal, float roll = 0.0f) {
        Camera camera;
        camera.position = position;
        camera.focal = focal;
        camera.forward = glm::normalize(target - position);
        const glm::vec3 right = glm::normalize(glm::cross(camera.forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        const glm::vec3 up = glm::cross(right, camera.forward);
        camera.right = right * std::cos(roll) + up * std::sin(roll);
        camera.up = up * std::cos(roll) - right * std::sin(roll);
        return camera;
    }

    void apply(ShaderProgram& program) const {
        program.set("uCamPos", position);
        program.set("uCamRight", right);
        program.set("uCamUp", up);
        program.set("uCamForward", forward);
        program.set("uFocal", focal);
    }
};

}  // namespace demo
