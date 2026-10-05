#include "platform/DevelopmentSession.hpp"

#include <algorithm>
#include <exception>
#include <utility>

#include "haylen/core/Log.hpp"
#include "io/OverlayPackage.hpp"
#include "platform/DevelopmentConnection.hpp"

namespace haylen::platform {

DevelopmentSession::DevelopmentSession(std::optional<std::filesystem::path> folder, std::string server) {
    if (folder) {
        watcher = std::make_unique<io::PackageWatcher>(*folder);
        core::Log::info("Watching \"{}\" for changes.", folder->generic_string());
    }
    if (!server.empty()) {
        connection = std::make_unique<DevelopmentConnection>(std::move(server), *this);
        addReporter([target = connection.get()](const core::Json& value) { target->report(value); });
    }
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
