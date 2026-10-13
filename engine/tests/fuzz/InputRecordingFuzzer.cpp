#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

#include "haylen/input/InputRecording.hpp"

// The entry point libFuzzer calls with each input it generates, which must either decode into a recording that encodes again or fail with the error of a malformed recording.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    try {
        const haylen::input::InputRecording recording = haylen::input::InputRecording::fromBytes(std::as_bytes(std::span(data, size)));
        (void)recording.toBytes();
    } catch (const std::invalid_argument&) {}
    return 0;
}
