#pragma once

#include <deque>
#include <format>
#include <string>
#include <string_view>

namespace demo::log {

enum class Level { Info, Warn, Error };

struct Entry {
    Level level;
    std::string text;
};

void write(Level level, std::string_view text);

// Most recent log lines, for the dev overlay. Main thread only.
const std::deque<Entry>& recent();

template <class... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Info, std::format(fmt, std::forward<Args>(args)...));
}

template <class... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Warn, std::format(fmt, std::forward<Args>(args)...));
}

template <class... Args>
void error(std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Error, std::format(fmt, std::forward<Args>(args)...));
}

}  // namespace demo::log
