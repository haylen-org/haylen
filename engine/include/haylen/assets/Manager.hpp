#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/graphics/Shader.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/graphics/VectorImage.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/TrueTypeFont.hpp"

namespace haylen::io {
class Package;
class PackageReader;
} // namespace haylen::io

namespace haylen::core {
class EventBus;
class JobSystem;
} // namespace haylen::core

namespace haylen::graphics {
class Device;
}

namespace haylen::assets {

// Loads, caches and preloads assets by their path inside the package content folder. Cached assets stay alive while anything, including a preload group, still holds them. The manager publishes `assetLoaded` when an asset enters the cache, `assetReloaded` when a changed file updates it in place, `assetChanged` when a changed file leaves a live asset of a type that cannot update in place behind, and `assetUnloaded` when its last holder lets go, queued on the event bus with the type and path of the asset.
class Manager final {
  public:
    // What a loader receives: the bytes of the file, or a reader of it for an asset that streams. Decoders may read companion files, such as the image of an atlas, through the package from their worker thread.
    struct Request {
        std::string type;
        std::string path;
        core::Json options;
        std::vector<std::uint8_t> bytes;
        std::shared_ptr<io::PackageReader> reader;
        const io::Package* package = nullptr;
    };

    // Loader for one kind of asset. The function `normalize` validates the options and returns their canonical form, so equal options share one cached asset. The function `decode` runs on a worker thread with the file bytes, and `finalize` runs on the frame thread, where GPU resources may be created. An asset for whose normalized options `streams` returns true never has its file read whole: its decoder receives a reader of the file instead and runs on the I/O pool, since it reads as it decodes, so files of any size, such as long music, load in bounded memory.
    struct Type {
        std::string name;
        std::vector<std::string> extensions;
        std::function<core::Json(const core::Json&)> normalize;
        std::function<std::shared_ptr<void>(Request&)> decode;
        std::function<std::shared_ptr<void>(std::shared_ptr<void>, const Request&)> finalize;
        std::function<bool(const core::Json&)> streams;

        // Updates a live asset in place from the fresh bytes of its file. Types without it leave the cache on reload, so the next load reads the file again, and the live asset announces with `assetChanged` that it is stale.
        std::function<void(const std::shared_ptr<void>&, Request&)> reload;
    };

    // One asset of a preload group. An empty type is resolved from the file extension.
    struct Entry {
        std::string path;
        std::string type;
        core::Json options = core::Json::object();
    };

    using Callback = std::function<void(std::shared_ptr<void>, std::string)>;
    using GroupProgress = std::function<void(float)>;
    using GroupCompletion = std::function<void(std::vector<std::string>)>;

    // How the manager learns which assets have their bytes on the device and brings the ones that do not, before it reads them: the content of a protected release whose files download on demand. The request calls back on the frame thread with an error message, empty once every asset is on the device.
    struct Availability {
        std::function<bool(std::string_view)> isLocal;
        std::function<void(std::vector<std::string>, GroupProgress, std::function<void(std::string)>)> acquire;
    };

    Manager(io::Package& contentPackage, core::JobSystem& jobSystem, graphics::Device& graphicsDevice, core::EventBus& eventBus);
    ~Manager();

    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;

    // Texture options travel as `{"filter": "nearest" or "linear", "wrap": "clamp", "repeat" or "mirror"}` in asset requests.
    [[nodiscard]] static graphics::Texture::Options textureOptionsFromJson(const core::Json& options);
    [[nodiscard]] static core::Json textureOptionsToJson(graphics::Texture::Options options);

    void registerType(Type type);
    [[nodiscard]] bool hasType(std::string_view name) const noexcept;
    [[nodiscard]] std::string getTypeForPath(std::string_view path) const;

    [[nodiscard]] core::Json normalizeOptions(std::string_view type, const core::Json& options) const;
    [[nodiscard]] std::shared_ptr<void> load(std::string_view type, std::string_view path, const core::Json& options = core::Json::object());

    // Returns the cached asset for the type, path and options, or caches what `make` builds. Composite assets use it to share their dependencies with direct loads.
    std::shared_ptr<void> share(std::string_view type, std::string_view path, const core::Json& options, const std::function<std::shared_ptr<void>(const Request&)>& make);
    void loadAsync(std::string_view type, std::string_view path, Callback callback, const core::Json& options = core::Json::object());

    [[nodiscard]] graphics::Texture texture(std::string_view path, graphics::Texture::Options options = {});
    void textureAsync(std::string_view path, std::function<void(graphics::Texture, std::string)> callback, graphics::Texture::Options options = {});
    [[nodiscard]] std::shared_ptr<text::Font> font(std::string_view path, text::TrueTypeFont::Options options = {});

    // Loads a `.shader` file that `haylen.py shaders` compiled, which reloads in place when the file changes.
    [[nodiscard]] graphics::Shader shader(std::string_view path);
    [[nodiscard]] graphics::VectorImage vectorImage(std::string_view path);
    [[nodiscard]] core::Json json(std::string_view path);
    [[nodiscard]] std::string text(std::string_view path) const;
    [[nodiscard]] std::vector<std::uint8_t> bytes(std::string_view path) const;

    // Reads at most `size` bytes of a file at an offset, fewer when the file ends first, so files of any size are read in parts.
    [[nodiscard]] std::vector<std::uint8_t> bytes(std::string_view path, std::uint64_t offset, std::size_t size) const;
    [[nodiscard]] std::uint64_t getFileSize(std::string_view path) const;
    [[nodiscard]] bool exists(std::string_view path) const;
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const;

