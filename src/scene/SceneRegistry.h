#pragma once

#include "scene/Scene.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace demo {

// Name -> factory map filled at static-initialization time by REGISTER_SCENE.
// Scene sources are compiled straight into the executable (not a static library), so the
// registration objects are never dropped by the linker.
class SceneRegistry {
public:
    using Factory = std::function<std::unique_ptr<Scene>()>;

    static SceneRegistry& instance();

    bool add(std::string name, Factory factory);
    std::unique_ptr<Scene> create(const std::string& name) const;  // nullptr if unknown
    std::vector<std::string> names() const;

private:
    std::map<std::string, Factory> m_factories;
};

}  // namespace demo

#define DEMO_SCENE_CONCAT_INNER(a, b) a##b
#define DEMO_SCENE_CONCAT(a, b) DEMO_SCENE_CONCAT_INNER(a, b)

// Usage (at namespace scope in the scene's .cpp): REGISTER_SCENE("corona", CoronaScene);
#define REGISTER_SCENE(NAME, TYPE)                                                     \
    static const bool DEMO_SCENE_CONCAT(kSceneRegistered_, __LINE__) =                 \
        ::demo::SceneRegistry::instance().add(NAME, []() -> std::unique_ptr<::demo::Scene> { \
            return std::make_unique<TYPE>();                                           \
        })
