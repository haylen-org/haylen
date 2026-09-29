#include "haylen/audio/Mixer.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "audio/MixerState.hpp"
#include "audio/SoundData.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::audio {

const Mixer::PlayOptions Mixer::kDefaultPlayOptions{};
const Mixer::MusicOptions Mixer::kDefaultMusicOptions{};

float Mixer::Spatialization::getGain(float distance) const noexcept {
    const float clamped = std::clamp(distance, minDistance, maxDistance);
    switch (model) {
    case Model::Linear:
        return math::Math::saturate(1.0F - rolloff * (clamped - minDistance) / (maxDistance - minDistance));
    case Model::Inverse:
        return minDistance / (minDistance + rolloff * (clamped - minDistance));
    case Model::Exponential:
        return std::pow(clamped / minDistance, -rolloff);
    }
    return 1.0F;
}

float Mixer::Spatialization::getPan(math::Vec2 offset) const noexcept {
    return std::clamp(offset.x / panDistance, -1.0F, 1.0F);
}

float Mixer::Spatialization::getDopplerPitch(math::Vec2 offset, math::Vec2 listenerVelocity, math::Vec2 voiceVelocity) const noexcept {
    const float distance = offset.getLength();
    if (doppler <= 0.0F || distance <= 0.0F) {
        return 1.0F;
    }

    // Speeds along the line from the listener to the voice, positive when the two approach each other.
    const math::Vec2 direction = offset / distance;
    const float listenerApproach = math::Vec2::dot(listenerVelocity, direction) * doppler;
    const float voiceApproach = -math::Vec2::dot(voiceVelocity, direction) * doppler;
    const float remaining = speedOfSound - voiceApproach;
    const float ratio = remaining > 0.0F ? (speedOfSound + listenerApproach) / remaining : kMaxDopplerPitch;
    return std::clamp(ratio, kMinDopplerPitch, kMaxDopplerPitch);
}

Mixer::Mixer(const Setup& setup) {
    if (setup.sampleRate == 0 || setup.channels == 0 || setup.maxVoices == 0) {
        throw std::invalid_argument("Audio needs a sample rate, a channel count and room for at least one voice.");
    }
    if (setup.session.mixWithOthers && setup.session.category != Session::Category::Playback) {
        throw std::invalid_argument("Only the playback audio session mixes with other apps on request.");
    }
    state = std::make_unique<MixerState>(setup);

    ma_engine_config config = ma_engine_config_init();
    config.pDevice = state->output.get();
    config.noDevice = MA_TRUE;
    config.noAutoStart = MA_TRUE;
    config.channels = setup.channels;
    config.sampleRate = setup.sampleRate;
    if (ma_engine_init(&config, &state->engine) != MA_SUCCESS) {
        throw std::runtime_error("The audio engine could not be started.");
    }
    state->engineReady = true;
    state->maxVoices = setup.maxVoices;

    state->addBus("master", nullptr);
    MixerState::Bus& master = state->getBus("master");
    for (const char* name : {"music", "sfx", "ui", "ambience"}) {
        state->addBus(name, &master);
    }
    state->getBus("music").processMode = core::ProcessMode::Always;
    state->getBus("ui").processMode = core::ProcessMode::Always;
    state->getBus("sfx").processMode = core::ProcessMode::Pausable;
    state->getBus("ambience").processMode = core::ProcessMode::Pausable;
    state->output.start(state->engine);
}

Mixer::~Mixer() = default;

