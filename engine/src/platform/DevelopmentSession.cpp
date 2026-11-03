#include "platform/DevelopmentSession.hpp"

#include <algorithm>
#include <exception>
#include <optional>
#include <utility>

#include "debug/Debugger.hpp"
#include "haylen/core/Log.hpp"
#include "io/OverlayPackage.hpp"
#include "platform/DevelopmentConnection.hpp"
#include "platform/Host.hpp"

namespace haylen::platform {

// The debugger talks to its client through the connection, and while it halts the app it pumps the connection itself, since no frame pumps it then.
DevelopmentSession::DevelopmentSession(Host& host, std::optional<std::filesystem::path> folder, std::string server) {
    // clang-format off
    debugger = std::make_unique<debug::Debugger>(host, [this](const core::Json& message) {
        if (connection) {
            connection->sendDebug(message);
        }
    }, [this] { pump(); });
    // clang-format on
    if (folder) {
        watcher = std::make_unique<io::PackageWatcher>(*folder);
        core::Log::info("Watching \"{}\" for changes.", folder->generic_string());
    }
    if (!server.empty()) {
        connection = std::make_unique<DevelopmentConnection>(std::move(server), *this, host.getBlockedReason());
        addReporter([target = connection.get()](const core::Json& value) { target->report(value); });
        awaitLocalNetwork(host);
    }
}

// A server outside this device is on the local network, which a platform such as Android 17 lets an app reach only once the person grants it, so the connection waits for the answer instead of waiting for packets the platform drops.
void DevelopmentSession::awaitLocalNetwork(Host& host) {
    if (connection->isOnThisDevice()) {
        return;
    }
    const Permission::State state = host.getPermission(Permission::Kind::LocalNetwork);
    if (state == Permission::State::Granted || state == Permission::State::Unknown) {
        return;
    }

    // clang-format off
    host.requestPermission(Permission::Kind::LocalNetwork, [release = connection->hold()](Permission::Answer answer) {
        if (answer.state == Permission::State::Granted) {
            release(std::nullopt);
            return;
        }
        release(answer.message.empty() ? "The person did not grant the access to the local network that reaching it needs." : answer.message);
    });
    // clang-format on
}

DevelopmentSession::~DevelopmentSession() = default;

void DevelopmentSession::setOverlay(std::shared_ptr<io::OverlayPackage> value) {
    overlay = std::move(value);
}

void DevelopmentSession::pump() {
    if (connection) {
        connection->pump();
    }
}

// A path queued twice before the frame takes the batch counts once.
void DevelopmentSession::addChanges(std::span<const std::string> paths) {
    const std::scoped_lock lock(mutex);
    for (const std::string& path : paths) {
        if (std::ranges::find(pending, path) == pending.end()) {
            pending.push_back(path);
        }
    }
}

std::vector<std::string> DevelopmentSession::takeChanges() {
    const std::scoped_lock lock(mutex);
    return std::exchange(pending, {});
}

std::shared_ptr<DevelopmentSession::Scan> DevelopmentSession::beginScan() {
    if (watcher == nullptr || scanning.exchange(true, std::memory_order_acq_rel)) {
        return nullptr;
    }
    return std::make_shared<Scan>(*this);
}

DevelopmentSession::Scan::~Scan() {
    session.scanning.store(false, std::memory_order_release);
}

// A folder that cannot be read right now, such as one that a checkout replaces, waits for the next scan.
void DevelopmentSession::Scan::run() {
    try {
        const std::vector<std::string> changed = session.watcher->scan();
        session.addChanges(changed);
    } catch (const std::exception& error) {
        core::Log::warning("The package folder could not be scanned for changes: {}", error.what());
    }
}

void DevelopmentSession::addReporter(Reporter reporter) {
    const std::scoped_lock lock(mutex);
    reporters.push_back(std::move(reporter));
}

void DevelopmentSession::report(const core::Json& value) const {
    std::vector<Reporter> listeners;
    {
        const std::scoped_lock lock(mutex);
        listeners = reporters;
    }
    for (const Reporter& listener : listeners) {
        listener(value);
    }
}

} // namespace haylen::platform
