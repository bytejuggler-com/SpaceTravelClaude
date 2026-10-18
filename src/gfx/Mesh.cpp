#include "gfx/Mesh.h"

#include <algorithm>
#include <utility>

namespace demo {

Mesh::Mesh(const MeshData& data) : m_indexCount(static_cast<GLsizei>(data.indices.size())) {
    glCreateBuffers(1, &m_vertexBuffer);
    glNamedBufferStorage(m_vertexBuffer, static_cast<GLsizeiptr>(data.vertices.size() * sizeof(Vertex)),
                         data.vertices.data(), 0);
    glCreateBuffers(1, &m_indexBuffer);
    glNamedBufferStorage(m_indexBuffer, static_cast<GLsizeiptr>(data.indices.size() * sizeof(std::uint32_t)),
                         data.indices.data(), 0);

    glCreateVertexArrays(1, &m_vao);
    glVertexArrayVertexBuffer(m_vao, 0, m_vertexBuffer, 0, sizeof(Vertex));
    glVertexArrayElementBuffer(m_vao, m_indexBuffer);

    const struct {
        GLuint location;
        GLint components;
        GLuint offset;
    } attributes[] = {
        {0, 3, static_cast<GLuint>(offsetof(Vertex, position))},
        {1, 3, static_cast<GLuint>(offsetof(Vertex, normal))},
        {2, 1, static_cast<GLuint>(offsetof(Vertex, material))},
        {3, 1, static_cast<GLuint>(offsetof(Vertex, occlusion))},
    };
    for (const auto& attribute : attributes) {
        glEnableVertexArrayAttrib(m_vao, attribute.location);
        glVertexArrayAttribFormat(m_vao, attribute.location, attribute.components, GL_FLOAT, GL_FALSE, attribute.offset);
        glVertexArrayAttribBinding(m_vao, attribute.location, 0);
    }
}

Mesh::~Mesh() { destroy(); }

Mesh::Mesh(Mesh&& other) noexcept
    : m_vao(std::exchange(other.m_vao, 0u)),
      m_vertexBuffer(std::exchange(other.m_vertexBuffer, 0u)),
      m_indexBuffer(std::exchange(other.m_indexBuffer, 0u)),
      m_indexCount(std::exchange(other.m_indexCount, 0)) {}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        destroy();
        m_vao = std::exchange(other.m_vao, 0u);
        m_vertexBuffer = std::exchange(other.m_vertexBuffer, 0u);
        m_indexBuffer = std::exchange(other.m_indexBuffer, 0u);
        m_indexCount = std::exchange(other.m_indexCount, 0);
    }
    return *this;
}

void Mesh::draw(GLsizei instanceCount, GLuint baseInstance) const {
    if (instanceCount <= 0 || m_indexCount == 0) return;
    glBindVertexArray(m_vao);
    glDrawElementsInstancedBaseInstance(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr, instanceCount,
                                        baseInstance);
}

void Mesh::destroy() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vertexBuffer) glDeleteBuffers(1, &m_vertexBuffer);
    if (m_indexBuffer) glDeleteBuffers(1, &m_indexBuffer);
    m_vao = m_vertexBuffer = m_indexBuffer = 0;
    m_indexCount = 0;
}

StorageBuffer::~StorageBuffer() {
    if (m_buffer) glDeleteBuffers(1, &m_buffer);
}

void StorageBuffer::upload(const void* data, size_t bytes) {
    if (!m_buffer) glCreateBuffers(1, &m_buffer);
    if (bytes > m_capacity) m_capacity = std::max(bytes, m_capacity * 2);
    // Orphan the previous storage so the driver never stalls on a buffer the GPU is still reading.
    glNamedBufferData(m_buffer, static_cast<GLsizeiptr>(std::max<size_t>(m_capacity, 16)), nullptr, GL_STREAM_DRAW);
    if (bytes > 0) glNamedBufferSubData(m_buffer, 0, static_cast<GLsizeiptr>(bytes), data);
}

}  // namespace demo
