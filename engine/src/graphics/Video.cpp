#include "haylen/graphics/Video.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "haylen/core/JobSystem.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/platform/AudioStream.hpp"

namespace haylen::graphics {

using Frame = platform::VideoDecoder::Frame;
using Read = platform::VideoDecoder::Read;

// What the frame thread and the workers share. The decoder belongs to one worker at a time, which `busy` marks, and to the frame thread alone for a decoder with a clock of its own. A seek starts a new generation, and the work of an older generation goes away unused.
struct Video::Shared {
    std::mutex mutex;
    std::unique_ptr<platform::VideoDecoder> decoder;
    platform::VideoDecoder::Info info;
    bool opened = false;
    bool unsupported = false;
    std::string failure;

    std::vector<Frame> queued;
    std::vector<Frame> spare;
    std::uint64_t generation = 0;
    std::optional<double> seekTo;
    bool busy = false;
    bool framesEnded = false;
    bool soundEnded = false;

    // The ring the voice of the current generation plays, and the decoded sound that waits for room in it.
    std::shared_ptr<platform::AudioStream> sound;
    std::vector<float> samples;
};

Video::Video(core::JobSystem& jobSystem, Device& graphics, audio::Mixer& audioMixer, const Options& options) : jobs(jobSystem), device(graphics), mixer(audioMixer), shared(std::make_shared<Shared>()), bus(options.bus), processMode(options.processMode), volume(options.volume), rate(options.rate), muted(options.muted), loop(options.loop), playing(options.autoplay) {
    if (!(options.rate >= kMinimumRate && options.rate <= kMaximumRate)) {
        throw std::invalid_argument("The rate of a video must be from 0.25 to 4.");
    }
    if (!(options.volume >= 0.0F) || !std::isfinite(options.volume)) {
        throw std::invalid_argument("The volume of a video must be zero or positive.");
    }
    texture = device.createDynamicTexture(1, 1, math::Color::transparent(), {.filter = Texture::Filter::Linear});
}

Video::~Video() {
    close();
}

void Video::close() {
    playing = false;
    stopSound();
    const std::scoped_lock lock(shared->mutex);
    ++shared->generation;
    shared->busy = true;
}

// The worker hands the decoder over through the shared state, which the frame thread reads at its next update, so a video that goes away first leaves the decoder to the worker.
void Video::load(Opener opener) {
    // clang-format off
    jobs.postIo([target = std::weak_ptr<Shared>(shared), opener = std::move(opener)] {
        std::unique_ptr<platform::VideoDecoder> decoder;
        std::string failure;
        bool noDecoder = false;
        try {
            decoder = opener();
            noDecoder = decoder == nullptr;
            if (noDecoder) {
                failure = "This platform has no decoder of video files.";
            }
        } catch (const std::exception& exception) {
            failure = exception.what();
        }
        const std::shared_ptr<Shared> alive = target.lock();
        if (!alive) {
            return;
        }
        const std::scoped_lock lock(alive->mutex);
        if (decoder) {
            alive->info = decoder->getInfo();
        }
        alive->decoder = std::move(decoder);
        alive->failure = std::move(failure);
        alive->unsupported = noDecoder;
        alive->opened = true;
    });
    // clang-format on
}

void Video::play() {
    if (atEnd) {
        seek(0.0);
    }
    playing = true;
    if (state != State::Ready) {
        return;
    }
    if (voice == 0) {
        restartSound();
    }
    mixer.setPaused(voice, false);
}

void Video::pause() {
    playing = false;
    if (voice != 0) {
        mixer.setPaused(voice, true);
    }
}

void Video::seek(double seconds) {
    if (!std::isfinite(seconds)) {
        throw std::invalid_argument("A video seeks to a finite number of seconds.");
    }
    const double duration = getDuration();
    time = std::clamp(seconds, 0.0, duration > 0.0 ? duration : seconds);
    atEnd = false;
    if (state == State::Ready && shared->info.clocked) {
        shared->decoder->seek(time);
        controlledRate = 0.0F;
        return;
    }
    {
        const std::scoped_lock lock(shared->mutex);
        ++shared->generation;
        shared->seekTo = time;
        shared->framesEnded = false;
        shared->soundEnded = false;
        for (Frame& frame : shared->queued) {
            shared->spare.push_back(std::move(frame));
        }
        shared->queued.clear();
    }
    if (state == State::Ready) {
        restartSound();
    }
}

void Video::setRate(float value) {
    if (!(value >= kMinimumRate && value <= kMaximumRate)) {
        throw std::invalid_argument("The rate of a video must be from 0.25 to 4.");
    }
    rate = value;
    if (voice != 0) {
        mixer.setPitch(voice, rate);
    }
}

void Video::setVolume(float value) {
    if (!(value >= 0.0F) || !std::isfinite(value)) {
        throw std::invalid_argument("The volume of a video must be zero or positive.");
    }
    volume = value;
    if (voice != 0) {
        mixer.setVolume(voice, muted ? 0.0F : volume);
    }
}

void Video::setMuted(bool value) {
    muted = value;
    if (voice != 0) {
        mixer.setVolume(voice, muted ? 0.0F : volume);
    }
}

double Video::getDuration() const noexcept {
    return state == State::Ready ? shared->info.duration : 0.0;
}

int Video::getWidth() const noexcept {
    return state == State::Ready ? shared->info.width : 0;
}

int Video::getHeight() const noexcept {
    return state == State::Ready ? shared->info.height : 0;
}

void Video::update(double seconds, bool running) {
    if (state == State::Loading) {
        std::unique_lock lock(shared->mutex);
        if (!shared->opened) {
            return;
        }
        std::string failure = shared->failure;
        lock.unlock();

        // A decoder with a clock of its own learns its size after it opens, and a read of a frame meanwhile reports its error.
        if (failure.empty() && shared->decoder->getInfo().clocked) {
            try {
                if (shared->decoder->getInfo().width == 0) {
                    Frame& probe = shared->spare.empty() ? shared->spare.emplace_back() : shared->spare.back();
                    (void)shared->decoder->readFrame(probe);
                    return;
                }
                shared->info = shared->decoder->getInfo();
            } catch (const std::exception& exception) {
                failure = exception.what();
            }
        }
        finishOpening(std::move(failure));
    }
    if (state != State::Ready) {
        return;
    }
    if (shared->info.clocked) {
        try {
            updateClocked();
        } catch (const std::exception& exception) {
            error = exception.what();
            fail();
        }
        return;
    }

    std::optional<Frame> shown;
    {
        const std::scoped_lock lock(shared->mutex);
        if (!shared->failure.empty()) {
            error = shared->failure;
        }
    }
    if (!error.empty()) {
        fail();
        return;
    }
    // The clock stops at the end of the video, where it waits for the decoder to deliver the last frame, so the time never depends on how fast the decoder runs.
    if (playing && running) {
        time += seconds * rate;
        if (shared->info.duration > 0.0) {
            time = std::min(time, shared->info.duration);
        }
    }

    // The newest frame whose time came shows, and the frames it passed go back to the spare frames.
    bool exhausted = false;
    {
        const std::scoped_lock lock(shared->mutex);
        std::size_t passed = 0;
        while (passed < shared->queued.size() && shared->queued[passed].time <= time) {
            ++passed;
        }
        if (passed > 0) {
            for (std::size_t index = 0; index + 1 < passed; ++index) {
                shared->spare.push_back(std::move(shared->queued[index]));
            }
            shown = std::move(shared->queued[passed - 1]);
            shared->queued.erase(shared->queued.begin(), shared->queued.begin() + static_cast<std::ptrdiff_t>(passed));
        }
        exhausted = shared->framesEnded && shared->queued.empty();
    }
    if (shown) {
        present(*shown);
        const std::scoped_lock lock(shared->mutex);
        shared->spare.push_back(std::move(*shown));
    }
    if (exhausted && !atEnd && time >= shared->info.duration) {
        reachEnd();
    }
    schedule();
}

void Video::finishOpening(std::string failure) {
    if (!failure.empty()) {
        error = std::move(failure);
        unsupported = shared->unsupported;
        fail();
        return;
    }
    const platform::VideoDecoder::Info& info = shared->info;
    state = State::Ready;
    device.replaceTexture(texture, Image(std::max(info.width, 1), std::max(info.height, 1), std::vector<std::uint8_t>(static_cast<std::size_t>(std::max(info.width, 1)) * static_cast<std::size_t>(std::max(info.height, 1)) * 4U, 0)));
    if (time > 0.0) {
        seek(time);
    }
    if (playing) {
        restartSound();
    }
    ready.emit();
    if (!info.clocked) {
        schedule();
    }
}

void Video::fail() {
    state = State::Failed;
    playing = false;
    stopSound();
    failed.emit(error);
}

void Video::present(Frame& frame) {
    if (frame.width == texture.getWidth() && frame.height == texture.getHeight()) {
        device.updateTexture(texture, frame.pixels);
    } else {
        device.replaceTexture(texture, Image(frame.width, frame.height, frame.pixels));
    }
    ++frameCount;
}

// A decoder with a clock of its own plays by itself, so the video hands it what changed and follows its time, its newest frame and its sound.
void Video::updateClocked() {
    platform::VideoDecoder& decoder = *shared->decoder;
    if (playing != controlledPlaying || rate != controlledRate) {
        controlledPlaying = playing;
        controlledRate = rate;
        decoder.control(playing, rate);
    }
    time = decoder.getTime();
    Frame& frame = shared->spare.empty() ? shared->spare.emplace_back() : shared->spare.back();
    const Read read = decoder.readFrame(frame);
    if (read == Read::Frame) {
        present(frame);
    }
    if (shared->info.channels > 0 && shared->sound) {
        (void)decoder.readAudio(shared->samples, time);
        pushSound(*shared);
    }
    if (read == Read::End && playing) {
        reachEnd();
    }
}

void Video::reachEnd() {
    if (loop) {
        seek(0.0);
        return;
    }
    playing = false;
    atEnd = true;
    if (voice != 0) {
        mixer.setPaused(voice, true);
    }
    ended.emit();
}

// Every seek plays the sound of the new time from a new ring and a new voice, which never hears the samples of the old time.
void Video::restartSound() {
    stopSound();
    const platform::VideoDecoder::Info& info = shared->info;
    if (info.channels == 0 || info.sampleRate == 0) {
        return;
    }
    auto stream = std::make_shared<platform::AudioStream>(info.sampleRate, info.channels, platform::AudioStream::Format::Float32, info.sampleRate);
    {
        const std::scoped_lock lock(shared->mutex);
        shared->sound = stream;
    }
    voice = mixer.play(stream, {.bus = bus, .volume = muted ? 0.0F : volume, .pitch = rate, .processMode = processMode});
    if (!playing) {
        mixer.setPaused(voice, true);
    }
}

void Video::stopSound() {
    if (voice != 0) {
        mixer.stop(voice);
        voice = 0;
    }
    const std::scoped_lock lock(shared->mutex);
    shared->sound.reset();
}

void Video::schedule() {
    {
        const std::scoped_lock lock(shared->mutex);
        if (shared->busy || shared->framesEnded || shared->queued.size() >= kFramesAhead) {
            return;
        }
        shared->busy = true;
    }
    jobs.post([target = shared] { decode(target); });
}

// The sound of a frame decodes with it, a little ahead of its time, so the ring never runs dry while frames wait. Exceptions of the decoder fail the video at the next update.
void Video::decode(const std::shared_ptr<Shared>& shared) {
    Shared& state = *shared;
    // The end of the sound belongs to a generation, so a seek that starts a new one meanwhile never hears the end of the old one.
    std::uint64_t generation = 0;
    std::optional<double> target;
    bool soundEnded = false;
    {
        const std::scoped_lock lock(state.mutex);
        generation = state.generation;
        target = std::exchange(state.seekTo, std::nullopt);
        soundEnded = state.soundEnded;
    }
    const bool sounded = state.info.channels > 0;
    try {
        if (target) {
            state.decoder->seek(*target);
            state.samples.clear();
        }
        while (true) {
            Frame frame;
            {
                const std::scoped_lock lock(state.mutex);
                if (state.generation != generation || state.queued.size() >= kFramesAhead) {
                    break;
                }
                if (!state.spare.empty()) {
                    frame = std::move(state.spare.back());
                    state.spare.pop_back();
                }
            }
            const Read read = state.decoder->readFrame(frame);
            const bool last = read != Read::Frame;
            if (sounded && !soundEnded) {
                soundEnded = !state.decoder->readAudio(state.samples, last ? state.info.duration : frame.time + kSoundAhead);
            }

            const std::scoped_lock lock(state.mutex);
            if (state.generation != generation) {
                state.spare.push_back(std::move(frame));
                break;
            }
            state.soundEnded = soundEnded;
            pushSound(state);
            if (last) {
                state.framesEnded = true;
                state.spare.push_back(std::move(frame));
                break;
            }
            state.queued.push_back(std::move(frame));
        }
    } catch (const std::exception& exception) {
        const std::scoped_lock lock(state.mutex);
        state.failure = exception.what();
    }
    const std::scoped_lock lock(state.mutex);
    state.busy = false;
}

// Pushes as much of the decoded sound as the ring takes, in whole frames, and keeps the rest for later.
void Video::pushSound(Shared& shared) {
    if (!shared.sound || shared.samples.empty()) {
        return;
    }
    const std::size_t channels = shared.info.channels;
    const std::size_t whole = shared.samples.size() / channels * channels;
    const std::size_t taken = shared.sound->push(std::span<const float>(shared.samples.data(), whole)) * channels;
    shared.samples.erase(shared.samples.begin(), shared.samples.begin() + static_cast<std::ptrdiff_t>(taken));
}

} // namespace haylen::graphics
