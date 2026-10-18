#pragma once

#include <string>

namespace demo {

// Saves the current back buffer (call before swapping) as a PNG.
bool saveScreenshot(const std::string& path, int width, int height);

}  // namespace demo
