#include "haylen/audio/Mixer.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

#include "audio/MixerState.hpp"
#include "audio/SoundData.hpp"
#include "audio/StreamSource.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::audio {

const std::array<std::pair<std::string_view, Mixer::OutputState>, 4> Mixer::kOutputStateNames{{{"running", OutputState::Running}, {"locked", OutputState::Locked}, {"suspended", OutputState::Suspended}, {"unavailable", OutputState::Unavailable}}};
const Mixer::PlayOptions Mixer::kDefaultPlayOptions{};
const Mixer::MusicOptions Mixer::kDefaultMusicOptions{};
const std::array<std::pair<std::string_view, Mixer::Limit::Steal>, 3> Mixer::Limit::kStealNames{{{"oldest", Steal::Oldest}, {"quietest", Steal::Quietest}, {"none", Steal::None}}};

std::optional<Mixer::Limit::Steal> Mixer::Limit::stealFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kStealNames, name, &std::pair<std::string_view, Steal>::first);
    return found != kStealNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Mixer::Limit::stealName(Steal value) noexcept {
    return std::ranges::find(kStealNames, value, &std::pair<std::string_view, Steal>::second)->first;
}

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

Mixer::Mixer(const Setup& setup, core::JobSystem& jobs) {
    if (setup.sampleRate == 0 || setup.channels == 0 || setup.maxVoices == 0) {
        throw std::invalid_argument("Audio needs a sample rate, a channel count and room for at least one voice.");
    }
    if (setup.session.mixWithOthers && setup.session.category != Session::Category::Playback) {
        throw std::invalid_argument("Only the playback audio session mixes with other apps on request.");
    }
    state = std::make_unique<MixerState>(setup, jobs);

    ma_engine_config config = ma_engine_config_init();
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

void Mixer::requireFinite(float value, const char* name) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string("Audio needs a finite ") + name + ".");
    }
}

void Mixer::requireFade(float seconds) {
    if (!(seconds >= 0.0F && seconds <= kMaxFadeSeconds)) {
        throw std::invalid_argument("Audio needs a fade from 0 to " + std::to_string(static_cast<int>(kMaxFadeSeconds)) + " seconds.");
    }
}

void Mixer::requirePitch(float value) {
    if (!(value > 0.0F) || !std::isfinite(value)) {
        throw std::invalid_argument("Audio needs a finite pitch above 0.");
    }
}

void Mixer::requirePosition(math::Vec2 position) {
    requireFinite(position.x, "position");
    requireFinite(position.y, "position");
}

void Mixer::requirePlayOptions(const PlayOptions& options) {
    requireFinite(options.volume, "volume");
    requirePitch(options.pitch);
    if (!(options.pitchVariation >= 0.0F) || (options.pitchVariation > 0.0F && options.pitchVariation >= options.pitch)) {
        throw std::invalid_argument("A pitch variation must be at least 0 and smaller than the pitch.");
    }
    requireFinite(options.pan, "pan");
    requireFade(options.fadeIn);
    requireFinite(options.startAt, "start time");
    if (options.position) {
        requirePosition(*options.position);
    }
}

std::uint64_t Mixer::toFrame(float seconds, std::uint32_t sampleRate) noexcept {
    return static_cast<std::uint64_t>(std::llround(static_cast<double>(seconds) * static_cast<double>(sampleRate)));
}

void Mixer::requireLoop(const PlayOptions& options, float duration, std::uint32_t sampleRate) {
    const float end = options.loopEnd.value_or(duration);
    if (!std::isfinite(options.loopStart) || !std::isfinite(end) || options.loopStart < 0.0F || end <= options.loopStart || end > duration + 1.0e-4F) {
        throw std::invalid_argument("A loop needs 0 <= loopStart < loopEnd <= the length of the sound.");
    }

    // The voice repeats whole frames, and a loop that rounds to none would repeat nothing forever.
    if (toFrame(end, sampleRate) <= toFrame(options.loopStart, sampleRate)) {
        throw std::invalid_argument("A loop needs to span at least one frame of the sound.");
    }
}

