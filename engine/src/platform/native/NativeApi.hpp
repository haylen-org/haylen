#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "haylen/platform/native/HaylenNative.h"

namespace haylen::platform {

// The HaylenNativeApi that native libraries receive, and the handlers they register with it. The handlers belong to the process, like the libraries, so they answer every app the process runs.
class NativeApi final {
  public:
    [[nodiscard]] static const HaylenNativeApi& get() noexcept;

    // Runs the handler a native library registered for the method and returns true, or returns false when none did. Runs on the frame thread.
    static bool dispatch(std::uint64_t call, std::string_view method, std::string_view paramsJson);

    // Tells the handler of the method that the app gave up the call, and returns false when no native library handles the method.
    static bool cancel(std::uint64_t call, std::string_view method);

  private:
    struct Handler {
        HaylenNativeHandler handler = nullptr;
        HaylenNativeCancel cancel = nullptr;
        void* user = nullptr;
    };

    static void emit(const char* event, const char* payloadJson);
    static void resolve(std::uint64_t call, int ok, const char* resultJson);
    static void registerHandler(const char* method, HaylenNativeHandler handler, HaylenNativeCancel cancel, void* user);
    static void log(int level, const char* text);

    [[nodiscard]] static std::optional<Handler> find(std::string_view method);

    static const HaylenNativeApi api;
    static std::mutex mutex;
    static std::unordered_map<std::string, Handler> handlers;
};

} // namespace haylen::platform
