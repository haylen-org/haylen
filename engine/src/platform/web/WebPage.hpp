#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::core {
class Engine;
struct AppConfig;
} // namespace haylen::core

namespace haylen::io {
class MemoryPackage;
}

namespace haylen::platform {

// The page that hosts the web runtime through `Module.haylen`, which `platform/web/haylen-runtime.js` defines before the runtime starts. The page learns when apps start and stop and how their frames perform, and it edits an app file by file.
class WebPage final {
  public:
    // Returns the CSS selector of the canvas the page handed to the runtime as `Module.canvas`.
    [[nodiscard]] static std::string getCanvasSelector();

    // Tells the page when an app starts and stops, and passes it frame statistics about once per second.
    static void reportStarted(const core::AppConfig& config);
    static void reportStopped() noexcept;
    static void reportFrame(core::Engine& engine);

    // Requests of the page. The ones that answer a number answer -1 when they fail, with the reason kept for `getLastError`, which `haylen-runtime.js` throws as a JavaScript error.
    [[nodiscard]] static const char* getLastError() noexcept;
    static void loadZip(const std::uint8_t* bytes, int size);
    static void clearFiles();
    static int setFile(const char* path, const std::uint8_t* bytes, int size);
    static int removeFile(const char* path);
    static void runFiles();
    static int reloadAsset(const char* path);

    // A hidden page sends the app to the background and a visible one brings it back. A page that goes away also makes the user data of the app durable.
    static void setVisible(bool visible);
    static void hide();
    static void setOnline(bool online);

    // The color scheme and the battery that the page reports when the runtime starts and whenever they change, and the answers of native dialogs as the JSON that `WebDialogJson::readAnswer` reads.
    static void setTheme(bool dark);
    static void setBattery(double level, bool charging, bool full);
    static void resolveDialog(double id, const char* json);

    // Answers of page handlers and events of the page, with their byte buffers as a table of an address and a size in wasm memory for each, which the page frees once the call returns. The flags of an event are those of `HaylenNativeEmitFlags`.
    static void resolve(double call, bool ok, const char* json, const std::uint32_t* buffers, int count);
    static void emit(const char* event, const char* json, const std::uint32_t* buffers, int count, int flags);

    // Ends of the screens of plugins: a screen that the page opened, and a screen that a redirect left before the page loaded again, with the state that the page kept for it.
    static void finishScreen(double id, bool ok, const char* json, const std::uint32_t* buffers, int count);
    static void restoreScreen(const char* plugin, const char* screen, const char* state, bool ok, const char* json, const std::uint32_t* buffers, int count);

    // The table of an address and a size in wasm memory for each buffer, which the page reads while a call into it runs.
    [[nodiscard]] static std::vector<std::uint32_t> describeBuffers(std::span<const std::vector<std::byte>> buffers);

    // Streams of the web parts of plugins. Opening answers the address of the stream, which stays valid for good, or 0 when it fails. A video stream takes RGBA8 frames without padding and an audio stream interleaved float frames.
    [[nodiscard]] static void* openVideoStream(const char* plugin, const char* name);
    static int pushVideoFrame(void* stream, const std::uint8_t* pixels, int width, int height, double timestamp);
    [[nodiscard]] static void* openAudioStream(const char* plugin, const char* name, int sampleRate, int channels, int capacityFrames);
    static int pushAudioFrames(void* stream, const float* samples, int frames);

  private:
    static constexpr double kStatsInterval = 1000.0;

    [[nodiscard]] static std::vector<std::vector<std::byte>> readBuffers(const std::uint32_t* table, int count);

    // Files the page sends one by one while it edits an app, restarted as a package on demand.
    static std::shared_ptr<io::MemoryPackage>& editorPackage;
    static std::string& lastError;
    static double lastStats;

    [[nodiscard]] static core::Json getFrameStatistics(core::Engine& engine);

    template <typename Body> static int answer(Body&& body);
};

} // namespace haylen::platform
