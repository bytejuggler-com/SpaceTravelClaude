#include "core/GlDebug.h"

#include "core/Gl.h"
#include "core/Log.h"

#include <cstring>
#include <string_view>

namespace demo {

namespace {

void APIENTRY onGlDebugMessage(GLenum /*source*/, GLenum type, GLuint /*id*/, GLenum severity, GLsizei length,
                               const GLchar* message, const void* /*user*/) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    std::string_view text(message, length > 0 ? static_cast<size_t>(length) : std::strlen(message));
    if (type == GL_DEBUG_TYPE_ERROR || severity == GL_DEBUG_SEVERITY_HIGH)
        log::error("GL: {}", text);
    else
        log::warn("GL: {}", text);
}

}  // namespace

void installGlDebugOutput() {
    GLint flags = 0;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (!(flags & GL_CONTEXT_FLAG_DEBUG_BIT)) {
        log::info("GL debug output disabled (no debug context)");
        return;
    }
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(onGlDebugMessage, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
    log::info("GL debug output enabled");
}

}  // namespace demo
