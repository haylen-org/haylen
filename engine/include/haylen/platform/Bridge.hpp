#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/core/Signal.hpp"

namespace haylen::platform {

// JSON request and event channel between the app and native code. Results and events always reach callbacks on the frame thread. Call ids are unique in the whole process, so a reply that arrives after the app restarted never answers a call of the new app.
class Bridge final {
  public:
    struct Result {
        bool ok = false;
        core::Json value;
        std::string error;
    };

    using Callback = std::function<void(Result)>;
    using Reply = std::function<void(Result)>;
    using Handler = std::function<void(const core::Json& params, Reply reply)>;
    using Dispatcher = std::function<void(std::uint64_t id, std::string_view method, std::string_view paramsJson)>;

    explicit Bridge(Dispatcher native);

    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;

    // Handlers registered in C++ take precedence over the native dispatcher for their method. Their reply may come later from any thread, even after the bridge is gone.
    void registerHandler(std::string method, Handler handler);
    [[nodiscard]] bool hasHandler(std::string_view method) const;

    std::uint64_t call(std::string_view method, const core::Json& params, Callback callback);
    core::Connection on(const std::string& event, std::function<void(const core::Json&)> listener);

    // Thread-safe entry points for native code. A failed call carries a message string or an object with a message field.
    void resolve(std::uint64_t id, bool ok, std::string_view resultJson);
    void emit(std::string_view event, std::string_view payloadJson);

    void pump();
    [[nodiscard]] std::size_t getPendingCallCount() const;

  private:
    struct Completion {
        std::uint64_t id = 0;
        Result result;
    };

    struct NativeEvent {
        std::string name;
        core::Json payload;
    };

    // Replies and native events wait here, shared with the replies of C++ handlers so a late reply never touches a destroyed bridge.
    struct Inbox {
        std::mutex mutex;
        std::vector<Completion> completions;
        std::vector<NativeEvent> events;
    };

    // Every bridge of the process draws from one sequence, so a restarted app never reuses the id of a call that is still in flight.
    static std::atomic<std::uint64_t> nextCallId;

    [[nodiscard]] static std::string getFailureMessage(const core::Json& payload);
    [[nodiscard]] static Result parseResult(bool ok, std::string_view json);

    Dispatcher dispatcher;
    std::unordered_map<std::string, Handler> handlers;
    std::unordered_map<std::uint64_t, Callback> callbacks;
    std::unordered_map<std::string, std::unique_ptr<core::Signal<const core::Json&>>> signals;
    std::shared_ptr<Inbox> inbox = std::make_shared<Inbox>();
};

} // namespace haylen::platform
