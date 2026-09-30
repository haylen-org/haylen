#include "haylen/core/SceneLoad.hpp"

#include <numeric>
#include <stdexcept>
#include <utility>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"

namespace haylen::core {

SceneLoad::Deferral::~Deferral() {
    fail(lua::Error("The scene load was dropped before it finished."));
}

SceneLoad::Deferral::Deferral(Deferral&& other) noexcept : load(std::exchange(other.load, {})) {}

SceneLoad::Deferral& SceneLoad::Deferral::operator=(Deferral&& other) noexcept {
    if (this != &other) {
        fail(lua::Error("The scene load was dropped before it finished."));
        load = std::exchange(other.load, {});
    }
    return *this;
}

void SceneLoad::Deferral::complete() {
    if (const std::shared_ptr<SceneLoad> target = std::exchange(load, {}).lock()) {
        target->release();
    }
}

void SceneLoad::Deferral::fail(const lua::Error& failure) {
    if (const std::shared_ptr<SceneLoad> target = std::exchange(load, {}).lock()) {
        target->fail(failure);
    }
}

SceneLoad::SceneLoad(Engine& owner, std::any value) : engine(owner), params(std::move(value)) {}

void SceneLoad::setProgress(float value, std::string text) {
    if (!(value >= 0.0F && value <= 1.0F)) {
        throw std::invalid_argument("A load progress runs from 0 to 1.");
    }
    if (isOver()) {
        return;
    }
    ownProgress = value;
    message = std::move(text);
}

SceneLoad::Progress SceneLoad::getProgress() const {
    if (isFinished()) {
        return {.value = 1.0F, .message = message};
    }
    const float sum = std::accumulate(groups.begin(), groups.end(), ownProgress.value_or(0.0F));
    const std::size_t parts = groups.size() + (ownProgress ? 1 : 0);
    return {.value = parts == 0 ? 0.0F : sum / static_cast<float>(parts), .message = message};
}

SceneLoad::Deferral SceneLoad::defer() {
    if (isOver()) {
        throw std::logic_error("The scene load is over.");
    }
    ++pending;
    return Deferral(weak_from_this());
}

void SceneLoad::preload(std::string_view group, PreloadCompletion completion) {
    auto deferral = std::make_shared<Deferral>(defer());
    const std::size_t part = groups.size();
    groups.push_back(0.0F);

    // The asset manager may drop its listeners, when the group is unloaded or the engine shuts down, and the deferral they hold then fails the load.
    // clang-format off
    try {
        engine.getAssets().preload(group, [weak = weak_from_this(), part](float fraction) {
            if (const std::shared_ptr<SceneLoad> load = weak.lock(); load && !load->isOver()) {
                load->groups[part] = fraction;
            }
        }, [deferral, name = std::string(group), completion = std::move(completion)](std::vector<std::string> failures) {
            std::optional<lua::Error> failure;
            if (!failures.empty()) {
                std::string text = "The asset group \"" + name + "\" could not load " + failures.front();
                for (std::size_t index = 1; index < failures.size(); ++index) {
                    text += ", " + failures[index];
                }
                failure.emplace(text + ".");
                deferral->fail(*failure);
            } else {
                deferral->complete();
            }
            if (completion) {
                completion(failure);
            }
        });
    } catch (...) {
        // A group the manager refuses never started, so the load lets go of it and the reason of the manager reaches the scene instead of a dropped deferral.
        groups.pop_back();
        deferral->complete();
        throw;
    }
    // clang-format on
}

void SceneLoad::release() {
    if (isOver()) {
        return;
    }
    --pending;
}

void SceneLoad::fail(const lua::Error& failure) {
    if (isOver()) {
        return;
    }
    error = failure;
}

void SceneLoad::cancel() noexcept {
    cancelled = true;
}

} // namespace haylen::core
