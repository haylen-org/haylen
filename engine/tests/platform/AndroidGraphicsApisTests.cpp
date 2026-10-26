#include <gtest/gtest.h>

#include "platform/android/AndroidGraphicsApis.hpp"

namespace haylen::platform {

using Api = gpu::Device::Api;

TEST(AndroidGraphicsApisTest, AnApiThatFailedToStartIsNotTriedAgain) {
    AndroidGraphicsApis apis;
    apis.setOrder({Api::Vulkan, Api::OpenGlEs});
    apis.fail(Api::Vulkan);
    apis.fail(Api::Vulkan);

    EXPECT_TRUE(apis.hasFailed(Api::Vulkan));
    EXPECT_FALSE(apis.hasFailed(Api::OpenGlEs));
    EXPECT_EQ(apis.getOrder(), (std::vector<Api>{Api::Vulkan, Api::OpenGlEs}));
}

TEST(AndroidGraphicsApisTest, TheDeviceAfterALossStartsOnANewContextOnce) {
    AndroidGraphicsApis apis;
    apis.setOrder({Api::Vulkan, Api::OpenGlEs});
    EXPECT_FALSE(apis.takeRenewal(Api::OpenGlEs));

    apis.lose(Api::OpenGlEs);
    EXPECT_FALSE(apis.hasFailed(Api::OpenGlEs));
    EXPECT_FALSE(apis.takeRenewal(Api::Vulkan));
    EXPECT_TRUE(apis.takeRenewal(Api::OpenGlEs));
    EXPECT_FALSE(apis.takeRenewal(Api::OpenGlEs));
}

} // namespace haylen::platform