Mixer::VoiceId Mixer::play(const Sound& sound, const PlayOptions& options) {
    if (!sound.isValid()) {
        throw std::invalid_argument("Cannot play an empty sound.");
    }
    if (options.pitchVariation < 0.0F || (options.pitchVariation > 0.0F && options.pitchVariation >= options.pitch)) {
        throw std::invalid_argument("A pitch variation must be at least 0 and smaller than the pitch.");
    }
    MixerState::Bus& bus = state->getBus(options.bus);
    state->makeRoom();

    auto voice = std::make_unique<MixerState::Voice>();
    voice->id = state->nextVoice++;
    voice->sound = sound;
    voice->bus = &bus;
    voice->processMode = options.processMode;
    voice->volume = options.volume;
    voice->pan = options.pan;
    voice->pitch = options.pitchVariation > 0.0F ? state->random.range(options.pitch - options.pitchVariation, options.pitch + options.pitchVariation) : options.pitch;
    voice->position = options.position;
    voice->lastPosition = options.position.value_or(math::Vec2{});

    const SoundData& data = *sound.getData();
    ma_data_source* source = nullptr;
    if (data.streamed) {
        const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
        if (ma_decoder_init_memory(data.encoded.data(), data.encoded.size(), &config, &voice->decoder) != MA_SUCCESS) {
            throw std::runtime_error("A streamed sound could not be decoded.");
        }
        voice->decoderSource = true;
        source = &voice->decoder;
    } else {
        ma_audio_buffer_config config = ma_audio_buffer_config_init(ma_format_f32, data.channels, data.frames, data.samples.data(), nullptr);
        config.sampleRate = data.sampleRate;
        if (ma_audio_buffer_init(&config, &voice->buffer) != MA_SUCCESS) {
            throw std::runtime_error("A sound could not be prepared for playback.");
        }
        source = &voice->buffer;
    }
    voice->sourceReady = true;

    if (ma_sound_init_from_data_source(&state->engine, source, MA_SOUND_FLAG_NO_SPATIALIZATION, &bus.group, &voice->handle) != MA_SUCCESS) {
        throw std::runtime_error("A voice could not be created.");
    }
    voice->handleReady = true;

    ma_sound_set_looping(&voice->handle, options.loop ? MA_TRUE : MA_FALSE);
    if (options.startAt > 0.0F) {
        ma_sound_seek_to_second(&voice->handle, options.startAt);
    }
    if (options.fadeIn > 0.0F) {
        ma_sound_set_fade_in_milliseconds(&voice->handle, 0.0F, 1.0F, MixerState::toMilliseconds(options.fadeIn));
    }
    for (const std::shared_ptr<Effect>& effect : options.effects) {
        state->getEffects(*voice).add(effect);
    }
    state->apply(*voice);
    state->refresh(*voice);

    const VoiceId id = voice->id;
    state->voices.push_back(std::move(voice));
    return id;
}

void Mixer::stop(VoiceId id, float fadeOutSeconds) {
    MixerState::Voice* voice = state->findVoice(id);
    if (voice == nullptr) {
        return;
    }
    voice->stopped = true;
    if (fadeOutSeconds > 0.0F) {
        ma_sound_stop_with_fade_in_milliseconds(&voice->handle, MixerState::toMilliseconds(fadeOutSeconds));
        return;
    }
    ma_sound_stop(&voice->handle);
}

void Mixer::setPaused(VoiceId id, bool paused) {
    if (MixerState::Voice* voice = state->findVoice(id)) {
        voice->setHold(MixerState::Voice::kVoiceHold, paused);
        state->refresh(*voice);
    }
}

bool Mixer::isPaused(VoiceId id) const {
    const MixerState::Voice* voice = state->findVoice(id);
    return voice != nullptr && (voice->holds & MixerState::Voice::kVoiceHold) != 0;
}

void Mixer::setVolume(VoiceId id, float volume) {
    if (MixerState::Voice* voice = state->findVoice(id)) {
        voice->volume = volume;
        state->apply(*voice);
    }
}

void Mixer::setPitch(VoiceId id, float pitch) {
    if (MixerState::Voice* voice = state->findVoice(id)) {
        voice->pitch = pitch;
        state->apply(*voice);
    }
}

float Mixer::getPitch(VoiceId id) const {
    const MixerState::Voice* voice = state->findVoice(id);
    return voice != nullptr ? voice->pitch : 0.0F;
}

void Mixer::seedVariation(std::uint64_t seed) noexcept {
    state->random.reseed(seed);
}

void Mixer::setPan(VoiceId id, float pan) {
    if (MixerState::Voice* voice = state->findVoice(id)) {
        voice->pan = pan;
        state->apply(*voice);
    }
}

void Mixer::setPosition(VoiceId id, math::Vec2 position) {
    MixerState::Voice* voice = state->findVoice(id);
    if (voice == nullptr) {
        return;
    }

    // A voice that just became positional starts at rest, so its first move is no leap from the origin.
    if (!voice->position) {
        voice->lastPosition = position;
    }
    voice->position = position;
    state->apply(*voice);
}

core::ProcessMode Mixer::getProcessMode(VoiceId id) const {
    const MixerState::Voice* voice = state->findVoice(id);
    return voice != nullptr ? voice->processMode : core::ProcessMode::Inherit;
}

bool Mixer::isActive(VoiceId id) const {
    const MixerState::Voice* voice = state->findVoice(id);
    return voice != nullptr && (ma_sound_is_playing(&voice->handle) == MA_TRUE || (!voice->stopped && voice->isHeld()));
}

