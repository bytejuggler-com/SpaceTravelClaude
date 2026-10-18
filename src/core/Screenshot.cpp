#include "core/Screenshot.h"

#include "core/Gl.h"
#include "core/Log.h"

#include <vector>

#pragma warning(push, 0)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#pragma warning(pop)

namespace demo {

bool saveScreenshot(const std::string& path, int width, int height) {
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    stbi_flip_vertically_on_write(1);
    if (!stbi_write_png(path.c_str(), width, height, 3, pixels.data(), width * 3)) {
        log::error("Failed to write screenshot {}", path);
        return false;
    }
    log::info("Saved screenshot {} ({}x{})", path, width, height);
    return true;
}

}  // namespace demo
