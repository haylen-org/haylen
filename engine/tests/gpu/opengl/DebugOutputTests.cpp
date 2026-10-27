#include <gtest/gtest.h>

#include <string_view>

#include "gpu/opengl/Context.hpp"
#include "gpu/opengl/DebugOutput.hpp"
#include "gpu/opengl/OpenGlTest.hpp"

namespace haylen::gpu::opengl {

// Hands the messages of a driver to the debug output through the callback it installs, which the test takes in place of the one of the context.
class DebugOutputTest : public OpenGlTest {
  protected:
    static constexpr Gl::Enum kDebugSourceApi = 0x8246;
    static constexpr Gl::Enum kDebugTypePerformance = 0x8250;
    static constexpr Gl::Enum kDebugSeverityHigh = 0x9146;

    void SetUp() override {
        OpenGlTest::SetUp();
        if (IsSkipped()) {
            return;
        }
        opened.gl.load(*context, GetParam());
        if (!opened.gl.getFeatures().debug) {
            GTEST_SKIP() << "The context has no debug output.";
        }
        installed = opened.gl.debugMessageCallback;
        opened.gl.debugMessageCallback = &DebugOutputTest::take;
    }

    void TearDown() override {
        if (installed != nullptr) {
            opened.gl.debugMessageCallback = installed;
        }
        OpenGlTest::TearDown();
    }

    static void take(Gl::DebugProc callback, const void*) {
        taken = callback;
    }

    static inline Gl::DebugProc taken = nullptr;
    Context opened;
    decltype(Gl::debugMessageCallback) installed = nullptr;
};

// The Adreno driver marks its notes about how it copies pixels through buffers as of high severity, and a note about performance never reaches the errors that fail the test, whatever its severity.
TEST_P(DebugOutputTest, KeepsNotesAboutPerformanceOutOfTheErrors) {
    const DebugOutput output(opened.gl, true);
    ASSERT_NE(taken, nullptr);
    const std::string_view note = "CPU path taken to copy PBO";
    taken(kDebugSourceApi, kDebugTypePerformance, 0, kDebugSeverityHigh, static_cast<Gl::Sizei>(note.size()), note.data(), nullptr);
}

INSTANTIATE_TEST_SUITE_P(Flavours, DebugOutputTest, ::testing::Values(Device::Api::OpenGl, Device::Api::OpenGlEs), OpenGlTest::getFlavourName);

} // namespace haylen::gpu::opengl