float Mixer::getCursor(VoiceId id) const {
    const MixerState::Voice* voice = state->findVoice(id);
    float seconds = 0.0F;
    if (voice != nullptr) {
        ma_sound_get_cursor_in_seconds(&voice->handle, &seconds);
    }
    return seconds;
}

void Mixer::stopAll(float fadeOutSeconds) {
    for (const std::unique_ptr<MixerState::Voice>& voice : state->voices) {
        stop(voice->id, fadeOutSeconds);
    }
}

std::size_t Mixer::getVoiceCount() const noexcept {
    return state->voices.size();
}

void Mixer::pauseAll(PauseReason reason) {
    const std::uint8_t hold = MixerState::toHold(reason);
    for (const std::unique_ptr<MixerState::Voice>& voice : state->voices) {
        voice->setHold(hold, true);
        state->refresh(*voice);
    }
}

void Mixer::resumeAll(PauseReason reason) {
    const std::uint8_t hold = MixerState::toHold(reason);
    for (const std::unique_ptr<MixerState::Voice>& voice : state->voices) {
        voice->setHold(hold, false);
        state->refresh(*voice);
    }
}

void Mixer::addEffect(VoiceId id, std::shared_ptr<Effect> effect) {
    if (MixerState::Voice* voice = state->findVoice(id)) {
        state->getEffects(*voice).add(std::move(effect));
    }
}

void Mixer::removeEffect(VoiceId id, const Effect& effect) {
    if (MixerState::Voice* voice = state->findVoice(id); voice != nullptr && voice->effects) {
        voice->effects->remove(effect);
    }
}

std::vector<std::shared_ptr<Effect>> Mixer::getEffects(VoiceId id) const {
    const MixerState::Voice* voice = state->findVoice(id);
    return voice != nullptr && voice->effects ? voice->effects->getEffects() : std::vector<std::shared_ptr<Effect>>{};
}

void Mixer::addBusEffect(std::string_view bus, std::shared_ptr<Effect> effect) {
    state->getBus(bus).effects->add(std::move(effect));
}

void Mixer::removeBusEffect(std::string_view bus, const Effect& effect) {
    state->getBus(bus).effects->remove(effect);
}

std::vector<std::shared_ptr<Effect>> Mixer::getBusEffects(std::string_view bus) const {
    return state->getBus(bus).effects->getEffects();
}

void Mixer::playMusic(const Sound& sound, const MusicOptions& options) {
    // Asking for the track that is already playing keeps it going instead of restarting it.
    if (MixerState::Voice* current = state->findVoice(state->musicVoice); current != nullptr && current->sound == sound && isActive(current->id)) {
        setVolume(current->id, options.volume);
        return;
    }

    stop(state->musicVoice, options.fade);
    state->musicVoice = play(sound, {.bus = options.bus, .volume = options.volume, .loop = options.loop, .fadeIn = options.fade});
    state->findVoice(state->musicVoice)->music = true;
}

void Mixer::stopMusic(float fadeOutSeconds) {
    stop(state->musicVoice, fadeOutSeconds);
    state->musicVoice = 0;
}

Sound Mixer::getMusic() const {
    const MixerState::Voice* voice = state->findVoice(state->musicVoice);
    return voice != nullptr && isActive(voice->id) ? voice->sound : Sound{};
}

void Mixer::createBus(const std::string& name, std::string_view parent) {
    if (name.empty() || state->buses.contains(name)) {
        throw std::invalid_argument("An audio bus needs a new, non-empty name: " + name);
    }
    state->addBus(name, &state->getBus(parent));
}

void Mixer::setBusVolume(std::string_view name, float volume, float fadeSeconds) {
    MixerState::Bus& bus = state->getBus(name);
    bus.volume = std::max(0.0F, volume);
    ma_sound_group_set_fade_in_milliseconds(&bus.group, -1.0F, bus.volume, MixerState::toMilliseconds(fadeSeconds));
}

float Mixer::getBusVolume(std::string_view name) const {
    return state->getBus(name).volume;
}

void Mixer::setBusMuted(std::string_view name, bool muted) {
    MixerState::Bus& bus = state->getBus(name);
    bus.muted = muted;
    ma_sound_group_set_volume(&bus.group, muted ? 0.0F : 1.0F);
}

bool Mixer::isBusMuted(std::string_view name) const {
    return state->getBus(name).muted;
}

void Mixer::setBusProcessMode(std::string_view name, core::ProcessMode mode) {
    state->getBus(name).processMode = mode;
    state->refreshAll();
}

