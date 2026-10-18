#include "core/GpuTimer.h"

namespace demo {

GpuTimer::GpuTimer() { glGenQueries(kQueryCount, m_queries); }

GpuTimer::~GpuTimer() { glDeleteQueries(kQueryCount, m_queries); }

void GpuTimer::begin() {
    GLuint query = m_queries[m_frame % kQueryCount];
    if (m_frame >= kQueryCount) {
        GLint available = 0;
        glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
        if (available) {
            GLuint64 ns = 0;
            glGetQueryObjectui64v(query, GL_QUERY_RESULT, &ns);
            m_lastMs = static_cast<double>(ns) / 1.0e6;
        }
    }
    glBeginQuery(GL_TIME_ELAPSED, query);
}

void GpuTimer::end() {
    glEndQuery(GL_TIME_ELAPSED);
    ++m_frame;
}

}  // namespace demo
