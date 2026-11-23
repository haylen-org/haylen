#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/io/PackageWatcher.hpp"

namespace haylen::io {
class OverlayPackage;
}

namespace haylen::platform {

class Host;

class DevelopmentConnection;

// The development state of a process that plays apps while they are being made: the queue of changed package paths, the scanner of the package folder, the connection to the development server with the overlay of the package it writes into, the listeners of what each batch of changes did, and the extension that the runtime of the scripts of the app brings, such as the debugger and the execution budget of Lua, whose client reaches it through the development server. The runtime owns it, so the changes, the snapshot of the folder, the connection and the breakpoints outlive every app that restarts under it, and only a process in development has one.
class DevelopmentSession final {
  public:
    // Hears the report of every batch of changes, such as the page or the development server.
    using Reporter = std::function<void(const core::Json& report)>;

    // The part of development that the runtime of the scripts of the app brings, such as the debugger of Lua with the execution budget of its code, which the session keeps for every app that restarts under it. The first app that runs scripts in the session gives it the extension.
    class Extension {
      public:
        virtual ~Extension() = default;

        // Whether the code of the app is held halted, such as at a breakpoint, while the host keeps the platform responsive.
        [[nodiscard]] virtual bool isHalted() const noexcept = 0;

        // Takes a message of the debugger client that the development server carried, on the frame thread.
        virtual void receive(core::Json message) = 0;

        // The connection to the development server ended, so the client of the debugger is gone.
        virtual void disconnect() = 0;
    };

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

    // A session for a package folder takes the snapshot that changes count from at once, so nothing saved while the app starts is lost, and a session with the address of a development server connects to it. A server outside the device waits for the person to grant the local network where the host asks for it, and a refusal says why the app never connects.
    explicit DevelopmentSession(Host& owner, std::optional<std::filesystem::path> folder = std::nullopt, std::string server = {});
    ~DevelopmentSession();

    DevelopmentSession(const DevelopmentSession&) = delete;
    DevelopmentSession& operator=(const DevelopmentSession&) = delete;

    // Queues changed package paths from any thread. The frame thread takes them as one batch.
    void addChanges(std::span<const std::string> paths);
    [[nodiscard]] std::vector<std::string> takeChanges();
    [[nodiscard]] bool hasChanges() const;

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

    // Gives the session the extension of the runtime of the scripts of the app, which it keeps from then on.
    void setExtension(std::unique_ptr<Extension> value);
    [[nodiscard]] Extension* getExtension() const noexcept {
        return extension.get();
    }

    // Whether the extension holds the code of the app halted.
    [[nodiscard]] bool isHalted() const noexcept {
        return extension != nullptr && extension->isHalted();
    }

    // Hands a message of the debugger to its client through the development server, while one is connected.
    void sendDebug(const core::Json& message) const;

    // Whether the frames of the host may halt, which a page, whose one thread delivers every message, never does.
    [[nodiscard]] bool canHaltFrames() const;

    // Keeps the platform responsive while the code of the app halts, and returns whether the app must let the halt go, such as when the window closes.
    [[nodiscard]] bool pumpHaltedEvents();

    // Records that the developer asked the development server to restart the app, which the frame thread takes once.
    void requestRestart() noexcept {
        restartRequested = true;
    }
    [[nodiscard]] bool takeRestartRequest() noexcept {
        return std::exchange(restartRequested, false);
    }

  private:
    Host& host;
    std::unique_ptr<io::PackageWatcher> watcher;
    std::unique_ptr<DevelopmentConnection> connection;
    std::shared_ptr<io::OverlayPackage> overlay;
    std::atomic<bool> scanning = false;
    mutable std::mutex mutex;
    std::vector<std::string> pending;
    std::vector<Reporter> reporters;
    bool restartRequested = false;

    // The extension talks to its client through the connection, so it goes first.
    std::unique_ptr<Extension> extension;
};

} // namespace haylen::platform
