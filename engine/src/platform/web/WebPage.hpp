#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/platform/SystemAnswer.hpp"

namespace haylen::core {
class Engine;
struct AppConfig;
} // namespace haylen::core

namespace haylen::platform {

// The page that hosts the web runtime through `Module.haylen`, which `platform/web/haylen-runtime.js` and `platform/web/haylen-page.js` define before the runtime starts. The page learns when apps start and stop and how their frames perform, and it edits an app file by file.
class WebPage final {
  public:
    // Returns the CSS selector of the canvas the page handed to the runtime as `Module.canvas`.
    [[nodiscard]] static std::string getCanvasSelector();

    // Whether the element that holds the canvas and the overlay of the page is fullscreen. SDL follows only a fullscreen of the canvas itself, so the page reports it.
    [[nodiscard]] static bool isFullscreen() noexcept;

    // Tells the page the width of the drawable of the engine, which turns framebuffer pixels into page pixels over the canvas, whenever the window sizes it.
    static void setDrawableWidth(int width);

    // Puts the element that holds the canvas and the overlay of the page in fullscreen on the screen with the id, or takes the page out of it, and answers once the browser settled it. A newer change replaces one that the browser has not settled, which answers `Denied`, and starts once the browser settled it.
    static void setFullscreen(bool value, std::string_view display, std::function<void(SystemAnswer answer)> callback);

    // Keeps the screen on through a wake lock of the browser while the page shows, or lets it go, and answers with the first request of the lock.
    static void setScreenKeptOn(bool value, std::function<void(SystemAnswer answer)> callback);

    // Answers once the browser locked the pointer that SDL asked for, or refused it, within a bound.
    static void awaitPointerLock(bool value, std::function<void(SystemAnswer answer)> callback);

    // Locks the pointer to the canvas, or releases it, for the Worker mode, whose page asks the browser itself, and answers as `awaitPointerLock` does.
    static void lockPointer(bool value, std::function<void(SystemAnswer answer)> callback);

    // The calls that a browser may gate: the writes and reads of its clipboard and and the request to see every screen, which the page answers asynchronously through `answerGatedCall` with the JSON `{state, message, missing, text}` of `SystemAnswer::fromJson` and the text of a read.
    static void setClipboard(std::string_view text, std::function<void(SystemAnswer answer)> callback);
    static void readClipboard(std::function<void(SystemAnswer answer, std::string text)> callback);
    static void requestEveryScreen(std::function<void(SystemAnswer answer)> callback);
    static void answerGatedCall(double id, const char* json);

    // The text of the last paste into the page or the last text it wrote, which a paste of the person into a field of the UI reads.
    [[nodiscard]] static std::string getClipboard();

    // Tells the page when an app starts, with the error that stopped its start, or failed to start, and when it stops, and passes it frame statistics about once per second. A start answers the request of the editor that ran the app.
    static void reportStarted(core::Engine& engine);
    static void reportStartFailed(const core::Json& error);
    static void reportStopped() noexcept;
    static void reportFrame(core::Engine& engine);

    // Answers the requests of the editor to apply changes once a frame applied them, with what hot reload did in that frame. A frame that runs while changes still wait answers nothing.
    static void finishFrame();

    // Tells the page to fade its splash out over the seconds, once the app drew its first complete frame.
    static void endSplashScreen(float fadeOutSeconds);

    // The requests of the editor contract of `haylen-runtime.js`, each with the id of its request where it answers later through `completeRequest`. The ones that answer a number answer -1 when they fail, with the error kept for `getLastError` as the JSON `{code, message}`, which the page rejects the request with. The page edits the files of the package that plays, under its overlay, as atomic batches that each make a new revision of the package, and `applyChanges` hands the paths a batch touched to hot reload in development, or restarts the app otherwise. The runtime of a release, which carries the bootstrap of its app, plays only its protected release and refuses every request that edits or runs a package.
    [[nodiscard]] static const char* getLastError() noexcept;
    static int edit(const char* operations, const std::uint32_t* buffers, int count);
    static int clearFiles();
    static int applyChanges(double request);
    static int runFiles(double request);
    static int loadZip(double request, const std::uint8_t* bytes, int size);
    static int stopApp();
    static void inspect(double request, const char* path);