core::ProcessMode Mixer::getBusProcessMode(std::string_view name) const {
    return state->getBus(name).processMode;
}

std::vector<std::string> Mixer::getBuses() const {
    std::vector<std::string> names;
    names.reserve(state->buses.size());
    for (const auto& [name, bus] : state->buses) {
        names.push_back(name);
    }
    return names;
}

std::vector<Mixer::BusStats> Mixer::getBusStats() const {
    std::vector<BusStats> stats;
    stats.reserve(state->buses.size());
    for (const auto& [name, bus] : state->buses) {
        BusStats entry{.name = name, .processing = core::FrameClock::canProcess(state->resolveMode(*bus), state->processPaused)};
        for (const std::unique_ptr<MixerState::Voice>& voice : state->voices) {
            if (voice->bus != bus.get()) {
                continue;
            }
            ++entry.voices;
            if (ma_sound_is_playing(&voice->handle) == MA_TRUE) {
                ++entry.playing;
            } else if (!voice->stopped && voice->isHeld()) {
                ++entry.paused;
            }
        }
        stats.push_back(std::move(entry));
    }
    return stats;
}

void Mixer::setProcessPaused(bool value) {
    if (state->processPaused == value) {
        return;
    }
    state->processPaused = value;
    state->refreshAll();
}

bool Mixer::isProcessPaused() const noexcept {
    return state->processPaused;
}

void Mixer::setListener(math::Vec2 position) noexcept {
    state->listener = position;
}

math::Vec2 Mixer::getListener() const noexcept {
    return state->listener;
}

void Mixer::followCamera(const graphics2d::Camera* camera) noexcept {
    state->camera = camera;
    if (camera != nullptr) {
        state->listener = camera->position + camera->offset;
        state->lastListener = state->listener;
    }
}

void Mixer::setSpatialization(const Spatialization& value) {
    if (value.minDistance < 0.0F || value.maxDistance <= value.minDistance) {
        throw std::invalid_argument("Audio attenuation needs 0 <= minimum distance < maximum distance.");
    }
    if (value.model != Spatialization::Model::Linear && value.minDistance <= 0.0F) {
        throw std::invalid_argument("The inverse and exponential audio models need a minimum distance above 0.");
    }
    if (value.rolloff < 0.0F || value.doppler < 0.0F) {
        throw std::invalid_argument("The audio rolloff and Doppler factor cannot be negative.");
    }
    if (value.panDistance <= 0.0F || value.speedOfSound <= 0.0F) {
        throw std::invalid_argument("The audio pan distance and speed of sound must be positive.");
    }
    state->spatialization = value;
    for (const std::unique_ptr<MixerState::Voice>& voice : state->voices) {
        state->apply(*voice);
    }
}

const Mixer::Spatialization& Mixer::getSpatialization() const noexcept {
    return state->spatialization;
}

void Mixer::suspend() {
    state->output.suspend();
}

void Mixer::resume() {
    state->output.resume();
}

void Mixer::reportDeviceEvent(DeviceEvent event) {
    state->output.report(event);
}

void Mixer::beginInterruption() {
    if (isInterrupted()) {
        return;
    }
    state->output.interrupt();
    pauseAll(PauseReason::Interruption);
}

void Mixer::endInterruption() {
    if (!isInterrupted()) {
        return;
    }
    state->output.endInterruption();
    resumeAll(PauseReason::Interruption);
}

bool Mixer::isInterrupted() const noexcept {
    return state->output.isInterrupted();
}

void Mixer::update(float deltaSeconds) {
    // Device events go out first, so the listeners that pause or resume the mixer act before the voices refresh.
    for (const DeviceEvent event : state->output.takeEvents()) {
        deviceEventReceived.emit(event);
    }
    state->releaseFinished();
    state->spatialize(deltaSeconds);
}

std::uint32_t Mixer::getSampleRate() const noexcept {
    return ma_engine_get_sample_rate(&state->engine);
}

std::uint32_t Mixer::getChannels() const noexcept {
    return ma_engine_get_channels(&state->engine);
}

bool Mixer::hasDevice() const noexcept {
    return state->output.hasDevice();
}

void Mixer::render(std::span<float> samples) {
    if (hasDevice()) {
        throw std::logic_error("A mixer with a device renders on its own.");
    }
    if (!state->output.isRunning()) {
        std::ranges::fill(samples, 0.0F);
        return;
    }
    const ma_uint64 frames = samples.size() / getChannels();
    ma_engine_read_pcm_frames(&state->engine, samples.data(), frames, nullptr);
}

} // namespace haylen::audio
