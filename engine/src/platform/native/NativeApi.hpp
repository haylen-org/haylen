#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/platform/ScreenRequest.hpp"
#include "haylen/platform/native/HaylenNative.h"

namespace haylen::platform {

// The HaylenNativeApi that native libraries receive, and the handlers they register with it. The handlers belong to the process, like the libraries, so they answer every app the process runs.
class NativeApi final {
  public:
    [[nodiscard]] static const HaylenNativeApi& get() noexcept;

    // Runs the handler a native library registered for the method with the parameters and their buffers and returns true, or returns false when none did. Runs on the frame thread.
    static bool dispatch(std::uint64_t call, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers);

    // Tells the handler of the method that the app gave up the call, and returns false when no native library handles the method.
    static bool cancel(std::uint64_t call, std::string_view method);

    // The ids of the plugins whose native part a library declared, in the order of their ids.
    [[nodiscard]] static std::vector<std::string> getPlugins();

    // Hands the report of an error that stopped the app to the error handlers of the libraries, in the order they registered. Runs on the frame thread.
    static void reportError(const core::Json& report);

    // Opens the screen with the opener that a library registered for its plugin and name and returns true, or returns false when none did. Runs on the frame thread.
    static bool openScreen(const ScreenRequest& request);

    // Tells the library of the screen that the app gave it up, and returns false when no library opens that screen.
    static bool cancelScreen(std::string_view plugin, std::string_view name, std::uint64_t screen);

    // Whether native UI of a library covers the app.
    [[nodiscard]] static bool isAppCovered();

    // The window of the app that getWindow hands the libraries, which the runtime sets once the window opened.
    static void setWindow(const HaylenNativeWindow& value);

  private:
    struct Handler {
        HaylenNativeHandler handler = nullptr;
        HaylenNativeCancel cancel = nullptr;
        void* user = nullptr;
    };

    struct ScreenHandler {
        HaylenNativeScreenOpener open = nullptr;
        HaylenNativeScreenCancel cancel = nullptr;
        void* user = nullptr;
    };

    struct ErrorHandler {
        HaylenNativeErrorHandler handler = nullptr;
        void* user = nullptr;

        friend bool operator==(const ErrorHandler&, const ErrorHandler&) = default;
    };

    static void emit(const char* event, const char* payloadJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount, int flags);
    static void resolve(std::uint64_t call, int ok, const char* resultJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount);
    static void registerHandler(const char* method, HaylenNativeHandler handler, HaylenNativeCancel cancel, void* user);
    static void log(int level, const char* text);
    static void registerPlugin(const char* id);
    static void registerErrorHandler(HaylenNativeErrorHandler handler, void* user);
    static HaylenNativeVideoStream* openVideoStream(const char* plugin, const char* name, int format, int width, int height);
    static void pushVideoFrame(HaylenNativeVideoStream* stream, const void* pixels, int width, int height, int stride, double timestamp);
    static HaylenNativeAudioStream* openAudioStream(const char* plugin, const char* name, int sampleRate, int channels, int format, int capacityFrames);
    static std::size_t pushAudioFrames(HaylenNativeAudioStream* stream, const void* samples, std::size_t frames);
    static void registerScreen(const char* plugin, const char* name, HaylenNativeScreenOpener open, HaylenNativeScreenCancel cancel, void* user);
    static void finishScreen(std::uint64_t screen, int ok, const char* resultJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount);
    static int getWindow(HaylenNativeWindow* target);
    static void coverApp();
    static void uncoverApp();

    // Copies the buffers that a library hands over, which may be null when there are none.
    [[nodiscard]] static std::vector<std::vector<std::byte>> copyBuffers(const HaylenNativeBuffer* buffers, std::size_t count);

    // The entries of a library may be called from any thread, so an entry that fails logs why instead of throwing into C.
    template <typename Body> static auto guard(const char* entry, Body&& body) noexcept -> decltype(body());

    [[nodiscard]] static std::optional<Handler> find(std::string_view method);
    [[nodiscard]] static std::optional<ScreenHandler> findScreen(std::string_view plugin, std::string_view name);

    // Screens are registered under their plugin and their name, which a dot joins like the methods of plugins.
    [[nodiscard]] static std::string getScreenKey(std::string_view plugin, std::string_view name);

    static const HaylenNativeApi api;
    static std::mutex& mutex;
    static std::unordered_map<std::string, Handler>& handlers;
    static std::unordered_map<std::string, ScreenHandler>& screens;
    static std::set<std::string, std::less<>>& plugins;
    static std::vector<ErrorHandler>& errorHandlers;
    static std::optional<HaylenNativeWindow>& window;
    static int covers;
};

} // namespace haylen::platform