    // Groups name a set of assets that load together and are released together. A path ending with a slash includes every file under that folder.
    void defineGroup(std::string name, std::vector<Entry> entries);
    void defineGroups(const core::Json& manifest);
    void preload(std::string_view group, GroupProgress progress = {}, GroupCompletion completion = {});
    void unloadGroup(std::string_view group);
    [[nodiscard]] float getGroupProgress(std::string_view group) const;
    [[nodiscard]] bool isGroupLoaded(std::string_view group) const;
    [[nodiscard]] std::vector<std::string> getGroups() const;

    // The paths of the assets of a group, with its folders expanded. Throws `std::invalid_argument` for a group that is not defined.
    [[nodiscard]] std::vector<std::string> getGroupPaths(std::string_view group) const;

    [[nodiscard]] std::size_t getCachedCount() const noexcept;
    [[nodiscard]] std::size_t getPendingCount() const noexcept;
    std::size_t releaseUnused();

    // Reloads every live asset read from a changed file, a path relative to the content folder, and returns how many there were. Assets nobody holds anymore just leave the cache.
    std::size_t reload(std::string_view path);

    // Drops every pending callback and group listener without running them, before the systems they point into shut down.
    void cancelAll() noexcept;

    // Finalizes the assets that finished decoding in the background, creating their GPU resources, until the upload budget of the frame runs out, and at least one per call so loading always moves on. The engine calls it once per frame and never while the app is in the background.
    // Makes loads ask the availability before they read, so a synchronous load of an asset that is not on the device fails at once and an asynchronous load or a preload brings it first. Without one every asset is on the device.
    void setAvailability(Availability value);

    void finalizePending();
    void setUploadBudget(double seconds);
    [[nodiscard]] double getUploadBudget() const noexcept {
        return uploadBudget;
    }
    [[nodiscard]] std::size_t getUploadCount() const noexcept {
        return uploads.size();
    }

  private:
    static constexpr double kDefaultUploadBudget = 0.004;

    // A decoded asset waiting on the frame thread for its finalizer, or the error that ended its load.
    struct Upload {
        std::string key;
        std::shared_ptr<Request> request;
        std::function<std::shared_ptr<void>(std::shared_ptr<void>, const Request&)> finalize;
        std::shared_ptr<void> decoded;
        std::string error;
    };

    struct Group {
        std::vector<Entry> entries;
        std::vector<std::shared_ptr<void>> held;
        std::vector<std::string> errors;
        std::vector<GroupProgress> progressListeners;
        std::vector<GroupCompletion> completionListeners;
        std::size_t completed = 0;
        std::size_t total = 0;

        // The share of the progress that the download of assets that are not on the device takes, and how far it got.
        float downloadShare = 0.0F;
        float downloaded = 0.0F;
        std::uint64_t generation = 0;
        bool loading = false;
        bool loaded = false;
    };

    struct Pending {
        std::vector<Callback> callbacks;
    };

    // A cached asset with the type, path and options it was loaded with, so a changed file finds every asset read from it.
    struct Cached {
        std::weak_ptr<void> asset;
        std::string type;
        std::string path;
        core::Json options;
    };

    // Publishes the asset events from any thread for as long as the manager lives, because the last holder of an asset may let go anywhere.
    struct Publisher;

    [[nodiscard]] static std::string lowercase(std::string_view text);
    [[nodiscard]] static std::string describe(const core::Json& value);
    [[nodiscard]] static text::TrueTypeFont::Options fontOptionsFromJson(const core::Json& options);
    [[nodiscard]] static core::Json fontOptionsToJson(const text::TrueTypeFont::Options& options);
    [[nodiscard]] static std::string cacheKey(std::string_view type, std::string_view path, const core::Json& options);
    [[nodiscard]] static bool isList(const core::Json& value) noexcept;
    [[nodiscard]] static Entry readEntry(std::string_view group, const core::Json& item);

    [[nodiscard]] const Type& getType(std::string_view name) const;
    [[nodiscard]] Request makeRequest(std::string_view typeName, std::string_view path, const core::Json& options) const;
    [[nodiscard]] std::shared_ptr<void> findCached(const std::string& key) const;
    [[nodiscard]] std::vector<Entry> expand(const std::vector<Entry>& entries) const;
    void startLoad(Request request, Callback callback);
    void readAndDecode(const std::shared_ptr<Request>& loading, const std::string& key, const Type& assetType);

    // Gives the request the bytes of its file, or a reader of it for an asset that streams.
    void readFile(Request& request, bool streamed) const;
    [[nodiscard]] static bool isStreamed(const Request& request, const Type& assetType);

    // Throws for an asset whose bytes are not on the device, which a synchronous read never waits for.
    void requireLocal(std::string_view path) const;
    [[nodiscard]] bool isLocal(std::string_view path) const;
    void loadGroup(const std::string& groupName, std::uint64_t generation, std::vector<Request> requests);
    void reportGroupProgress(Group& group) const;
    void finish(const std::string& key, const std::shared_ptr<void>& asset, const std::string& error);

    // Wraps a new asset so the manager learns when its last holder lets go, and announces it.
    [[nodiscard]] std::shared_ptr<void> track(std::shared_ptr<void> asset, const Request& request) const;
    void completeGroup(const std::string& name, std::uint64_t generation);

    io::Package& package;
    core::JobSystem& jobs;
    graphics::Device& device;
    std::unordered_map<std::string, Type> types;
    std::unordered_map<std::string, Cached> cache;
    std::unordered_map<std::string, Pending> pending;
    std::unordered_map<std::string, Group> groups;
    Availability availability;
    std::deque<Upload> uploads;
    double uploadBudget = kDefaultUploadBudget;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    std::shared_ptr<Publisher> publisher;
};

} // namespace haylen::assets
