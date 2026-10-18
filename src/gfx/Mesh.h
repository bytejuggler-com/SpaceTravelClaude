#pragma once

#include "core/Gl.h"
#include "gfx/MeshData.h"

#include <cstddef>

namespace demo {

// Mesh uploaded to the GPU (vertex layout: location 0 position, 1 normal, 2 material, 3 occlusion).
// Pair with shaders/common/mesh.vert, which reads per-instance data from a storage buffer.
class Mesh {
public:
    Mesh() = default;
    explicit Mesh(const MeshData& data);
    ~Mesh();
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Draws `instanceCount` instances starting at `baseInstance` (visible in GLSL as gl_BaseInstance).
    void draw(GLsizei instanceCount, GLuint baseInstance = 0) const;
    GLsizei triangleCount() const { return m_indexCount / 3; }

private:
    void destroy();

    GLuint m_vao = 0;
    GLuint m_vertexBuffer = 0;
    GLuint m_indexBuffer = 0;
    GLsizei m_indexCount = 0;
};

// Shader storage buffer that is re-filled every frame (e.g. per-instance transforms).
class StorageBuffer {
public:
    StorageBuffer() = default;
    ~StorageBuffer();
    StorageBuffer(const StorageBuffer&) = delete;
    StorageBuffer& operator=(const StorageBuffer&) = delete;

    void upload(const void* data, size_t bytes);
    void bind(GLuint index) const { glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, m_buffer); }

private:
    GLuint m_buffer = 0;
    size_t m_capacity = 0;
};

}  // namespace demo
