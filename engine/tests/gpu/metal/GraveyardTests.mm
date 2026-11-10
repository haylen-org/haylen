#import <Foundation/Foundation.h>

#include <gtest/gtest.h>

#include "gpu/metal/Graveyard.hpp"

namespace haylen::gpu::metal {

// An object that the first frame lets go of lives until the GPU finished that frame, since the count of finished frames is 0 while it runs.
TEST(GraveyardTest, KeepsWhatTheFirstFrameRetiresUntilThatFrameFinishes) {
    Graveyard graveyard(1);
    __weak NSObject* watched = nil;
    @autoreleasepool {
        NSObject* object = [[NSObject alloc] init];
        watched = object;
        graveyard.retire(object);
    }

    graveyard.release(0);
    EXPECT_NE(watched, nil);

    graveyard.release(1);
    EXPECT_EQ(watched, nil);
}

} // namespace haylen::gpu::metal
