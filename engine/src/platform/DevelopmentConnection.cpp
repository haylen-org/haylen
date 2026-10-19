#include "platform/DevelopmentConnection.hpp"

#include <algorithm>
#include <exception>
#include <span>
#include <utility>

#include "haylen/core/Log.hpp"
#include "haylen/core/Version.hpp"
#include "haylen/io/PackageWatcher.hpp"
#include "haylen/io/Path.hpp"
#include "io/OverlayPackage.hpp"
#include "platform/ConsoleRelay.hpp"
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
    const std::string type = readText(message, "type");
    if (type == "shaderError") {
        core::Log::error("The shader \"{}\" did not compile: {}", readText(message, "path"), readText(message, "message"));
        return;
    }
    if (type == "console") {
        submitLine(message);
        return;
    }
    if (type == "restart") {
        session.requestRestart();
        return;
    }
    if (type != "files" || !isBatch(message)) {
        core::Log::warning("The development server sent a message that this engine does not know.");
        return;
    }
    batch = message;
    contents.clear();
    if (batch.at("files").empty()) {
        apply();
    }
}

// The developer types a line of the console on the machine that runs the development server, and the result goes back with the identifier of the line.
void DevelopmentConnection::submitLine(const core::Json& message) {
    core::Json id = message.contains("id") ? message.at("id") : core::Json();
    // clang-format off
    ConsoleRelay::submit(readText(message, "line"), [this, id = std::move(id)](const debug::Console::Result& result) {
        if (socket->getState() == net::WebSocket::State::Open) {
            socket->send(core::Json{{"type", "consoleResult"}, {"id", id}, {"ok", result.ok}, {"output", result.output}}.dump());
        }
    });
    // clang-format on
}

std::string DevelopmentConnection::readText(const core::Json& message, const char* key) {
    if (!message.is_object()) {
        return {};
    }
    const auto found = message.find(key);
    return found != message.end() && found->is_string() ? found->get<std::string>() : std::string();
}

// A batch lists files with their paths, and may list removed paths, the session of the server and its revision.
bool DevelopmentConnection::isBatch(const core::Json& message) {
    const auto files = message.find("files");
    if (files == message.end() || !files->is_array() || !std::ranges::all_of(*files, [](const core::Json& file) { return file.is_object() && file.contains("path") && file.at("path").is_string(); })) {
        return false;
    }
    const auto removed = message.find("removed");
    if (removed != message.end() && (!removed->is_array() || !std::ranges::all_of(*removed, &core::Json::is_string))) {
        return false;
    }
    const auto server = message.find("session");
    const auto number = message.find("revision");
    return (server == message.end() || server->is_string()) && (number == message.end() || number->is_number_integer());
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
        const std::string path = accept(files.at(index).at("path").get<std::string>());
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
