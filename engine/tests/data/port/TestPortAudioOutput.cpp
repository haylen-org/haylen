#include "TestPortAudioOutput.hpp"

namespace haylen::platform {

std::unique_ptr<port::AudioOutput::Playback> TestPortAudioOutput::open(const Request&, Stream& stream) {
    return std::make_unique<TestPortPlayback>(*this, stream);
}

void TestPortAudioOutput::play() {
    if (playing == nullptr) {
        return;
    }

    playing->render(samples);
    playedFrames += kPeriodFrames;
}

TestPortAudioOutput::TestPortPlayback::~TestPortPlayback() {
    stop();
}

port::AudioOutput::Format TestPortAudioOutput::TestPortPlayback::getFormat() const noexcept {
    return {.sampleRate = kSampleRate, .channels = kChannels, .periodFrames = kPeriodFrames, .periods = kPeriods};
}

void TestPortAudioOutput::TestPortPlayback::start() noexcept {
    owner.playing = this;
}

void TestPortAudioOutput::TestPortPlayback::stop() noexcept {
    if (owner.playing == this) {
        owner.playing = nullptr;
    }
}

bool TestPortAudioOutput::TestPortPlayback::isLocked() const noexcept {
    return false;
}

std::optional<std::string> TestPortAudioOutput::TestPortPlayback::getFailure() const {
    return std::nullopt;
}

void TestPortAudioOutput::TestPortPlayback::render(std::vector<float>& samples) noexcept {
    stream.render(samples);
}

} // namespace haylen::platform
