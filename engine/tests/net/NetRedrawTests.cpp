#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "gpu/NullDevice.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/net/WebSocket.hpp"
#include "haylen/plugins/NetPlugin.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "net/WebSocketFixture.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestApplication.hpp"

namespace haylen::net {

// An app that redraws on change keeps updating while a WebSocket has bytes that wait to be written, so an upload that sends no faster than the connection carries finishes without another change, and the app rests once nothing waits.
class NetRedrawTest : public WebSocketFixture {
  protected:
    static constexpr std::size_t kChunkSize = 1024 * 1024;
    static constexpr int kChunks = 16;

    // Sends the chunks of an upload from the update of the app, the next one whenever fewer bytes than two chunks wait to be written.
    class Upload final : public plugins::Plugin {
      public:
        explicit Upload(std::string address) : url(std::move(address)) {}

        [[nodiscard]] std::string_view getName() const noexcept override {
            return "upload";
        }
        void start(core::Engine& engine) override {
            socket = engine.getPlugin<plugins::NetPlugin>().connectWebSocket(url);
        }
        void update(core::Engine&, float) override {
            if (socket->getState() != WebSocket::State::Open || sent == kChunks || socket->getBufferedAmount() >= 2 * kChunkSize) {
                return;
            }

            socket->send(chunk);
            ++sent;
        }

      private:
        std::string url;
        std::string chunk = std::string(kChunkSize, 'u');
        std::shared_ptr<WebSocket> socket;
        int sent = 0;
    };

    // Runs frames and returns how many of them the device committed.
    [[nodiscard]] std::uint64_t drawn(int count) {
        const std::uint64_t before = app.host().getGpu().getCommittedFrames();
        app.frames(count);
        return app.host().getGpu().getCommittedFrames() - before;
    }

    EchoServer server;
    // clang-format off
    test::EngineFixture app{{{"haylen.json", R"({"name": "Test App", "identifier": "com.haylen.tests", "window": {"redraw": "onChange"}})"}}, std::make_unique<test::TestApplication>([this](core::Engine& engine) {
        engine.addPlugin(std::make_unique<Upload>(server.getUrl("/sink")));
    })};
    // clang-format on
};

TEST_F(NetRedrawTest, FinishesAnUploadWithoutAnotherChange) {
    ASSERT_TRUE(app.frameUntil([this] { return server.getReceivedBytes() == kChunkSize * kChunks; }, std::chrono::seconds(30)));

    // The last bytes leave the count at the next pump, and then nothing waits, so the frames rest.
    app.frames(10);
    EXPECT_EQ(drawn(30), 0U);
}

} // namespace haylen::net
