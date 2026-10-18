#pragma once

#include "core/Gl.h"

#include <glm/glm.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace demo {

struct ShaderStage {
    GLenum type;       // GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_COMPUTE_SHADER, ...
    std::string file;  // relative to the shader root
};

// A linked GLSL program. Rebuilds keep the previous working program until the new one compiles,
// so a typo during hot-reload never blanks the screen.
class ShaderProgram {
public:
    ShaderProgram(std::string name, std::vector<ShaderStage> stages);
    ~ShaderProgram();
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    const std::string& name() const { return m_name; }
    GLuint id() const { return m_program; }
    bool valid() const { return m_program != 0; }
    const std::string& lastError() const { return m_error; }  // empty when the last build succeeded

    void use() const { glUseProgram(m_program); }
    GLint uniform(const char* name);  // cached; -1 if absent or optimized out

    void set(const char* name, float v) { glProgramUniform1f(m_program, uniform(name), v); }
    void set(const char* name, int v) { glProgramUniform1i(m_program, uniform(name), v); }
    void set(const char* name, const glm::vec2& v) { glProgramUniform2f(m_program, uniform(name), v.x, v.y); }
    void set(const char* name, const glm::vec3& v) { glProgramUniform3f(m_program, uniform(name), v.x, v.y, v.z); }
    void set(const char* name, const glm::vec4& v) { glProgramUniform4f(m_program, uniform(name), v.x, v.y, v.z, v.w); }
    void set(const char* name, const glm::mat3& m);
    void set(const char* name, const glm::mat4& m);

private:
    friend class ShaderLib;

    struct Dependency {
        std::filesystem::path path;
        std::filesystem::file_time_type writeTime;
    };

    bool build(const std::filesystem::path& root);
    bool dependenciesChanged() const;

    std::string m_name;
    std::vector<ShaderStage> m_stages;
    GLuint m_program = 0;
    std::string m_error;
    std::vector<Dependency> m_dependencies;  // every file read by the last build, includes too
    std::unordered_map<std::string, GLint> m_uniforms;
};

// Owns all programs, resolves #include "path" (relative to the shader root, include-once)
// and, in dev builds, rebuilds programs whose source files changed on disk.
class ShaderLib {
public:
    explicit ShaderLib(std::filesystem::path root);

    // Returned references stay valid for the lifetime of the ShaderLib.
    ShaderProgram& load(std::string name, std::vector<ShaderStage> stages);
    ShaderProgram& loadFullscreen(std::string name, std::string fragmentFile);
    ShaderProgram& loadCompute(std::string name, std::string computeFile);

    void pollHotReload();  // cheap: checks file times at most a few times per second
    void reloadAll();

    const std::filesystem::path& root() const { return m_root; }
    const std::vector<std::unique_ptr<ShaderProgram>>& programs() const { return m_programs; }

private:
    void build(ShaderProgram& program, bool isReload);

    std::filesystem::path m_root;
    std::vector<std::unique_ptr<ShaderProgram>> m_programs;
    std::chrono::steady_clock::time_point m_lastPoll{};
};

}  // namespace demo
