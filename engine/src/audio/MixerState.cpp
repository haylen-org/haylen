#include "audio/MixerState.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/JobSystem.hpp"

namespace haylen::audio {

MixerState::Voice::~Voice() {
    if (handleReady) {
        ma_sound_uninit(&handle);
    }
    if (bufferReady) {
        ma_audio_buffer_uninit(&buffer);
    }
}

MixerState::~MixerState() {
    output.stop();
    voices.clear();
    tails.clear();
    for (auto name = busOrder.rbegin(); name != busOrder.rend(); ++name) {
        Bus& bus = *buses.at(*name);
        bus.effects.reset();
        ma_sound_group_uninit(&bus.group);
    }
    if (engineReady) {
        ma_engine_uninit(&engine);
    }
}

ma_uint64 MixerState::toMilliseconds(float seconds) {
    return static_cast<ma_uint64>(std::lround(std::max(0.0F, seconds) * 1000.0F));
}

std::uint8_t MixerState::toHold(Mixer::PauseReason reason) noexcept {
    return reason == Mixer::PauseReason::App ? Voice::kAppHold : Voice::kInterruptionHold;
}

MixerState::Bus& MixerState::getBus(std::string_view name) const {
    const auto found = buses.find(name);
    if (found == buses.end()) {
        throw std::invalid_argument("The audio bus \"" + std::string(name) + "\" does not exist.");
    }
    return *found->second;
}

MixerState::Voice* MixerState::findVoice(Mixer::VoiceId id) const noexcept {
    const auto found = std::find_if(voices.begin(), voices.end(), [id](const std::unique_ptr<Voice>& voice) { return voice->id == id; });
    return found == voices.end() ? nullptr : found->get();
}

void MixerState::addBus(const std::string& name, Bus* parent) {
    auto created = std::make_unique<Bus>();
    created->name = name;
    created->parent = parent;
    if (ma_sound_group_init(&engine, 0, parent != nullptr ? &parent->group : nullptr, &created->group) != MA_SUCCESS) {
        throw std::runtime_error("The audio bus \"" + name + "\" could not be created.");
    }
    created->effects = std::make_unique<EffectChain>(engine, &created->group, parent != nullptr ? &parent->group : ma_engine_get_endpoint(&engine));
    buses.emplace(name, std::move(created));
    busOrder.push_back(name);
}

std::unique_ptr<MixerState::Voice> MixerState::createVoice(const Mixer::PlayOptions& options) {
    Bus& bus = getBus(options.bus);
    auto voice = std::make_unique<Voice>();
    voice->id = nextVoice++;
    voice->bus = &bus;
    voice->processMode = options.processMode;
    voice->volume = options.volume;
    voice->priority = options.priority;
    voice->pan = options.pan;
    voice->pitch = options.pitchVariation > 0.0F ? random.range(options.pitch - options.pitchVariation, options.pitch + options.pitchVariation) : options.pitch;
    voice->position = options.position;
    voice->lastPosition = options.position.value_or(math::Vec2{});
    return voice;
}

Mixer::VoiceId MixerState::start(std::unique_ptr<Voice> voice, ma_data_source* source, const Mixer::PlayOptions& options) {
    Limited* limited = voice->sound.isValid() ? findLimit(voice->sound) : nullptr;
    if (limited != nullptr && !admits(*limited)) {
        return voice->id;
    }
    if (ma_sound_init_from_data_source(&engine, source, MA_SOUND_FLAG_NO_SPATIALIZATION, &voice->bus->group, &voice->handle) != MA_SUCCESS) {
        throw std::runtime_error("A voice could not be created.");
    }
    voice->handleReady = true;

    ma_sound_set_looping(&voice->handle, options.loop ? MA_TRUE : MA_FALSE);
    if (options.startAt > 0.0F) {
        ma_sound_seek_to_second(&voice->handle, options.startAt);
    }
    if (options.fadeIn > 0.0F) {
        ma_sound_set_fade_in_milliseconds(&voice->handle, 0.0F, 1.0F, toMilliseconds(options.fadeIn));
    }
    for (const std::shared_ptr<Effect>& effect : options.effects) {
        getEffects(*voice).add(effect);
    }

    if (limited != nullptr) {
        makeRoomFor(*limited);
        limited->lastPlay = clock;
    }
    makeRoom();
    apply(*voice);
    refresh(*voice);

    const Mixer::VoiceId id = voice->id;
    voices.push_back(std::move(voice));
    return id;
}

core::ProcessMode MixerState::resolveMode(const Voice& voice) const noexcept {
    return voice.processMode != core::ProcessMode::Inherit ? voice.processMode : resolveMode(*voice.bus);
}

core::ProcessMode MixerState::resolveMode(const Bus& bus) const noexcept {
    for (const Bus* level = &bus; level != nullptr; level = level->parent) {
        if (level->processMode != core::ProcessMode::Inherit) {
            return level->processMode;
        }
    }
    return core::ProcessMode::Pausable;
}

void MixerState::refresh(Voice& voice) {
    if (voice.stopped) {
        return;
    }
    voice.processHeld = !core::FrameClock::canProcess(resolveMode(voice), processPaused);
    const bool play = !voice.isHeld();
    if (play == voice.started) {
        return;
    }
    voice.started = play;
    if (!play) {
        ma_sound_stop_with_fade_in_milliseconds(&voice.handle, toMilliseconds(Mixer::kDeclickSeconds));
        return;
    }

    // Starting a sound at its end would replay it, so a voice that finished while it was held stays finished. A voice that played before comes back with the declick ramp, since its pause scheduled a stop and a fade.
    if (ma_sound_at_end(&voice.handle) == MA_TRUE) {
        return;
    }
    if (voice.everStarted) {
        ma_sound_reset_stop_time_and_fade(&voice.handle);
        ma_sound_set_fade_in_milliseconds(&voice.handle, 0.0F, 1.0F, toMilliseconds(Mixer::kDeclickSeconds));
    }
    voice.everStarted = true;
    ma_sound_start(&voice.handle);
}

void MixerState::refreshAll() {
    for (const std::unique_ptr<Voice>& voice : voices) {
        refresh(*voice);
    }
}

void MixerState::apply(Voice& voice) const {
    float gain = 1.0F;
    float pan = voice.pan;
    float pitch = voice.pitch;
    if (voice.position) {
        const math::Vec2 offset = *voice.position - listener;
        gain = spatialization.getGain(offset.getLength());
        pan += spatialization.getPan(offset);
        pitch *= spatialization.getDopplerPitch(offset, listenerVelocity, voice.velocity);
    }
    ma_sound_set_volume(&voice.handle, voice.volume * gain);
    ma_sound_set_pan(&voice.handle, std::clamp(pan, -1.0F, 1.0F));
    ma_sound_set_pitch(&voice.handle, pitch);
}

void MixerState::fill(Voice& voice) const {
    if (!voice.readerSource) {
        return;
    }
    if (std::function<void()> work = voice.readerSource->takeFill()) {
        jobs.postIo(std::move(work));
    }
}

void MixerState::retire(Voice& voice) {
    if (!voice.effects || voice.effects->empty()) {
        return;
    }
    const float tail = voice.effects->getTail();
    if (tail > 0.0F) {
        const auto frames = static_cast<ma_uint64>(std::ceil(tail * static_cast<float>(ma_engine_get_sample_rate(&engine))));
        tails.push_back({std::move(voice.effects), ma_engine_get_time_in_pcm_frames(&engine) + frames});
    }
}

EffectChain& MixerState::getEffects(Voice& voice) {
    if (!voice.effects) {
        voice.effects = std::make_unique<EffectChain>(engine, &voice.handle, &voice.bus->group);
    }
    return *voice.effects;
}

void MixerState::advance(float deltaSeconds) {
    if (!output.isSilent()) {
        silentFrames = 0.0;
        return;
    }

    // The fraction of a frame left over carries into the next update, so the mix keeps exact time over many frames.
    silentFrames += static_cast<double>(std::max(deltaSeconds, 0.0F)) * static_cast<double>(ma_engine_get_sample_rate(&engine));
    auto frames = static_cast<ma_uint64>(silentFrames);
    silentFrames -= static_cast<double>(frames);
    silentBlock.resize(kSilentBlockFrames * ma_engine_get_channels(&engine));
    while (frames > 0) {
        const ma_uint64 block = std::min(frames, kSilentBlockFrames);
        ma_engine_read_pcm_frames(&engine, silentBlock.data(), block, nullptr);
        frames -= block;
    }
}

void MixerState::releaseFinished() {
    for (auto voice = voices.begin(); voice != voices.end();) {
        const bool finished = ((*voice)->stopped || !(*voice)->isHeld()) && ma_sound_is_playing(&(*voice)->handle) == MA_FALSE;
        if (!finished) {
            ++voice;
            continue;
        }
        retire(**voice);
        voice = voices.erase(voice);
    }

    const ma_uint64 now = ma_engine_get_time_in_pcm_frames(&engine);
    std::erase_if(tails, [now](const Tail& tail) { return tail.endFrame <= now; });
}

void MixerState::spatialize(float deltaSeconds) {
    if (camera != nullptr) {
        listener = camera->position + camera->offset;
    }

    // Velocities come from the movement since the last update, and an update without elapsed time sees everything at rest.
    const float rate = deltaSeconds > 0.0F ? 1.0F / deltaSeconds : 0.0F;
    listenerVelocity = (listener - lastListener) * rate;
    lastListener = listener;
    for (const std::unique_ptr<Voice>& voice : voices) {
        if (!voice->position) {
            continue;
        }
        voice->velocity = (*voice->position - voice->lastPosition) * rate;
        voice->lastPosition = *voice->position;
        apply(*voice);
    }
}

// A voice that something holds already went quiet with its own ramp, so its stop changes nothing more.
void MixerState::fadeOut(Voice& voice, float seconds) {
    voice.stopped = true;
    if (!voice.started) {
        return;
    }
    ma_sound_stop_with_fade_in_milliseconds(&voice.handle, toMilliseconds(std::max(seconds, Mixer::kDeclickSeconds)));
}

MixerState::Limited* MixerState::findLimit(const Sound& sound) {
    const auto found = limits.find(sound.getData().get());
    if (found == limits.end()) {
        return nullptr;
    }
    // A limit whose sound went away belongs to no sound, even when new data takes the same address.
    if (found->second.sound.expired()) {
        limits.erase(found);
        return nullptr;
    }
    return &found->second;
}

bool MixerState::admits(const Limited& limited) const {
    if (limited.lastPlay && clock - *limited.lastPlay < static_cast<double>(limited.limit.cooldown)) {
        return false;
    }
    if (limited.limit.voices == 0 || limited.limit.steal != Mixer::Limit::Steal::None) {
        return true;
    }
    const SoundData* data = limited.sound.lock().get();
    const auto playing = std::ranges::count_if(voices, [data](const std::unique_ptr<Voice>& voice) { return !voice->stopped && voice->sound.getData().get() == data; });
    return static_cast<std::size_t>(playing) < limited.limit.voices;
}

void MixerState::makeRoomFor(Limited& limited) {
    if (limited.limit.voices == 0) {
        return;
    }
    const SoundData* data = limited.sound.lock().get();
    for (;;) {
        Voice* victim = nullptr;
        std::size_t playing = 0;
        for (const std::unique_ptr<Voice>& voice : voices) {
            if (voice->stopped || voice->sound.getData().get() != data) {
                continue;
            }
            ++playing;
            const bool quieter = victim != nullptr && limited.limit.steal == Mixer::Limit::Steal::Quietest && voice->volume < victim->volume;
            if (victim == nullptr || quieter) {
                victim = voice.get();
            }
        }
        if (playing < limited.limit.voices) {
            return;
        }
        fadeOut(*victim, 0.0F);
    }
}

void MixerState::makeRoom() {
    for (;;) {
        Voice* victim = nullptr;
        std::size_t playing = 0;
        for (const std::unique_ptr<Voice>& voice : voices) {
            if (voice->stopped) {
                continue;
            }
            ++playing;
            if (victim == nullptr || (victim->music && !voice->music)) {
                victim = voice.get();
                continue;
            }
            const bool lower = voice->priority < victim->priority || (voice->priority == victim->priority && voice->volume < victim->volume);
            if (voice->music == victim->music && lower) {
                victim = voice.get();
            }
        }
        if (playing < maxVoices) {
            return;
        }
        fadeOut(*victim, 0.0F);
    }
}

} // namespace haylen::audio
