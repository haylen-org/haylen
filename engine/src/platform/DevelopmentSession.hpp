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

namespace haylen::io {
class OverlayPackage;
}

namespace haylen::platform {

class DevelopmentConnection;

// The development state of a process that plays apps while they are being made: the queue of changed package paths, the scanner of the package folder, the connection to the development server with the overlay of the package it writes into, and the listeners of what each batch of changes did. The runtime owns it, so the changes, the snapshot of the folder and the connection outlive every app that restarts under it, and only a process in development has one.
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

    // A session for a package folder takes the snapshot that changes count from at once, so nothing saved while the app starts is lost, and a session with the address of a development server connects to it.
    explicit DevelopmentSession(std::optional<std::filesystem::path> folder = std::nullopt, std::string server = {});
    ~DevelopmentSession();

    DevelopmentSession(const DevelopmentSession&) = delete;
    DevelopmentSession& operator=(const DevelopmentSession&) = delete;

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

    // Whether the session talks to a development server, which needs the package to play under an overlay that takes the files the server sends.
    [[nodiscard]] bool isConnected() const noexcept {
        return connection != nullptr;
    }

    // The overlay of the package that plays, which the runtime sets whenever it starts an app.
    void setOverlay(std::shared_ptr<io::OverlayPackage> value);
    [[nodiscard]] io::OverlayPackage* getOverlay() const noexcept {
        return overlay.get();
    }

    // Delivers what the development server sent since the last call, on the frame thread.
    void pump();

  private:
    std::unique_ptr<io::PackageWatcher> watcher;
    std::unique_ptr<DevelopmentConnection> connection;
    std::shared_ptr<io::OverlayPackage> overlay;
    std::atomic<bool> scanning = false;
    mutable std::mutex mutex;
    std::vector<std::string> pending;
    std::vector<Reporter> reporters;
};

} // namespace haylen::platform
