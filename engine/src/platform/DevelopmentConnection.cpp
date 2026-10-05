#include "platform/DevelopmentConnection.hpp"

#include <exception>
#include <span>
#include <utility>

#include "haylen/core/Log.hpp"
#include "haylen/core/Version.hpp"
#include "haylen/io/PackageWatcher.hpp"
#include "haylen/io/Path.hpp"
#include "io/OverlayPackage.hpp"
#include "platform/DevelopmentSession.hpp"

namespace haylen::platform {

DevelopmentConnection::DevelopmentConnection(std::string url, DevelopmentSession& owner) : session(owner), socket(std::make_unique<net::WebSocket>(std::move(url), net::WebSocket::Options{.reconnect = {.enabled = true, .maxDelay = kMaxReconnectDelay}})) {
    socket->opened.connect([this] { sendHello(); });
    socket->received.connect([this](std::string_view data, bool binary) { receive(data, binary); });
    socket->failed.connect([](std::string_view message) { core::Log::warning("The development server is out of reach: {}", message); });
}

void DevelopmentConnection::pump() {
    socket->pump();
}

// The session and revision of the last batch tell the server which files the app already has, so after a restart of the server or the app it sends only what changed since.
void DevelopmentConnection::sendHello() {
    batch = nullptr;
    contents.clear();
    core::Log::info("Connected to the development server at \"{}\".", socket->getUrl());
    socket->send(core::Json{{"type", "hello"}, {"session", serverSession}, {"revision", revision}, {"engine", core::Version::kString}}.dump());
}

void DevelopmentConnection::report(const core::Json& value) {
    if (socket->getState() != net::WebSocket::State::Open) {
        return;
    }
    core::Json message = value;
    message["type"] = "report";
    message["revision"] = revision;
    socket->send(message.dump());
}

void DevelopmentConnection::receive(std::string_view data, bool binary) {
    if (binary) {
        if (batch.is_object()) {
            contents.emplace_back(reinterpret_cast<const std::uint8_t*>(data.data()), reinterpret_cast<const std::uint8_t*>(data.data()) + data.size());
            if (contents.size() == batch.at("files").size()) {
                apply();
            }
        }
        return;
    }

    const core::Json message = core::Json::parse(data, nullptr, false);
    const std::string type = message.is_object() ? message.value("type", "") : "";
    if (type == "shaderError") {
        core::Log::error("The shader \"{}\" did not compile: {}", message.value("path", ""), message.value("message", ""));
        return;
    }
    if (type != "files" || !message.contains("files") || !message.at("files").is_array()) {
        core::Log::warning("The development server sent a message that this engine does not know.");
        return;
    }
    batch = message;
    contents.clear();
    if (batch.at("files").empty()) {
        apply();
    }
}

// Only files of the package reach the overlay, so a path that is absolute, leaves the package or names anything but a file that hot reload watches is refused.
void DevelopmentConnection::apply() {
    io::OverlayPackage* overlay = session.getOverlay();
    std::vector<std::string> paths;
    // clang-format off
    const auto accept = [&paths](const std::string& path) -> std::string {
        try {
            std::string normalized = io::Path::normalize(path);
            if (io::PackageWatcher::isWatched(normalized)) {
                paths.push_back(normalized);
                return normalized;
            }
        } catch (const std::exception&) {
        }
        core::Log::warning("The development server sent the path \"{}\", which is not a file of the package, so it was refused.", path);
        return {};
    };
    // clang-format on

    const core::Json& files = batch.at("files");
    for (std::size_t index = 0; index < files.size(); ++index) {
        const std::string path = accept(files.at(index).value("path", ""));
        if (!path.empty() && overlay != nullptr) {
            overlay->setFile(path, std::move(contents[index]));
        }
    }
    for (const core::Json& removed : batch.value("removed", core::Json::array())) {
        const std::string path = accept(removed.get<std::string>());
        if (!path.empty() && overlay != nullptr) {
            overlay->removeFile(path);
        }
    }
    serverSession = batch.value("session", "");
    revision = batch.value("revision", std::int64_t{0});
    batch = nullptr;
    contents.clear();
    session.addChanges(paths);
}

} // namespace haylen::platform
