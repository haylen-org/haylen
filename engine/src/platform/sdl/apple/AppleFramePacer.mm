#import "platform/sdl/apple/AppleFramePacer.hpp"

#if TARGET_OS_OSX
#import "platform/sdl/apple/HaylenFramePacer.h"

namespace haylen::platform::sdl {

void AppleFramePacer::start(std::uint32_t wakeType) {
    [HaylenFramePacer.shared startWithWakeType:wakeType];
}

void AppleFramePacer::pace(int rate, int hiddenRate, bool hidden) {
    [HaylenFramePacer.shared paceAtRate:rate hiddenRate:hiddenRate hidden:hidden ? YES : NO];
}

void AppleFramePacer::followScreen() {
    [HaylenFramePacer.shared followScreen];
}

void AppleFramePacer::receiveWake() {
    [HaylenFramePacer.shared receiveWake];
}

bool AppleFramePacer::takeFrame() {
    return [HaylenFramePacer.shared takeFrame] == YES;
}

void AppleFramePacer::stop() {
    [HaylenFramePacer.shared stop];
}

} // namespace haylen::platform::sdl
#endif