    // Passes the report of every batch of changes that hot reload applied to the page.
    static void reportReloaded(const core::Json& report);

    // A hidden page sends the app to the background and a visible one brings it back. A page that goes away also makes the user data of the app durable.
    static void setVisible(bool visible);
    static void hide();
    static void setOnline(bool online);

    // The color scheme, the languages as a JSON array of tags, the accessibility preferences, the battery and the time zone that the page reports when the runtime starts and whenever they change.
    static void setTheme(bool dark);
    static void setLanguages(const char* json);
    static void setBattery(double level, bool charging, bool full);
    static void setTimeZone(const char* zone);
    static void setAccessibility(double textScale, bool reducedMotion, bool highContrast, bool reducedTransparency);

    // Answers of page handlers and events of the page, with their byte buffers as a table of an address and a size in wasm memory for each, which the page frees once the call returns. The flags of an event are those of `HaylenNativeEmitFlags`.
    static void resolve(double call, bool ok, const char* json, const std::uint32_t* buffers, int count);
    // Queues a line of the developer console from the page, whose result reaches the page in the next frame.
    static void runConsoleLine(double id, const char* line);
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

    // The frames that the ring of an audio stream has room for, which the page of the Worker mode keeps its pushes within.
    [[nodiscard]] static int getAudioStreamRoom(void* stream) noexcept;

  private:
    static constexpr double kStatsInterval = 1000.0;

    // A request of the editor that failed, with the stable code of the contract, such as `invalidPath`, and the sentence for the person.
    class RequestError final : public std::runtime_error {
      public:
        RequestError(std::string errorCode, const std::string& message) : std::runtime_error(message), code(std::move(errorCode)) {}

        [[nodiscard]] const std::string& getCode() const noexcept {
            return code;
        }

      private:
        std::string code;
    };

    // A request whose answer waits for a frame or a start, with the revision of the package it applies.
    struct PendingRequest {
        double request = 0.0;
        int revision = 0;
    };

    // One step of a batch of edits, checked before any of them applies.
    struct Edit {
        enum class Kind : std::uint8_t {
            Write,
            Remove,
            Rename,
        };

        Kind kind = Kind::Write;
        std::string path{};
        std::string target{};
        std::vector<std::uint8_t> bytes{};
    };
    static constexpr std::array<std::string_view, 3> kEditNames = {"write", "remove", "rename"};

    [[nodiscard]] static std::vector<Edit> readEdits(const char* operations, std::span<const std::vector<std::byte>> contents);
    [[nodiscard]] static std::string readPath(const core::Json& operation, const char* key);
    static void checkEdits(const std::vector<Edit>& edits);

    // The sizes of the files of a package by their paths.
    using FileSizes = std::map<std::string, std::uint64_t, std::less<>>;
    [[nodiscard]] static FileSizes measure(const io::Package& package);

    // Refuses a package that would hold more files or bytes than the project budget of the page, `Module.haylen.projectBudget`.
    static void checkBudget(const FileSizes& files);
    static void completeRequest(double request, bool ok, const core::Json& result);

    [[nodiscard]] static std::vector<std::vector<std::byte>> readBuffers(const std::uint32_t* table, int count);

    // The paths the page changed since its last `applyChanges`, the revision of the package, the requests that wait for their answer and the reports of hot reload of the frame that runs.
    static std::vector<std::string>& touched;
    static int revision;
    static std::vector<PendingRequest>& pendingApplies;
    static std::vector<PendingRequest>& pendingRuns;
    static std::vector<core::Json>& frameReports;

    using GatedCallback = std::function<void(SystemAnswer answer, std::string text)>;

    // Keeps the callback of a gated call until the page answers it, and returns its id.
    [[nodiscard]] static double awaitGatedCall(GatedCallback callback);
    [[nodiscard]] static GatedCallback withoutText(std::function<void(SystemAnswer answer)> callback);

    // The gated calls that the page has not answered yet.
    static std::unordered_map<std::uint64_t, GatedCallback>& gatedCalls;
    static std::uint64_t nextGatedCall;
    static std::string& lastError;
    static double lastStats;

    [[nodiscard]] static core::Json getFrameStatistics(core::Engine& engine);

    template <typename Body> static int answer(Body&& body);
    static void requireEditable();
    static void requirePlaying();
};

} // namespace haylen::platform
