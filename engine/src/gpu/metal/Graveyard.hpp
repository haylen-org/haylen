#pragma once

#import <Foundation/Foundation.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace haylen::gpu::metal {

// Keeps the Metal objects that the device let go of until the GPU finished every frame that may use them, since the command buffers of the device do not retain what they reference.
class Graveyard final {
  public:
    // Starts with the number of the first frame of the device, which every object retired before that frame commits waits for.
    explicit Graveyard(std::uint64_t firstFrame) noexcept : frame(firstFrame) {}

    // The number of the frame that records now, or that records next between frames, which every object retired from now on waits for.
    void setFrame(std::uint64_t value) noexcept {
        frame = value;
    }

    // Holds room for one more entry for each of the objects that live, which every method that makes an object calls before it makes one, so `retire` never allocates.
    void reserve(std::size_t living);

    void retire(id object) noexcept;

    // Releases the objects that wait for frames up to the one the GPU finished last.
    void release(std::uint64_t completed);

  private:
    struct Entry {
        id object = nil;
        std::uint64_t frame = 0;
    };

    // The entries in the order of their frames.
    std::vector<Entry> entries;
    std::uint64_t frame;
};

} // namespace haylen::gpu::metal
