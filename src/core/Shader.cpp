#include "core/Shader.h"

#include "core/Log.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

namespace demo {

namespace {

constexpr auto kPollInterval = std::chrono::milliseconds(250);

struct Preprocessed {
    std::string code;
    std::vector<std::string> files;  // source-string index -> path relative to root (for #line / error mapping)
    std::string error;
};

fs::file_time_type writeTimeOf(const fs::path& path) {
    std::error_code ec;
    const auto time = fs::last_write_time(path, ec);
    return ec ? fs::file_time_type::min() : time;
}

// Inlines #include "file" directives. Each included file gets its own GLSL source-string number
// via #line, so compiler errors can be mapped back to the right file.
bool expandIncludes(const fs::path& root, const std::string& relative, Preprocessed& out) {
    const std::string key = fs::path(relative).lexically_normal().generic_string();
    if (std::find(out.files.begin(), out.files.end(), key) != out.files.end()) return true;  // include once

    const int index = static_cast<int>(out.files.size());
    out.files.push_back(key);

    std::ifstream in(root / key);
    if (!in) {
        out.error += std::format("cannot open {}\n", key);
        return false;
    }

    if (index > 0) out.code += std::format("#line 1 {}\n", index);
    std::string line;
    int lineNumber = 0;
    while (std::getline(in, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        const size_t start = line.find_first_not_of(" \t");
        if (start != std::string::npos && line.compare(start, 8, "#include") == 0) {
            const size_t open = line.find('"', start);
            const size_t close = open == std::string::npos ? open : line.find('"', open + 1);
            if (close == std::string::npos) {
                out.error += std::format("{}:{}: malformed #include\n", key, lineNumber);
                return false;
            }
            if (!expandIncludes(root, line.substr(open + 1, close - open - 1), out)) return false;
            out.code += std::format("#line {} {}\n", lineNumber + 1, index);
            continue;
        }
        out.code += line;
        out.code += '\n';
    }
    return true;
}

// Rewrites "ERROR: 2:15:" (AMD/Intel) and "2(15) :" (NVIDIA) prefixes to "common/noise.glsl:15:".
std::string mapCompilerLog(const std::string& log, const std::vector<std::string>& files) {
    static const std::regex kLocation(R"(^((?:ERROR|WARNING): )?(\d+)[:(](\d+)\)?\s*:?\s*)");
    std::istringstream stream(log);
    std::string line, result;
    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, kLocation)) {
            const size_t fileIndex = std::stoul(match[2].str());
            if (fileIndex < files.size())
                line = std::format("{}{}:{}: {}", match[1].str(), files[fileIndex], match[3].str(), match.suffix().str());
        }
        if (!line.empty()) result += line + '\n';
    }
    return result;
}

std::string shaderInfoLog(GLuint shader) {
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<size_t>(std::max(length, 1)), '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    log.resize(std::strlen(log.c_str()));
    return log;
}

std::string programInfoLog(GLuint program) {
    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<size_t>(std::max(length, 1)), '\0');
    glGetProgramInfoLog(program, length, nullptr, log.data());
    log.resize(std::strlen(log.c_str()));
    return log;
}

}  // namespace

// ---------------------------------------------------------------------------------------------

ShaderProgram::ShaderProgram(std::string name, std::vector<ShaderStage> stages)
    : m_name(std::move(name)), m_stages(std::move(stages)) {}

ShaderProgram::~ShaderProgram() {
    if (m_program) glDeleteProgram(m_program);
}

GLint ShaderProgram::uniform(const char* name) {
    auto it = m_uniforms.find(name);
    if (it == m_uniforms.end()) it = m_uniforms.emplace(name, glGetUniformLocation(m_program, name)).first;
    return it->second;
}

void ShaderProgram::set(const char* name, const glm::mat3& m) {
    glProgramUniformMatrix3fv(m_program, uniform(name), 1, GL_FALSE, glm::value_ptr(m));
}

void ShaderProgram::set(const char* name, const glm::mat4& m) {
    glProgramUniformMatrix4fv(m_program, uniform(name), 1, GL_FALSE, glm::value_ptr(m));
}

bool ShaderProgram::build(const fs::path& root) {
    std::vector<GLuint> shaders;
    std::vector<Dependency> dependencies;
    std::string errors;

    for (const ShaderStage& stage : m_stages) {
        Preprocessed source;
        const bool expanded = expandIncludes(root, stage.file, source);
        // Track files even when preprocessing failed, so fixing/creating them triggers a rebuild.
        for (const std::string& file : source.files) {
            const fs::path path = root / file;
            dependencies.push_back({path, writeTimeOf(path)});
        }
        if (!expanded) {
            errors += source.error;
            continue;
        }

        const GLuint shader = glCreateShader(stage.type);
        const char* text = source.code.c_str();
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);

        GLint compiled = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            errors += mapCompilerLog(shaderInfoLog(shader), source.files);
            glDeleteShader(shader);
            continue;
        }
        shaders.push_back(shader);
    }
    m_dependencies = std::move(dependencies);

    if (!errors.empty()) {
        for (GLuint shader : shaders) glDeleteShader(shader);
        m_error = std::move(errors);
        return false;
    }

    const GLuint program = glCreateProgram();
    for (GLuint shader : shaders) glAttachShader(program, shader);
    glLinkProgram(program);
    for (GLuint shader : shaders) {
        glDetachShader(program, shader);
        glDeleteShader(shader);
    }

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        m_error = "link: " + programInfoLog(program);
        glDeleteProgram(program);
        return false;
    }

    if (m_program) glDeleteProgram(m_program);
    m_program = program;
    m_uniforms.clear();
    m_error.clear();
    return true;
}

bool ShaderProgram::dependenciesChanged() const {
    return std::any_of(m_dependencies.begin(), m_dependencies.end(),
                       [](const Dependency& dep) { return writeTimeOf(dep.path) != dep.writeTime; });
}

// ---------------------------------------------------------------------------------------------

ShaderLib::ShaderLib(fs::path root) : m_root(std::move(root)) {
    log::info("Shader root: {}", m_root.string());
}

ShaderProgram& ShaderLib::load(std::string name, std::vector<ShaderStage> stages) {
    auto& program = *m_programs.emplace_back(std::make_unique<ShaderProgram>(std::move(name), std::move(stages)));
    build(program, false);
    return program;
}

ShaderProgram& ShaderLib::loadFullscreen(std::string name, std::string fragmentFile) {
    return load(std::move(name), {{GL_VERTEX_SHADER, "common/fullscreen.vert"},
                                  {GL_FRAGMENT_SHADER, std::move(fragmentFile)}});
}

ShaderProgram& ShaderLib::loadCompute(std::string name, std::string computeFile) {
    return load(std::move(name), {{GL_COMPUTE_SHADER, std::move(computeFile)}});
}

void ShaderLib::pollHotReload() {
    const auto now = std::chrono::steady_clock::now();
    if (now - m_lastPoll < kPollInterval) return;
    m_lastPoll = now;

    for (auto& program : m_programs)
        if (program->dependenciesChanged()) build(*program, true);
}

void ShaderLib::reloadAll() {
    for (auto& program : m_programs) build(*program, true);
}

void ShaderLib::build(ShaderProgram& program, bool isReload) {
    if (program.build(m_root)) {
        log::info("Shader '{}' {}", program.name(), isReload ? "reloaded" : "built");
    } else {
        log::error("Shader '{}' failed{}:\n{}", program.name(),
                   program.valid() ? " (keeping previous version)" : "", program.lastError());
    }
}

}  // namespace demo
