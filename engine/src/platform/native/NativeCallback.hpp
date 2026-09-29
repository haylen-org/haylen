#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

#include "haylen/platform/Bridge.hpp"
#include "platform/native/NativeSignature.hpp"

namespace haylen::platform {

class NativeCallbacks;

// A C function pointer that native code may call from any thread and that runs a Lua function of the app on the frame thread. It copies the arguments, so the Lua function may run after native code returned. The process keeps it until release, because native code may still call it after the app stopped, when the call does nothing.
class NativeCallback final {
  public:
    enum class Thread : std::uint8_t {
        // Every call waits for the next frame, whichever thread made it.
        Any,
        // A call on the frame thread runs at once, and a call from another thread waits for the next frame.
        Frame,
    };

    using Value = std::variant<std::monostate, bool, std::int64_t, double, void*, std::string>;

    // Where calls go: the callbacks of the app that created the callback, while it runs, and the mailbox of its bridge.
    struct Link {
        std::weak_ptr<NativeCallbacks> owner;
        Bridge::Mailbox mailbox;
        std::thread::id frameThread;
        std::uint64_t id = 0;
    };

    // Throws std::runtime_error where the platform has no native code, such as the browser.
    [[nodiscard]] static std::shared_ptr<NativeCallback> create(NativeSignature signature, Thread thread, Link link);

    // Lets the process forget a callback, which is destroyed with its last reference. Native code must not call it afterwards.
    static void release(const std::shared_ptr<NativeCallback>& callback);

    NativeCallback(const NativeCallback&) = delete;
    NativeCallback& operator=(const NativeCallback&) = delete;
    ~NativeCallback();

    [[nodiscard]] void* getAddress() const noexcept {
        return address;
    }
    [[nodiscard]] std::uint64_t getId() const noexcept {
        return link.id;
    }

  private:
    struct Closure;

    NativeCallback(NativeSignature value, Thread mode, Link target);

    // Runs on the thread of native code with the arguments as libffi passes them.
    static void invoke(void* data, void** arguments);

    [[nodiscard]] static std::int64_t readInteger(const void* argument, const NativeSignature::Parameter& parameter);

    // Copies the arguments, or describes why they cannot be read.
    [[nodiscard]] std::vector<Value> read(void** arguments, std::string& failure) const;
    void deliver(std::vector<Value> values, std::string failure) const;

    static std::mutex& retainedMutex;
    static std::unordered_map<const NativeCallback*, std::shared_ptr<NativeCallback>>& retained;

    NativeSignature signature;
    Thread thread = Thread::Any;
    Link link;
    std::unique_ptr<Closure> closure;
    void* address = nullptr;
};

} // namespace haylen::platform
