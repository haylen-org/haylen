#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/core/Signal.hpp"

namespace haylen::platform {

// Request and event channel between the app and native code. Parameters, results and events are JSON with byte buffers next to it. Results, events and work that native code posts always reach the frame thread in pump. Call ids are unique in the whole process, so a reply that arrives after the app restarted never answers a call of the new app.
class Bridge final {
    struct Inbox;

  public:
    // How many retained events of one name wait for a listener. A newer one drops the oldest.
    static constexpr std::size_t kRetainedLimit = 32;

    // JSON with the byte buffers it refers to as {"$bytes": N}, so binary data such as images and audio never turns into text.
    struct Payload {
        core::Json json;
        std::vector<std::vector<std::byte>> buffers;
    };

    // Why a call failed. The code and the data are whatever native code sent, and null when it sent none. The bridge fails calls itself with the codes timeout and cancelled.
    struct Error {
        std::string message;
        core::Json code;
        core::Json data;
    };

    struct Result {
        bool ok = false;
        Payload value;
        Error error;
    };

    // How a native event reaches the listeners of its name. A retained event that nothing listens to waits for the first listener. The batched events of a name that arrive in one frame reach the listeners once, as one event whose JSON lists theirs in order, such as the readings of a sensor.
    struct EmitOptions {
        bool retain = false;
        bool batched = false;
    };

    // Queues work for the frame thread from any thread. The work runs in pump, and it is dropped once the bridge is gone, so it must not own Lua values.
    class Mailbox final {
      public:
        // Returns false when the bridge is gone and the work was dropped.
        bool post(std::function<void()> task) const;

      private:
        friend class Bridge;

        explicit Mailbox(std::weak_ptr<Inbox> target) : inbox(std::move(target)) {}

        std::weak_ptr<Inbox> inbox;
    };

    using Callback = std::function<void(Result)>;
    using Reply = std::function<void(Result)>;
    using Handler = std::function<void(const Payload& params, Reply reply)>;
    using Dispatcher = std::function<void(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers)>;
    using Canceller = std::function<void(std::uint64_t id, std::string_view method)>;

    // The dispatcher hands calls to native code, and the canceller tells native code that the app no longer waits for one.
    Bridge(Dispatcher native, Canceller cancel);

    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;

    // Handlers registered in C++ take precedence over the native dispatcher for their method. Their reply may come later from any thread, even after the bridge is gone.
    void registerHandler(std::string method, Handler handler);
    [[nodiscard]] bool hasHandler(std::string_view method) const;

    // A call with a timeout fails with the code timeout when no answer arrived in time, and native code hears that it was given up. Throws std::invalid_argument for an empty method name or parameters that refer to a buffer they lack.
    std::uint64_t call(std::string_view method, const Payload& params, Callback callback, std::optional<std::chrono::steady_clock::duration> timeout = std::nullopt);

    // Calls a method whose answer nobody needs. Nothing waits for it, it never counts as pending, and its answer is dropped.
    void send(std::string_view method, const Payload& params);

    // Fails a pending call with the code cancelled at the next pump and tells native code, and returns false when the call already settled.
    bool cancel(std::uint64_t id);

    core::Connection on(const std::string& event, std::function<void(const Payload&)> listener);

    // Thread-safe entry points for native code, which move the buffers into the queue of the bridge without copying them. A failed call carries a message string or an object with message, code and data. JSON that refers to a buffer it lacks fails the call with the code invalidBytes, and drops the event with an error in the log. An event that nothing listens to is dropped, unless it is retained: then it waits until a listener of its name connects, which receives the waiting events in order at the next pump.
    void resolve(std::uint64_t id, bool ok, std::string_view resultJson, std::vector<std::vector<std::byte>> buffers = {});
    void emit(std::string_view event, std::string_view payloadJson, std::vector<std::vector<std::byte>> buffers = {}, const EmitOptions& options = kDefaultEmitOptions);
    [[nodiscard]] Mailbox getMailbox() const;

    void pump();
    [[nodiscard]] std::size_t getPendingCallCount() const;

    // Reads an answer of native code the way resolve does: JSON that is not valid fails with the code invalidJson, JSON that refers to a buffer it lacks fails with the code invalidBytes, and a failure keeps its message, code and data but drops its buffers.
    [[nodiscard]] static Result parseResult(bool ok, std::string_view json, std::vector<std::vector<std::byte>> buffers);

  private:
    struct Completion {
        std::uint64_t id = 0;
        Result result;
    };

    struct NativeEvent {
        std::string name;
        Payload payload;
        EmitOptions options;
    };

    // Replies, native events and posted work wait here, shared with the replies of C++ handlers and with mailboxes so late native code never touches a destroyed bridge.
    struct Inbox {
        std::mutex mutex;
        std::vector<Completion> completions;
        std::vector<NativeEvent> events;
        std::vector<std::function<void()>> tasks;
    };

    struct Pending {
        Callback callback;
        std::string method;
        bool native = false;
        std::optional<std::chrono::steady_clock::time_point> deadline;
    };

    static const EmitOptions kDefaultEmitOptions;

    // Every bridge of the process draws from one sequence, so a restarted app never reuses the id of a call that is still in flight.
    static std::atomic<std::uint64_t> nextCallId;

    [[nodiscard]] static Error readFailure(core::Json payload);

    // Gathers the batched events of each name into one event at the place of the first of them, whose JSON lists theirs in order.
    [[nodiscard]] static std::vector<NativeEvent> gather(std::vector<NativeEvent> events);

    // Runs the engine handler of the method, from a copy because it may register handlers itself, or hands the call to native code.
    void start(std::uint64_t id, std::string_view method, const Payload& params, Reply reply);

    // Delivers the retained events of a name once it has listeners, and keeps them otherwise.
    void deliverRetained(const std::string& name);

    // Gives up the calls whose timeout passed and tells native code about each one.
    void expireCalls();

    Dispatcher dispatcher;
    Canceller canceller;
    std::unordered_map<std::string, Handler> handlers;
    std::unordered_map<std::uint64_t, Pending> pending;
    std::vector<std::pair<Callback, Result>> cancelled;
    std::unordered_map<std::string, std::unique_ptr<core::Signal<const Payload&>>> signals;
    std::unordered_map<std::string, std::deque<Payload>> retained;
    std::shared_ptr<Inbox> inbox = std::make_shared<Inbox>();
};

} // namespace haylen::platform
