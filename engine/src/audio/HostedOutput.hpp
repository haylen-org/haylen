#pragma once

#include <miniaudio.h>

#include <memory>
#include <optional>
#include <span>
#include <string>

#include "haylen/audio/OutputSettings.hpp"
#include "haylen/port/AudioOutput.hpp"

namespace haylen::audio {

// One opening of the audio output that a host gives the mixer, as the custom backend of a miniaudio context, which holds it as its user data. The device of that context opens one playback of the output, reads the mix into it through the stream of the output and converts the mix to the format the playback reports.
class HostedOutput final {
  public:
    HostedOutput(std::shared_ptr<port::AudioOutput> value, OutputSettings::Latency outputLatency) noexcept;

    HostedOutput(const HostedOutput&) = delete;
    HostedOutput& operator=(const HostedOutput&) = delete;

    // Makes a context of the configuration open its devices through this output alone.
    void configure(ma_context_config& config) noexcept;

    // The sentence of the output that refused to open, empty while it did not refuse.
    [[nodiscard]] const std::string& getRefusal() const noexcept {
        return refusal;
    }

    [[nodiscard]] bool isLocked() const noexcept;
    [[nodiscard]] std::optional<std::string> getFailure() const;

  private:
    // The stream that a playback reads, which mixes into the samples it is given through the device it belongs to.
    class DeviceStream final : public port::AudioOutput::Stream {
      public:
        void render(std::span<float> samples) noexcept override;

        ma_device* device = nullptr;
    };

    static ma_result initContext(ma_context* context, const ma_context_config* config, ma_backend_callbacks* callbacks);
    static ma_result initDevice(ma_device* device, const ma_device_config* config, ma_device_descriptor* descriptor, ma_device_descriptor* capture);
    static ma_result uninitDevice(ma_device* device);
    static ma_result startDevice(ma_device* device);
    static ma_result stopDevice(ma_device* device);
    [[nodiscard]] static HostedOutput& from(ma_device* device) noexcept;

    std::shared_ptr<port::AudioOutput> output;
    OutputSettings::Latency latency;
    DeviceStream stream;
    std::unique_ptr<port::AudioOutput::Playback> playback;
    std::string refusal;
};

} // namespace haylen::audio
