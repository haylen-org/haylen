#include "haylen/2d/particles/System.hpp"

#include <algorithm>
#include <stdexcept>

#include "haylen/core/JobSystem.hpp"

namespace haylen::particles2d {

System::System(const Effect& effect, std::uint64_t seed) {
    if (!effect.isComposite()) {
        parts.push_back({.emitter = std::make_shared<Emitter>(effect.config, seed)});
        return;
    }
    for (std::size_t index = 0; index < effect.parts.size(); ++index) {
        const Effect::Part& part = effect.parts[index];
        parts.push_back({.name = part.name, .offset = part.offset, .scale = part.scale, .emitter = std::make_shared<Emitter>(part.config, seed + index * 977U)});
    }
}

void System::place() {
    for (const Part& part : parts) {
        part.emitter->position = position + part.offset.rotated(rotation) * scale;
        part.emitter->scale = scale * part.scale;
        part.emitter->rotation = rotation;
    }
}

void System::update(float deltaSeconds) {
    place();
    for (const Part& part : parts) {
        part.emitter->update(deltaSeconds);
    }
}

void System::update(float deltaSeconds, core::JobSystem& jobs) {
    place();
    for (const Part& part : parts) {
        part.emitter->update(deltaSeconds, jobs);
    }
}

void System::draw(graphics2d::Renderer& renderer) const {
    for (const Part& part : parts) {
        part.emitter->draw(renderer);
    }
}

void System::restart() {
    place();
    for (const Part& part : parts) {
        part.emitter->restart();
    }
}

void System::clear() noexcept {
    for (const Part& part : parts) {
        part.emitter->clear();
    }
}

std::size_t System::getCount() const noexcept {
    std::size_t count = 0;
    for (const Part& part : parts) {
        count += part.emitter->getCount();
    }
    return count;
}

bool System::isAlive() const noexcept {
    return std::any_of(parts.begin(), parts.end(), [](const Part& part) { return part.emitter->isAlive(); });
}

bool System::isEmitting() const noexcept {
    return std::any_of(parts.begin(), parts.end(), [](const Part& part) { return part.emitter->emitting; });
}

void System::setEmitting(bool value) {
    for (const Part& part : parts) {
        part.emitter->emitting = value;
    }
}

const std::shared_ptr<Emitter>& System::getEmitter(std::size_t index) const {
    if (index >= parts.size()) {
        throw std::out_of_range("The particle system has no emitter at that index.");
    }
    return parts[index].emitter;
}

const std::string& System::getName(std::size_t index) const {
    if (index >= parts.size()) {
        throw std::out_of_range("The particle system has no emitter at that index.");
    }
    return parts[index].name;
}

std::shared_ptr<Emitter> System::findEmitter(std::string_view name) const {
    const auto found = std::find_if(parts.begin(), parts.end(), [name](const Part& part) { return part.name == name; });
    return found != parts.end() ? found->emitter : nullptr;
}

} // namespace haylen::particles2d
