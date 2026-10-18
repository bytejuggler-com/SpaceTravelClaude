#include "scene/SceneRegistry.h"

namespace demo {

SceneRegistry& SceneRegistry::instance() {
    static SceneRegistry registry;  // function-local: safe to use from other translation units' static init
    return registry;
}

bool SceneRegistry::add(std::string name, Factory factory) {
    return m_factories.emplace(std::move(name), std::move(factory)).second;
}

std::unique_ptr<Scene> SceneRegistry::create(const std::string& name) const {
    const auto it = m_factories.find(name);
    return it == m_factories.end() ? nullptr : it->second();
}

std::vector<std::string> SceneRegistry::names() const {
    std::vector<std::string> result;
    for (const auto& [name, factory] : m_factories) result.push_back(name);
    return result;
}

}  // namespace demo
