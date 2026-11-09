#include "platform/DevelopmentConnection.hpp"

#include <algorithm>
#include <exception>
#include <span>
#include <string_view>
#include <utility>

#include "debug/Debugger.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/core/Version.hpp"
#include "haylen/io/PackageWatcher.hpp"
#include "haylen/io/Path.hpp"
#include "io/OverlayPackage.hpp"
#include "platform/ConsoleRelay.hpp"
#include "platform/DevelopmentSession.hpp"

namespace haylen::platform {

DevelopmentConnection::DevelopmentConnection(std::string url, DevelopmentSession& owner, net::WebSocket::BlockedReason blockedReason) : session(owner), address(std::move(url)), explainBlocked(std::move(blockedReason)) {}

// The lines that the server sent and no frame ran yet would answer into the socket that goes away here.
DevelopmentConnection::~DevelopmentConnection() {
    ConsoleRelay::withdraw(this);
}

std::function<void(std::optional<std::string>)> DevelopmentConnection::hold() {
    gate = std::make_shared<Gate>();
    // clang-format off
    return [shared = std::weak_ptr<Gate>(gate)](std::optional<std::string> refusal) {
        if (const std::shared_ptr<Gate> alive = shared.lock()) {
            const std::scoped_lock lock(alive->mutex);
            alive->answered = true;
            alive->refusal = std::move(refusal);
        }
    };
    // clang-format on
}

// A refused connection says once why it never opens.
bool DevelopmentConnection::isHeld() {
    if (!gate) {
        return false;
    }
    const std::scoped_lock lock(gate->mutex);
    if (!gate->answered) {
        return true;
    }
    if (gate->refusal && !refused) {
        core::Log::warning("The app does not connect to the development server at \"{}\". {}", address, *gate->refusal);
        refused = true;
    }
    return gate->refusal.has_value();
}

// The address names its host between the scheme and the port or the path, in brackets for IPv6.
bool DevelopmentConnection::isOnThisDevice() const {
    const std::size_t start = address.find("://");
    std::string_view host = std::string_view(address).substr(start == std::string::npos ? 0 : start + 3);
    host = host.substr(0, host.find('/'));
    if (host.starts_with('[')) {
        host = host.substr(1, host.find(']') - 1);
    } else {
        host = host.substr(0, host.find(':'));
    }
    return host == "localhost" || host == "::1" || host.starts_with("127.");
}

void DevelopmentConnection::pump() {
    if (!socket) {
        open();
        return;
    }
    if (!held.empty() && !session.getDebugger().isHalted()) {
        for (const auto& [data, binary] : std::exchange(held, {})) {
            receive(data, binary);
        }
    }
    socket->pump();
}

void DevelopmentConnection::open() {
    if (isHeld()) {
        return;
    }
    socket = std::make_unique<net::WebSocket>(address, net::WebSocket::Options{.reconnect = {.enabled = true, .maxDelay = kMaxReconnectDelay}, .blockedReason = explainBlocked});
    socket->opened.connect([this] { sendHello(); });
    socket->received.connect([this](std::string_view data, bool binary) { receive(data, binary); });
    socket->failed.connect([](std::string_view message) { core::Log::warning("The development server is out of reach: {}", message); });
    socket->disconnected.connect([this](int, std::string_view) { session.getDebugger().reset(); });
}

// The session and revision of the last batch tell the server which files the app already has, so after a restart of the server or the app it sends only what changed since.
void DevelopmentConnection::sendHello() {
    batch = nullptr;
    contents.clear();
    core::Log::info("Connected to the development server at \"{}\".", socket->getUrl());
    socket->send(core::Json{{"type", "hello"}, {"session", serverSession}, {"revision", revision}, {"engine", core::Version::kString}}.dump());
}

void DevelopmentConnection::report(const core::Json& value) {
    if (!socket || socket->getState() != net::WebSocket::State::Open) {
        return;
    }
    core::Json message = value;
    message["type"] = "report";
    message["revision"] = revision;
    socket->send(message.dump());
}

// Lua strings may hold any bytes, which the text of the message replaces where they are no UTF-8.
void DevelopmentConnection::sendDebug(const core::Json& value) {
    if (socket && socket->getState() == net::WebSocket::State::Open) {
        socket->send(core::Json{{"type", "debug"}, {"message", value}}.dump(-1, ' ', false, core::Json::error_handler_t::replace));
    }
}

void DevelopmentConnection::receive(std::string_view data, bool binary) {
    const bool halted = session.getDebugger().isHalted();
    if (binary && halted) {
        held.emplace_back(data, true);
        return;
    }
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
    if (type == "debug" && message.contains("message")) {
        session.getDebugger().receive(message.at("message"));
        return;
    }
    if (halted) {
        held.emplace_back(data, false);
        return;
    }
    if (type == "shaderError") {
        core::Log::error("The shader \"{}\" did not compile: {}", readText(message, "path"), readText(message, "message"));
        return;
    }
    if (type == "console") {
        submitLine(message);
        return;
    }
    if (type == "consoleCommands") {
        listCommands();
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
    }, this);
    // clang-format on
}

// The terminal completes the lines the developer types from the commands of the app, which it asks for as its console opens and after every line.
void DevelopmentConnection::listCommands() {
    // clang-format off
    ConsoleRelay::requestCommands([this](const core::Json& commands) {
        if (socket->getState() == net::WebSocket::State::Open) {
            socket->send(core::Json{{"type", "consoleCommands"}, {"commands", commands}}.dump());
        }
    }, this);
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
