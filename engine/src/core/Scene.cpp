#include "haylen/core/Scene.hpp"

namespace haylen::core {

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
