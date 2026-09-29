#include "haylen/audio/Reverb.hpp"

#include <stdexcept>
#include <string>

#include "audio/Freeverb.hpp"

namespace haylen::audio {

Reverb::Reverb(const Settings& settings) : roomSize(settings.roomSize), damping(settings.damping), width(settings.width), wet(settings.wet), dry(settings.dry) {
    requireUnit(settings.roomSize, "room size");
    requireUnit(settings.damping, "damping");
    requireUnit(settings.width, "width");
    requireUnit(settings.wet, "wet level");
    requireUnit(settings.dry, "dry level");
}

Reverb::~Reverb() = default;

void Reverb::setRoomSize(float value) {
    requireUnit(value, "room size");
    roomSize.store(value, std::memory_order_relaxed);
    changed();
}

void Reverb::setDamping(float value) {
    requireUnit(value, "damping");
    damping.store(value, std::memory_order_relaxed);
    changed();
}

void Reverb::setWidth(float value) {
    requireUnit(value, "width");
    width.store(value, std::memory_order_relaxed);
    changed();
}

void Reverb::setWet(float value) {
    requireUnit(value, "wet level");
    wet.store(value, std::memory_order_relaxed);
    changed();
}

void Reverb::setDry(float value) {
    requireUnit(value, "dry level");
    dry.store(value, std::memory_order_relaxed);
    changed();
}

float Reverb::getTail() const noexcept {
    return Freeverb::getDecayTime(getRoomSize());
}

void Reverb::requireUnit(float value, const char* name) {
    if (!(value >= 0.0F && value <= 1.0F)) {
        throw std::invalid_argument(std::string("A reverb ") + name + " must be between 0 and 1.");
    }
}

void Reverb::changed() noexcept {
    version.fetch_add(1, std::memory_order_release);
}

void Reverb::prepare(std::uint32_t rate, std::uint32_t count) {
    channels = count;
    model = std::make_unique<Freeverb>(rate);
}

void Reverb::process(const float* input, float* output, std::uint32_t frames) noexcept {
    const std::uint32_t current = version.load(std::memory_order_acquire);
    if (current != appliedVersion) {
        appliedVersion = current;
        model->setRoomSize(getRoomSize());
        model->setDamping(getDamping());
        model->setMix(getWet(), getDry(), getWidth());
    }
    model->process(input, output, frames, channels);
}

} // namespace haylen::audio
