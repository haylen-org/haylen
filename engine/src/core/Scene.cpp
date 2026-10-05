#include "haylen/core/Scene.hpp"

#include <algorithm>

namespace haylen::core {

const std::array<std::pair<std::string_view, Scene::State>, 9> Scene::kStateNames{{{"created", State::Created}, {"loading", State::Loading}, {"loaded", State::Loaded}, {"entering", State::Entering}, {"active", State::Active}, {"covered", State::Covered}, {"exiting", State::Exiting}, {"exited", State::Exited}, {"unloaded", State::Unloaded}}};

std::optional<Scene::State> Scene::stateFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kStateNames, name, &std::pair<std::string_view, State>::first);
    return found != kStateNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Scene::stateName(State value) noexcept {
    return std::ranges::find(kStateNames, value, &std::pair<std::string_view, State>::second)->first;
}

std::string Scene::getName() const {
    return "Scene";
}

void Scene::load(Engine&, SceneLoad&) {}

void Scene::enter(Engine&, const std::any&) {}

void Scene::enterTransitionFinished(Engine&) {}

void Scene::exitTransitionStarted(Engine&) {}

void Scene::exit(Engine&) {}

void Scene::unload(Engine&) {}

void Scene::pause(Engine&) {}

void Scene::resume(Engine&) {}

void Scene::paused(Engine&) {}

void Scene::unpaused(Engine&) {}

void Scene::event(Engine&, const platform::Event&) {}

void Scene::fixedUpdate(Engine&, float) {}

void Scene::update(Engine&, float) {}

void Scene::render(Engine&) {}

void Scene::renderUi(Engine&) {}

bool Scene::isTransparent() const {
    return false;
}

ProcessMode Scene::getProcessMode() const {
    return ProcessMode::Inherit;
}

SceneLoad::Progress Scene::getLoadProgress() const {
    if (loading) {
        return loading->getProgress();
    }
    return {.value = state == State::Created || state == State::Unloaded ? 0.0F : 1.0F};
}

} // namespace haylen::core
