#include "support/FakeVideoDecoder.hpp"

#include <cmath>
#include <format>
#include <stdexcept>
#include <utility>

namespace haylen::test {

FakeVideoDecoder::FakeVideoDecoder(Settings value, std::shared_ptr<Log> target) : settings(std::move(value)), log(std::move(target)) {}

const platform::VideoDecoder::Info& FakeVideoDecoder::getInfo() const noexcept {
    const std::scoped_lock lock(log->mutex);
    shown = settings.info;
    if (!log->sized) {
        shown.width = 0;
        shown.height = 0;
    }
    return shown;
}

platform::VideoDecoder::Read FakeVideoDecoder::readFrame(Frame& frame) {
    {
        std::unique_lock lock(log->mutex);
        log->readingThreads.insert(std::this_thread::get_id());
        log->released.wait(lock, [this] { return !log->held; });
    }
    if (settings.info.clocked) {
        const std::scoped_lock lock(log->mutex);
        if (!log->error.empty()) {
            throw std::runtime_error(log->error);
        }
        if (!log->frameShown) {
            return Read::Waiting;
        }
        log->frameShown = false;
    }
    const double time = next / settings.frameRate;
    if (!settings.info.clocked && time >= settings.info.duration) {
        return Read::End;
    }
    if (settings.failAt && next == *settings.failAt) {
        throw std::runtime_error("The fake video broke at its frame " + std::to_string(next) + ".");
    }
    frame.width = settings.info.width;
    frame.height = settings.info.height;
    frame.time = settings.info.clocked ? getTime() : time;
    frame.pixels.assign(static_cast<std::size_t>(frame.width * frame.height) * 4U, 255);
    for (std::size_t pixel = 0; pixel < frame.pixels.size(); pixel += 4) {
        frame.pixels[pixel] = static_cast<std::uint8_t>(next);
    }
    ++next;
    const std::scoped_lock lock(log->mutex);
    ++log->framesRead;
    return Read::Frame;
}

bool FakeVideoDecoder::readAudio(std::vector<float>& samples, double until) {
    const double end = std::min(until, settings.info.duration);
    // The frames count up to the end, so a time that rounding puts a hair short of it still reaches it.
    const auto frames = static_cast<std::size_t>(std::max(0.0, std::floor((end - soundTime) * settings.info.sampleRate + 1e-6)));
    samples.insert(samples.end(), frames * settings.info.channels, settings.sample);
    soundTime += static_cast<double>(frames) / settings.info.sampleRate;
    const std::scoped_lock lock(log->mutex);
    log->soundTime = soundTime;
    return soundTime < settings.info.duration;
}

void FakeVideoDecoder::seek(double seconds) {
    next = static_cast<int>(std::floor(seconds * settings.frameRate));
    soundTime = seconds;
    const std::scoped_lock lock(log->mutex);
    log->seeks.push_back(seconds);
    log->clock = seconds;
}

void FakeVideoDecoder::control(bool playing, double rate) {
    const std::scoped_lock lock(log->mutex);
    log->controls.push_back(std::format("{} {}", playing ? "play" : "pause", rate));
}

double FakeVideoDecoder::getTime() const {
    const std::scoped_lock lock(log->mutex);
    return log->clock;
}

} // namespace haylen::test
