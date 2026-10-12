#pragma once

#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "haylen/content/Error.hpp"

namespace varn::runtime {
class Runtime;
}

namespace varn::http::client {
class HttpClient;
}

namespace haylen::content {

// Downloads the immutable files that serve protected content, whole or by byte range, through the streaming HTTP client of Varn, so no response is ever held whole. Every response is checked before a byte reaches the caller: its status, the range the server sent and the length. A server that ignores ranges never sends a whole shard where a range was asked for, and a download that resumes appends only to the same file, which the validator of its first response names. Callbacks run on the thread that polls the event loop of the runtime.
class HttpTransport final {
  public:
    struct Request {
        std::string url;

        // The first byte and the length of a range, or the whole file without a size, which may hold at most `maximumSize` bytes.
        std::uint64_t offset = 0;
        std::optional<std::uint64_t> size;
        std::uint64_t maximumSize = std::uint64_t{64} << 20;

        // A download that resumes names the bytes of the range it holds already and the validator that the response that sent them had.
        std::uint64_t held = 0;
        std::string validator;

        // The deadline of the whole request, which by default grows with the bytes it asks for.
        std::optional<std::chrono::milliseconds> timeout;
    };

    // What the server answered before the body: whether it continues the bytes held or sends the range again from its start, the validator of the file, empty when the server named none that resuming can use, and the size of the whole file when the server said it.
    struct Start {
        bool resumed = false;
        std::string validator;
        std::optional<std::uint64_t> fileSize;
    };

    struct Callbacks {
        std::function<void(const Start&)> onStart;
        std::function<void(std::span<const std::uint8_t>)> onBytes;

        // Runs exactly once, with no error when every byte arrived, and nothing runs after it.
        std::function<void(std::optional<Error>)> onComplete;
    };

    // A download that runs, which any thread may cancel. Cancelling ends it with `NetworkCancelled`.
    class Transfer final {
      public:
        Transfer() = default;

        void cancel() const;
        [[nodiscard]] bool isActive() const;

      private:
        friend class HttpTransport;
        struct State;

        explicit Transfer(std::shared_ptr<State> transferState) : state(std::move(transferState)) {}

        std::shared_ptr<State> state;
    };

    explicit HttpTransport(varn::runtime::Runtime& runtime);
    ~HttpTransport();

    HttpTransport(const HttpTransport&) = delete;
    HttpTransport& operator=(const HttpTransport&) = delete;

    Transfer fetch(Request request, Callbacks callbacks);

    // One deadline covers a whole request, so it grows with the bytes asked for, enough for a slow but working connection.
    [[nodiscard]] static std::chrono::milliseconds getTimeout(std::uint64_t bytes) noexcept;

  private:
    static constexpr std::chrono::seconds kBaseTimeout{30};
    static constexpr std::uint64_t kSlowestBytesPerSecond = 32 * 1024;

    void send(const std::shared_ptr<Transfer::State>& state);
    static void receiveHead(Transfer::State& state, int status, std::optional<std::uint64_t> first, std::optional<std::uint64_t> last, std::optional<std::uint64_t> fileSize, std::string validator);
    static void receiveBytes(Transfer::State& state, std::span<const std::uint8_t> bytes);

    // Ends the request with the failure that its completion reports, or with none when the request starts again.
    static void stop(Transfer::State& state, std::optional<Error> failure);
    void complete(const std::shared_ptr<Transfer::State>& state, std::optional<Error> failure);
    [[nodiscard]] static Error describeFailure(const Transfer::State& state, int code, const std::string& message);
    [[nodiscard]] static std::string readValidator(std::optional<std::string_view> etag);

    std::unique_ptr<varn::http::client::HttpClient> client;
};

} // namespace haylen::content