Mixer::VoiceId Mixer::play(const Sound& sound, const PlayOptions& options) {
    if (!sound.isValid()) {
        throw std::invalid_argument("Cannot play an empty sound.");
    }
    requirePlayOptions(options);
    const SoundData& data = *sound.getData();
    const bool region = options.loopStart > 0.0F || options.loopEnd.has_value();
    if (region) {
        requireLoop(options, sound.getDuration(), data.sampleRate);
    }
    std::unique_ptr<MixerState::Voice> voice = state->createVoice(options);
    voice->sound = sound;

    if (data.reader) {
        voice->readerSource = std::make_unique<ReaderSource>(data);
        if (region) {
            voice->readerSource->setLoopRegion(toFrame(options.loopStart, data.sampleRate), toFrame(options.loopEnd.value_or(sound.getDuration()), data.sampleRate));
        }
        MixerState::Voice& streamed = *voice;
        const VoiceId id = state->start(std::move(voice), streamed.readerSource->get(), options);
        state->fill(streamed);
        return id;
    }

    ma_audio_buffer_config config = ma_audio_buffer_config_init(ma_format_f32, data.channels, data.frames, data.samples.data(), nullptr);
    config.sampleRate = data.sampleRate;
    if (ma_audio_buffer_init(&config, &voice->buffer) != MA_SUCCESS) {
        throw std::runtime_error("A sound could not be prepared for playback.");
    }
    voice->bufferReady = true;
    ma_data_source* source = &voice->buffer;
    if (region) {
        ma_data_source_set_loop_point_in_pcm_frames(source, toFrame(options.loopStart, data.sampleRate), std::min<ma_uint64>(toFrame(options.loopEnd.value_or(sound.getDuration()), data.sampleRate), data.frames));
    }
    return state->start(std::move(voice), source, options);
}

// The voice takes the stream over from the voice that played it before, which reads silence from then on.
Mixer::VoiceId Mixer::play(const std::shared_ptr<platform::AudioStream>& stream, const PlayOptions& options) {
    if (!stream) {
        throw std::invalid_argument("Cannot play an empty audio stream.");
    }
    if (options.loop || options.startAt != 0.0F || options.pitchVariation != 0.0F || options.loopStart != 0.0F || options.loopEnd) {
        throw std::invalid_argument("A stream plays as its samples arrive, so its voice takes no loop, start time or pitch variation.");
    }
    requirePlayOptions(options);
    std::unique_ptr<MixerState::Voice> voice = state->createVoice(options);
    voice->stream = stream;
    voice->streamSource = std::make_unique<StreamSource>(*stream);
    ma_data_source* source = voice->streamSource->get();
    return state->start(std::move(voice), source, options);
}

void Mixer::stop(VoiceId id, float fadeOutSeconds) {
    requireFade(fadeOutSeconds);
    MixerState::Voice* voice = state->findVoice(id);
    if (voice == nullptr) {
        return;
    }
    state->fadeOut(*voice, fadeOutSeconds);
}

void Mixer::setLimit(const Sound& sound, const Limit& limit) {
    if (!sound.isValid()) {
        throw std::invalid_argument("Cannot limit an empty sound.");
    }
    if (!(limit.cooldown >= 0.0F) || !std::isfinite(limit.cooldown)) {
        throw std::invalid_argument("The cooldown of a sound must be a finite number of seconds from 0.");
    }
    const SoundData* data = sound.getData().get();
    if (limit.voices == 0 && limit.cooldown == 0.0F) {
        state->limits.erase(data);
        return;
    }
    MixerState::Limited& limited = state->limits[data];
    limited.sound = sound.getData();
    limited.limit = limit;
}

std::optional<Mixer::Limit> Mixer::getLimit(const Sound& sound) const {
    if (!sound.isValid()) {
        return std::nullopt;
    }
    const MixerState::Limited* limited = state->findLimit(sound);
    return limited != nullptr ? std::optional(limited->limit) : std::nullopt;
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
    requireFinite(volume, "volume");
    if (MixerState::Voice* voice = state->findVoice(id)) {
        voice->volume = volume;
        state->apply(*voice);
    }
}

void Mixer::setPitch(VoiceId id, float pitch) {
    requirePitch(pitch);
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
    requireFinite(pan, "pan");
    if (MixerState::Voice* voice = state->findVoice(id)) {
        voice->pan = pan;
        state->apply(*voice);
    }
}

void Mixer::setPosition(VoiceId id, math::Vec2 position) {
    requirePosition(position);
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
    return voice != nullptr && !voice->stopped && (ma_sound_is_playing(&voice->handle) == MA_TRUE || voice->isHeld());
}

float Mixer::getCursor(VoiceId id) const {
    const MixerState::Voice* voice = state->findVoice(id);
    float seconds = 0.0F;
    if (voice != nullptr) {
        ma_sound_get_cursor_in_seconds(&voice->handle, &seconds);
    }
    return seconds;
}

void Mixer::setCursor(VoiceId id, float seconds) {
    requireFinite(seconds, "cursor");
    MixerState::Voice* voice = state->findVoice(id);
    if (voice == nullptr) {
        return;
    }
    if (voice->stream) {
        throw std::invalid_argument("The voice of a live audio stream has no cursor to move.");
    }
    // The length comes from the sound, because asking the decoder of a streamed voice would seek it while the audio thread reads it.
    const float length = voice->sound.getDuration();
    ma_sound_seek_to_second(&voice->handle, std::max(0.0F, length > 0.0F ? std::min(seconds, length) : seconds));
}

