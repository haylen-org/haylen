#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "haylen/platform/native/HaylenNative.h"

// The part of the test library that works through the `HaylenNativeApi` of the engine: bridge handlers that answer from threads of the library, byte buffers, batched and retained events, video and audio streams, a screen, the window of the app, covers and the errors that stop apps. The library keeps one instance for the whole process, and each init hands it the interface of the engine that loaded it. Its handlers, its screen and its error handler run on the frame thread, and its threads carry the interface that started them, because a later init replaces the one the library keeps.
class NativeTestLibrary final {
  public:
    // The build of the library names itself, as `dynamic` or as `static` for the copy that iOS and tvOS apps link.
    static constexpr const char* kOrigin = NATIVE_TEST_ORIGIN;

    // Registers the handlers of the library, its screen and its error handler, declares it the native part of the `native-test` plugin and announces it from a thread of its own with a retained event, which waits for a listener that connects later. Returns 1 when the engine hands a native API of another version.
    [[nodiscard]] static int initialize(const HaylenNativeApi* engine);

    // Fails on purpose, so the tests see how a failing init reaches Lua.
    [[nodiscard]] static int failInitialization();

    [[nodiscard]] static NativeTestLibrary& get();

    // Pushes a frame of `width` by `height` pixels into the video stream `pattern` of `native-test` from a thread of the library. The stream is BGRA, every pixel holds blue, green and red from the seed and an opaque alpha, and every row carries four bytes of padding.
    void pushFrameLater(std::int32_t width, std::int32_t height, std::int32_t seed) const;

    // Pushes `count` 16-bit samples of the value into the audio stream `tone` of `native-test`, mono at 8000 Hz, from a thread of the library.
    void pushToneLater(std::int32_t count, std::int32_t value) const;

    // Tells whether the engine refuses to open the streams of `native-test` again with another format.
    [[nodiscard]] bool isReopenRefused() const;

    [[nodiscard]] std::int32_t getErrorCount() const;
    [[nodiscard]] const char* getLastError() const;

    // Ends the open panel with a result from a thread of the library, the way a native window ends when its user closes it.
    void closeScreenLater() const;

    // Returns the handle of the window of the app as a pointer-wide integer, or -1 while the engine has none.
    [[nodiscard]] std::intptr_t getWindow() const;

    void setCovered(bool covered) const;

  private:
    using Json = nlohmann::json;
    using Handler = void (NativeTestLibrary::*)(std::uint64_t call, const Json& params, std::span<const HaylenNativeBuffer> buffers) const;

    static const std::array<std::pair<std::string_view, Handler>, 5> kHandlers;

    NativeTestLibrary() = default;

    static void handleCall(void* user, std::uint64_t call, const char* method, const char* paramsJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount);
    static void cancelCall(void* user, std::uint64_t call);
    static void openScreen(void* user, std::uint64_t screenId, const char* paramsJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount);
    static void cancelScreen(void* user, std::uint64_t screenId);
    static void handleAppFailure(void* user, const char* reportJson);

    // The JSON of a value, with bytes that are not UTF-8 replaced, since the text crosses into the engine.
    [[nodiscard]] static std::string write(const Json& value);

    // The engine hands JSON text, which the library reads without exceptions, since none may cross the C interface of the engine.
    [[nodiscard]] static Json read(const char* text);

    void echo(std::uint64_t call, const Json& params, std::span<const HaylenNativeBuffer> buffers) const;
    void echoBytes(std::uint64_t call, const Json& params, std::span<const HaylenNativeBuffer> buffers) const;
    void burst(std::uint64_t call, const Json& params, std::span<const HaylenNativeBuffer> buffers) const;
    void failOnPurpose(std::uint64_t call, const Json& params, std::span<const HaylenNativeBuffer> buffers) const;
    void wait(std::uint64_t call, const Json& params, std::span<const HaylenNativeBuffer> buffers) const;

    static void announce(const HaylenNativeApi* engine);
    static void answerEcho(const HaylenNativeApi* engine, std::uint64_t call, const Json& params);
    static void answerBytes(const HaylenNativeApi* engine, std::uint64_t call, const std::vector<std::uint8_t>& bytes);
    static void pushFrame(const HaylenNativeApi* engine, std::int32_t width, std::int32_t height, std::int32_t seed);
    static void pushTone(const HaylenNativeApi* engine, std::int32_t count, std::int32_t value);
    static void closeScreen(const HaylenNativeApi* engine, std::uint64_t screenId);

    const HaylenNativeApi* api = nullptr;
    std::int32_t errorCount = 0;
    std::string lastError;
    std::uint64_t screen = 0;
};
