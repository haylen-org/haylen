#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "NativeDemoStore.hpp"
#include "haylen/platform/native/HaylenNative.h"

// The desktop part of the Native Demo plugin, a C++ library for macOS, Windows and Linux built on the standard library and the threads of each system. The function `native.load` hands its init function the `HaylenNativeApi` of the engine, through which the library answers the methods of the plugin, sends its events, pushes its video and audio streams, opens its screens over the window of the app and hears the errors that stop the app. Its handlers, their cancel functions, its screens and its error handler run on the frame thread, and its threads only emit events, answer calls, end screens and push frames and samples.
// The desktops place no views of native libraries over the app, so the banner, the covering native screen and the map fail with the code `unsupported`, and so do the file picker and the share sheet, which the desktop systems offer through no shared API, the camera, the microphone and the location, whose desktop APIs differ on each system, and `config`, since native libraries receive no plugin parameters. The confirm screen, the fake store, the fake ads and the fake sign-in are windows of their own, which `NativeDemoScreen` opens with the window of the app from `getWindow`, and the other apps open through the shell of each system.
class NativeDemoPlugin final {
  public:
    // Creates the plugin, which lives as long as the process, since the engine and the threads of the library may reach it until the process ends, registers its methods, its screens and its error handler, declares the library the native part of `native-demo` and sends `loaded` retained, so the first listener of the app receives it however late it connects. Returns 1 when the engine hands a native API of another version.
    [[nodiscard]] static int initialize(const HaylenNativeApi* api);

  private:
    using Json = nlohmann::json;

    // The work that runs on threads of the library, each with a generation that a new start, or stopping, raises, which ends the older threads when they wake up.
    enum class Work { Ticks, Video, Tone, Burst };

    // A call of the app with its parameters and the buffers they refer to, which stay valid while the handler runs.
    struct Call {
        std::uint64_t id;
        const Json& params;
        std::span<const HaylenNativeBuffer> buffers;
    };

    // A call of `wait`, which only a cancel or a timeout of the app ends.
    struct Wait {
        std::uint64_t call;
        Json token;
    };

    using Handler = void (NativeDemoPlugin::*)(const Call& call);

    static constexpr std::string_view kPrefix = "native-demo.";
    static constexpr std::string_view kLanguage = "C++";
#if defined(_WIN32)
    static constexpr std::string_view kPlatform = "windows";
#elif defined(__APPLE__)
    static constexpr std::string_view kPlatform = "macos";
#else
    static constexpr std::string_view kPlatform = "linux";
#endif
    static constexpr std::size_t kMaxWaits = 32;
    static constexpr int kMaxFrameSide = 2048;

    // The picture of the video stream, 30 frames per second of BGRA pixels, and the tone of the audio stream, 16-bit mono samples a tenth of a second ahead of the clock.
    static constexpr int kVideoWidth = 320;
    static constexpr int kVideoHeight = 180;
    static constexpr int kToneRate = 44100;
    static constexpr int kToneBlock = 441;
    static constexpr int kBurstsPerSecond = 30;

    static const std::array<std::pair<std::string_view, Handler>, 20> kHandlers;
    static const std::array<std::pair<std::string_view, std::string_view>, 17> kUnsupported;

    explicit NativeDemoPlugin(const HaylenNativeApi& engine);

    static void handleCall(void* user, std::uint64_t call, const char* method, const char* paramsJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount);
    static void cancelCall(void* user, std::uint64_t call);
    static void openConfirm(void* user, std::uint64_t screen, const char* paramsJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount);
    static void openPurchase(void* user, std::uint64_t screen, const char* paramsJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount);
    static void openInterstitial(void* user, std::uint64_t screen, const char* paramsJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount);
    static void openRewarded(void* user, std::uint64_t screen, const char* paramsJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount);
    static void openSignIn(void* user, std::uint64_t screen, const char* paramsJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount);
    static void cancelScreen(void* user, std::uint64_t screen);
    static void handleAppFailure(void* user, const char* reportJson);

    // The JSON of a value, with bytes that are not UTF-8 replaced, since the text crosses into the engine.
    [[nodiscard]] static std::string write(const Json& value);
    [[nodiscard]] static std::optional<double> readNumber(const Json& params, const char* key);
    [[nodiscard]] static std::string readText(const Json& params, const char* key, std::string_view fallback);

    void succeed(std::uint64_t call, const Json& result, std::span<const HaylenNativeBuffer> buffers = {}) const;
    void fail(std::uint64_t call, std::string_view message, std::string_view code) const;
    void failUnsupported(std::uint64_t call, std::string_view message) const;
    void emit(std::string_view event, const Json& payload, int flags = 0) const;

    // Raises the generation of the work, which ends the threads of the earlier generation, and returns the new one.
    unsigned restart(Work work);
    [[nodiscard]] bool isCurrent(Work work, unsigned generation) const;

    void echo(const Call& call);
    void echoBytes(const Call& call);
    void generatedImage(const Call& call);
    void captureFrame(const Call& call);
    void checksumView(const Call& call);
    void startVideo(const Call& call);
    void stopVideo(const Call& call);
    void startTone(const Call& call);
    void stopTone(const Call& call);
    void burst(const Call& call);
    void compute(const Call& call);
    void failOnPurpose(const Call& call);
    void wait(const Call& call);
    void ticks(const Call& call);
    void start(const Call& call);
    void products(const Call& call);
    void restorePurchases(const Call& call);
    void currentUser(const Call& call);
    void signOut(const Call& call);
    void openApp(const Call& call);

    void playVideo(unsigned generation, HaylenNativeVideoStream* stream) const;
    void playTone(unsigned generation, HaylenNativeAudioStream* stream, double frequency) const;
    void sendBursts(unsigned generation, int count, int limit) const;
    void sendTicks(unsigned generation, double seconds) const;
    void countPrimes(std::uint64_t call, long long limit) const;
    void runOpener(std::uint64_t call, const std::string& url) const;

    // The screens of the fake store, account and ads tell the app what the person confirmed.
    void purchased();
    void signedIn();
    void rewarded() const;

    [[nodiscard]] static Json describeAccount(std::string_view token);

    const HaylenNativeApi& api;
    std::array<std::atomic<unsigned>, 4> generations{};
    std::vector<Wait> waits;
    std::optional<std::string> lastError;
    unsigned capturedFrames = 0;
    NativeDemoStore store;
};