void Mixer::stopAll(float fadeOutSeconds) {
    requireFade(fadeOutSeconds);
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

Mixer::VoiceId Mixer::playMusic(const Sound& sound, const MusicOptions& options) {
    // Asking for the track that is already playing keeps it going instead of restarting it.
    if (sound.isValid() && getMusic() == sound) {
        setVolume(state->musicVoice, options.volume);
        return state->musicVoice;
    }

    // The next track starts before the current one fades out, so a track that cannot play leaves the current one playing.
    const VoiceId track = play(sound, {.bus = options.bus, .volume = options.volume, .loop = options.loop, .fadeIn = options.fade, .startAt = options.startAt, .loopStart = options.loopStart, .loopEnd = options.loopEnd});
    stop(state->musicVoice, options.fade);
    state->musicVoice = track;
    state->findVoice(track)->music = true;
    return track;
}

void Mixer::stopMusic(float fadeOutSeconds) {
    stop(state->musicVoice, fadeOutSeconds);
    state->musicVoice = 0;
}

Sound Mixer::getMusic() const {
    return isActive(state->musicVoice) ? state->findVoice(state->musicVoice)->sound : Sound{};
}

void Mixer::createBus(const std::string& name, std::string_view parent) {
    if (name.empty() || state->buses.contains(name)) {
        throw std::invalid_argument("An audio bus needs a non-empty name that no other bus uses, not \"" + name + "\".");
    }
    state->addBus(name, &state->getBus(parent));
}

void Mixer::setBusVolume(std::string_view name, float volume, float fadeSeconds) {
    requireFinite(volume, "bus volume");
    requireFade(fadeSeconds);
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

void Mixer::setOutputMuted(bool value) {
    state->outputMuted = value;
    ma_engine_set_volume(&state->engine, value ? 0.0F : 1.0F);
}

bool Mixer::isOutputMuted() const noexcept {
    return state->outputMuted;
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
            if (!voice->stopped && voice->isHeld()) {
                ++entry.paused;
            } else if (ma_sound_is_playing(&voice->handle) == MA_TRUE) {
                ++entry.playing;
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

void Mixer::setListener(math::Vec2 position) {
    requirePosition(position);
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
    for (const float setting : {value.minDistance, value.maxDistance, value.rolloff, value.panDistance, value.doppler, value.speedOfSound}) {
        requireFinite(setting, "spatialization setting");
    }
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
    const bool interrupted = isInterrupted();
    state->output.endInterruption();
    if (interrupted) {
        resumeAll(PauseReason::Interruption);
    }
}

void Mixer::restartOutput() {
    state->output.restart();
}

bool Mixer::isInterrupted() const noexcept {
    return state->output.isInterrupted();
}

void Mixer::update(float deltaSeconds) {
    // A device that opened starts playing, and device events go out first, so the listeners that pause or resume the mixer act before the voices refresh. A listener that fails keeps neither the other events nor the voices from their update, and its failure comes out last.
    state->output.update(deltaSeconds);
    state->clock += static_cast<double>(std::max(deltaSeconds, 0.0F));
    std::exception_ptr failure;
    for (const DeviceEvent event : state->output.takeEvents()) {
        try {
            deviceEventReceived.emit(event);
        } catch (...) {
            failure = failure != nullptr ? failure : std::current_exception();
        }
    }
    state->advance(deltaSeconds);
    state->releaseFinished();
    state->spatialize(deltaSeconds);
    for (const std::unique_ptr<MixerState::Voice>& voice : state->voices) {
        state->fill(*voice);
    }
    if (failure != nullptr) {
        std::rethrow_exception(failure);
    }
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

bool Mixer::isOutputAvailable() const noexcept {
    return state->output.isAvailable();
}

bool Mixer::isOutputOpening() const noexcept {
    return state->output.isOpening();
}

Mixer::OutputState Mixer::getOutputState() const noexcept {
    const Device& output = state->output;
    if (!output.hasDevice() || !output.isAvailable() || output.isOpening()) {
        return OutputState::Unavailable;
    }
    if (output.isSuspended()) {
        return OutputState::Suspended;
    }
    return output.isLocked() ? OutputState::Locked : OutputState::Running;
}

std::string_view Mixer::outputStateName(OutputState value) noexcept {
    return std::ranges::find(kOutputStateNames, value, &std::pair<std::string_view, OutputState>::second)->first;
}

float Mixer::getOutputLatency() const noexcept {
    return state->output.getLatency();
}

void Mixer::render(std::span<float> samples) {
    if (state->output.isSelfRendering()) {
        throw std::logic_error("A mixer with a device or one that keeps time renders on its own.");
    }
    if (!state->output.isRunning()) {
        std::ranges::fill(samples, 0.0F);
        return;
    }
    const ma_uint64 frames = samples.size() / getChannels();
    ma_engine_read_pcm_frames(&state->engine, samples.data(), frames, nullptr);
}

} // namespace haylen::audio
