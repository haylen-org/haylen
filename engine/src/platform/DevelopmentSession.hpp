#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/io/PackageWatcher.hpp"

namespace haylen::platform {

// The development state of a process that plays apps while they are being made: the queue of changed package paths, the scanner of the package folder and the listeners of what each batch of changes did. The runtime owns it, so the changes and the snapshot of the folder outlive every app that restarts under it, and only a process in development has one.
class DevelopmentSession final {
  public:
    // Hears the report of every batch of changes, such as the page or the development server.
    using Reporter = std::function<void(const core::Json& report)>;

    // One scan of the package folder, which a job on the I/O pool runs because it blocks on the file system. Only one exists at a time, and it frees the folder for the next scan once the job ran or was dropped, so scans never overlap, even when the app restarts during one.
    class Scan final {
      public:
        explicit Scan(DevelopmentSession& owner) noexcept : session(owner) {}
        ~Scan();

        Scan(const Scan&) = delete;
        Scan& operator=(const Scan&) = delete;

        // Queues what changed in the folder since the last scan.
        void run();

      private:
        DevelopmentSession& session;
    };

    // A session for a package folder takes the snapshot that changes count from at once, so nothing saved while the app starts is lost.
    explicit DevelopmentSession(std::optional<std::filesystem::path> folder = std::nullopt);

    // Queues changed package paths from any thread. The frame thread takes them as one batch.
    void addChanges(std::span<const std::string> paths);
    [[nodiscard]] std::vector<std::string> takeChanges();

    // Whether the session scans a package folder, which the desktop player of an app folder does.
    [[nodiscard]] bool isWatching() const noexcept {
        return watcher != nullptr;
    }

    // Returns the next scan of the folder, or null without a folder and while the last scan still exists.
    [[nodiscard]] std::shared_ptr<Scan> beginScan();

    void addReporter(Reporter reporter);
    void report(const core::Json& value) const;

  private:
    std::unique_ptr<io::PackageWatcher> watcher;
    std::atomic<bool> scanning = false;
    mutable std::mutex mutex;
    std::vector<std::string> pending;
    std::vector<Reporter> reporters;
};

} // namespace haylen::platform
