#import <Foundation/Foundation.h>

#include <gtest/gtest.h>

#include <vector>

#include "gpu/metal/Graveyard.hpp"
#include "support/AllocationTracker.hpp"

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

// The device holds the place of every object it made, so the destroys that retire those objects allocate nothing.
TEST(GraveyardTest, RetiresTheObjectsItHoldsRoomForWithoutAllocating) {
    Graveyard graveyard(1);
    std::vector<NSObject*> objects;
    for (int object = 0; object < 64; ++object) {
        graveyard.reserve(objects.size() + 1);
        objects.push_back([[NSObject alloc] init]);
    }

    const test::AllocationTracker tracker;
    for (NSObject* object : objects) {
        graveyard.retire(object);
    }

    EXPECT_EQ(tracker.getTotal(), 0U);
}

} // namespace haylen::gpu::metal
