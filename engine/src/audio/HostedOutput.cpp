#include "audio/HostedOutput.hpp"

#include <exception>
#include <utility>

namespace haylen::audio {

HostedOutput::HostedOutput(std::shared_ptr<port::AudioOutput> value, OutputSettings::Latency outputLatency) noexcept : output(std::move(value)), latency(outputLatency) {}

void HostedOutput::configure(ma_context_config& config) noexcept {
    config.custom.onContextInit = &HostedOutput::initContext;
    config.pUserData = this;
}

bool HostedOutput::isLocked() const noexcept {
    return playback != nullptr && playback->isLocked();
}

std::optional<std::string> HostedOutput::getFailure() const {
    return playback != nullptr ? playback->getFailure() : std::nullopt;
}

void HostedOutput::DeviceStream::render(std::span<float> samples) noexcept {
    const ma_uint32 frames = static_cast<ma_uint32>(samples.size() / device->playback.internalChannels);
    ma_device_handle_backend_data_callback(device, samples.data(), nullptr, frames);
}

ma_result HostedOutput::initContext(ma_context*, const ma_context_config*, ma_backend_callbacks* callbacks) {
    callbacks->onDeviceInit = &initDevice;
    callbacks->onDeviceUninit = &uninitDevice;
    callbacks->onDeviceStart = &startDevice;
    callbacks->onDeviceStop = &stopDevice;
    return MA_SUCCESS;
}

// The playback plays in the format it reports, and the device converts the mix of the engine to it.
ma_result HostedOutput::initDevice(ma_device* device, const ma_device_config* config, ma_device_descriptor* descriptor, ma_device_descriptor*) {
    if (config->deviceType != ma_device_type_playback) {
        return MA_DEVICE_TYPE_NOT_SUPPORTED;
    }

    HostedOutput& self = from(device);
    self.stream.device = device;
    try {
        self.playback = self.output->open({.sampleRate = descriptor->sampleRate, .channels = descriptor->channels, .latency = self.latency}, self.stream);
    } catch (const std::exception& error) {
        self.refusal = error.what();
        return MA_ERROR;
    }

    const port::AudioOutput::Format format = self.playback->getFormat();
    descriptor->format = ma_format_f32;
    descriptor->sampleRate = format.sampleRate;
    descriptor->channels = format.channels;
    ma_channel_map_init_standard(ma_standard_channel_map_default, descriptor->channelMap, MA_MAX_CHANNELS, format.channels);
    descriptor->periodSizeInFrames = format.periodFrames;
    descriptor->periodCount = format.periods;
    return MA_SUCCESS;
}

ma_result HostedOutput::uninitDevice(ma_device* device) {
    from(device).playback.reset();
    return MA_SUCCESS;
}

ma_result HostedOutput::startDevice(ma_device* device) {
    from(device).playback->start();
    return MA_SUCCESS;
}

ma_result HostedOutput::stopDevice(ma_device* device) {
    from(device).playback->stop();
    return MA_SUCCESS;
}

HostedOutput& HostedOutput::from(ma_device* device) noexcept {
    return *static_cast<HostedOutput*>(device->pContext->pUserData);
}

} // namespace haylen::audio
