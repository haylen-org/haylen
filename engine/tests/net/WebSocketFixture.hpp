#pragma once

#include <gtest/gtest.h>

#include <Poco/Buffer.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Timespan.h>

#include <chrono>
#include <functional>
#include <string>
#include <thread>

#include "haylen/net/WebSocket.hpp"

namespace haylen::net {

// Runs a WebSocket server that echoes on this machine and waits for the connections of the tests, which work on threads of their own.
class WebSocketFixture : public ::testing::Test {
  protected:
    // A server reads each frame and serves for a bounded time, so a test that hangs ends instead of keeping its threads.
    static constexpr long kReceiveSeconds = 30;
    static constexpr std::chrono::seconds kServeLimit{60};

    class EchoServer final {
      public:
        EchoServer() : listener(Poco::Net::SocketAddress("127.0.0.1", 0)), server(new Factory(), listener, new Poco::Net::HTTPServerParams()) {
            server.start();
        }
        ~EchoServer() {
            server.stopAll(true);
        }

        EchoServer(const EchoServer&) = delete;
        EchoServer& operator=(const EchoServer&) = delete;

        [[nodiscard]] std::string getUrl(const std::string& path = "/echo") const {
            return "ws://127.0.0.1:" + std::to_string(listener.address().port()) + path;
        }

      private:
        // Echoes every message, answers pings with pongs and understands a few commands: `fragments` answers in two frames, `ping-me` pings the client and reports its pong, `bye` closes from the server side and `drop` hangs up without a close frame. The path `/refuse` answers the upgrade request with 403 instead.
        class Handler final : public Poco::Net::HTTPRequestHandler {
          public:
            void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override {
                if (request.getURI() == "/refuse") {
                    response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
                    response.setContentLength(0);
                    response.send();
                    return;
                }

                if (request.get("Sec-WebSocket-Protocol", "").find("chat") != std::string::npos) {
                    response.set("Sec-WebSocket-Protocol", "chat");
                }

                Poco::Net::WebSocket socket(request, response);
                socket.setReceiveTimeout(Poco::Timespan(kReceiveSeconds, 0));
                Poco::Buffer<char> frame(0);
                while (true) {
                    int flags = 0;
                    frame.resize(0);
                    const int length = socket.receiveFrame(frame, flags);
                    const int opcode = flags & Poco::Net::WebSocket::FRAME_OP_BITMASK;
                    const std::string text(frame.begin(), frame.size());

                    if ((length == 0 && flags == 0) || opcode == Poco::Net::WebSocket::FRAME_OP_CLOSE) {
                        socket.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
                        return;
                    }

                    if (opcode == Poco::Net::WebSocket::FRAME_OP_PONG) {
                        send(socket, "pong " + text);
                    } else if (opcode == Poco::Net::WebSocket::FRAME_OP_PING) {
                        socket.sendFrame(frame.begin(), length, static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_PONG));
                    } else if (text == "fragments") {
                        socket.sendFrame("hel", 3, Poco::Net::WebSocket::FRAME_OP_TEXT);
                        socket.sendFrame("lo", 2, static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_CONT));
                    } else if (text == "ping-me") {
                        socket.sendFrame("beat", 4, static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_PING));
                    } else if (text == "bye") {
                        socket.shutdown(4001, "server bye");
                        socket.receiveFrame(frame, flags);
                        return;
                    } else if (text == "drop") {
                        return;
                    } else {
                        socket.sendFrame(frame.begin(), length, opcode == Poco::Net::WebSocket::FRAME_OP_BINARY ? Poco::Net::WebSocket::FRAME_BINARY : Poco::Net::WebSocket::FRAME_TEXT);
                    }
                }
            }

          private:
            static void send(Poco::Net::WebSocket& socket, const std::string& text) {
                socket.sendFrame(text.data(), static_cast<int>(text.size()), Poco::Net::WebSocket::FRAME_TEXT);
            }
        };

        class Factory final : public Poco::Net::HTTPRequestHandlerFactory {
          public:
            Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override {
                return new Handler();
            }
        };

        Poco::Net::ServerSocket listener;
        Poco::Net::HTTPServer server;
    };

    // Pumps until the condition holds, since the connection works on a thread of its own.
    static bool pumpUntil(WebSocket& socket, const std::function<bool()>& done) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline) {
            socket.pump();

            if (done()) {
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        return false;
    }

    static bool waitUntil(const std::function<bool()>& done, std::chrono::milliseconds timeout = std::chrono::seconds(10)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!done() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        return done();
    }

    // Returns an address where nothing listens, so every connection attempt fails at once.
    static std::string refusedUrl() {
        Poco::Net::ServerSocket unused(Poco::Net::SocketAddress("127.0.0.1", 0));
        const std::string url = "ws://127.0.0.1:" + std::to_string(unused.address().port());
        unused.close();
        return url;
    }
};

} // namespace haylen::net
