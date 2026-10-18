#include "core/Log.h"

#include <cstdio>

namespace demo::log {

namespace {
constexpr size_t kMaxRecent = 256;
std::deque<Entry> g_recent;
}  // namespace

void write(Level level, std::string_view text) {
    static const char* const kTags[] = {"INFO ", "WARN ", "ERROR"};
    std::FILE* out = level == Level::Info ? stdout : stderr;
    std::fprintf(out, "[%s] %.*s\n", kTags[static_cast<int>(level)], static_cast<int>(text.size()), text.data());
    std::fflush(out);

    g_recent.push_back({level, std::string(text)});
    if (g_recent.size() > kMaxRecent) g_recent.pop_front();
}

const std::deque<Entry>& recent() { return g_recent; }

}  // namespace demo::log
