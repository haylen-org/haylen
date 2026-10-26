#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include "haylen/core/Log.hpp"
#include "haylen/gpu/Device.hpp"
#include "support/OffscreenGlContext.hpp"

namespace haylen::gpu::opengl {

// Runs a test on each flavour of OpenGL this machine makes a context of, skipping the others with the reason, and fails it when OpenGL reported an error meanwhile.
class OpenGlTest : public ::testing::TestWithParam<Device::Api> {
  public:
    [[nodiscard]] static std::string getFlavourName(const ::testing::TestParamInfo<Device::Api>& info) {
        return info.param == Device::Api::OpenGl ? "OpenGl" : "OpenGlEs";
    }

  protected:
    static constexpr int kSize = 64;

    void SetUp() override {
        try {
            context = std::make_unique<test::OffscreenGlContext>(GetParam(), kSize, kSize);
        } catch (const std::runtime_error& error) {
            GTEST_SKIP() << error.what();
        }
        // clang-format off
        listener = core::Log::addListener([this](core::Log::Level level, std::string_view) {
            if (level == core::Log::Level::Error) {
                ++errors;
            }
        });
        // clang-format on
    }

    void TearDown() override {
        if (context == nullptr) {
            return;
        }
        core::Log::removeListener(listener);
        EXPECT_EQ(errors, 0) << "OpenGL reported errors during the test.";
    }

    std::unique_ptr<test::OffscreenGlContext> context;

  private:
    std::uint64_t listener = 0;
    int errors = 0;
};

} // namespace haylen::gpu::opengl
