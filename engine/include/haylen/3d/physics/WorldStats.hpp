#pragma once

#include <cstddef>

namespace haylen::physics3d {

// The counts of a 3D physics world and what its last step cost: its time, the threads it ran on and the most temporary memory any step of the world used, in bytes.
struct WorldStats {
    int bodies = 0;
    int activeBodies = 0;
    int stepThreads = 0;
    float stepMilliseconds = 0.0F;
    std::size_t temporaryMemoryPeak = 0;
};

} // namespace haylen::physics3d
