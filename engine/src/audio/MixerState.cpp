#include "audio/MixerState.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/core/FrameClock.hpp"

namespace haylen::audio {

MixerState::Voice::~Voice() {
    if (handleReady) {
        ma_sound_uninit(&handle);
    }
    if (sourceReady && decoderSource) {
        ma_decoder_uninit(&decoder);
    } else if (sourceReady) {
        ma_audio_buffer_uninit(&buffer);
    }
}

MixerState::~MixerState() {
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
        throw std::invalid_argument("Unknown audio bus: " + std::string(name));
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
        throw std::runtime_error("The audio bus " + name + " could not be created.");
    }
    created->effects = std::make_unique<EffectChain>(engine, &created->group, parent != nullptr ? &parent->group : ma_engine_get_endpoint(&engine));
    buses.emplace(name, std::move(created));
    busOrder.push_back(name);
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
        ma_sound_stop(&voice.handle);
        return;
    }

    // Starting a sound at its end would replay it, so a voice that finished while it was held stays finished.
    if (ma_sound_at_end(&voice.handle) == MA_FALSE) {
        ma_sound_start(&voice.handle);
    }
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

void MixerState::makeRoom() {
    if (voices.size() < maxVoices) {
        return;
    }
    auto oldest = std::find_if(voices.begin(), voices.end(), [](const std::unique_ptr<Voice>& voice) { return !voice->music; });
    if (oldest == voices.end()) {
        oldest = voices.begin();
    }
    retire(**oldest);
    voices.erase(oldest);
}

} // namespace haylen::audio
