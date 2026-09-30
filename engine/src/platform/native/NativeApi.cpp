#include "platform/native/NativeApi.hpp"

#include <algorithm>

#include "haylen/core/Log.hpp"
#include "platform/BridgeRelay.hpp"

namespace haylen::platform {

const HaylenNativeApi NativeApi::api{.version = HAYLEN_NATIVE_API_VERSION, .emit = &emit, .resolve = &resolve, .registerHandler = &registerHandler, .log = &log, .registerPlugin = &registerPlugin, .registerErrorHandler = &registerErrorHandler};
std::mutex& NativeApi::mutex = *new std::mutex();
std::unordered_map<std::string, NativeApi::Handler>& NativeApi::handlers = *new std::unordered_map<std::string, Handler>();
std::set<std::string, std::less<>>& NativeApi::plugins = *new std::set<std::string, std::less<>>();
std::vector<NativeApi::ErrorHandler>& NativeApi::errorHandlers = *new std::vector<ErrorHandler>();

const HaylenNativeApi& NativeApi::get() noexcept {
    return api;
}

// The handler runs outside the lock, so it may register handlers or answer at once.
bool NativeApi::dispatch(std::uint64_t call, std::string_view method, std::string_view paramsJson) {
    const std::optional<Handler> found = find(method);
    if (!found) {
        return false;
    }
    found->handler(found->user, call, std::string(method).c_str(), std::string(paramsJson).c_str());
    return true;
}

bool NativeApi::cancel(std::uint64_t call, std::string_view method) {
    const std::optional<Handler> found = find(method);
    if (!found) {
        return false;
    }
    if (found->cancel != nullptr) {
        found->cancel(found->user, call);
    }
    return true;
}

std::optional<NativeApi::Handler> NativeApi::find(std::string_view method) {
    const std::scoped_lock lock(mutex);
    const auto found = handlers.find(std::string(method));
    if (found == handlers.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::vector<std::string> NativeApi::getPlugins() {
    const std::scoped_lock lock(mutex);
    return {plugins.begin(), plugins.end()};
}

// The handlers run outside the lock, so they may register more handlers or send events.
void NativeApi::reportError(const core::Json& report) {
    std::vector<ErrorHandler> snapshot;
    {
        const std::scoped_lock lock(mutex);
        snapshot = errorHandlers;
    }
    if (snapshot.empty()) {
        return;
    }

    const std::string text = report.dump(-1, ' ', false, core::Json::error_handler_t::replace);
    for (const ErrorHandler& entry : snapshot) {
        entry.handler(entry.user, text.c_str());
    }
}

void NativeApi::emit(const char* event, const char* payloadJson, int retain) {
    BridgeRelay::emit(event, payloadJson != nullptr ? payloadJson : "null", retain != 0);
}

void NativeApi::resolve(std::uint64_t call, int ok, const char* resultJson) {
    BridgeRelay::resolve(call, ok != 0, resultJson != nullptr ? resultJson : "null");
}

void NativeApi::registerHandler(const char* method, HaylenNativeHandler handler, HaylenNativeCancel cancel, void* user) {
    const std::scoped_lock lock(mutex);
    if (handler == nullptr) {
        handlers.erase(method);
        return;
    }
    handlers.insert_or_assign(method, Handler{.handler = handler, .cancel = cancel, .user = user});
}

void NativeApi::registerPlugin(const char* id) {
    if (id == nullptr || *id == '\0') {
        core::Log::error("A native library declared the native part of a plugin without its id.");
        return;
    }
    const std::scoped_lock lock(mutex);
    plugins.emplace(id);
}

void NativeApi::registerErrorHandler(HaylenNativeErrorHandler handler, void* user) {
    if (handler == nullptr) {
        core::Log::error("A native library registered an error handler without a function.");
        return;
    }
    const ErrorHandler entry{.handler = handler, .user = user};
    const std::scoped_lock lock(mutex);
    if (std::ranges::find(errorHandlers, entry) == errorHandlers.end()) {
        errorHandlers.push_back(entry);
    }
}

void NativeApi::log(int level, const char* text) {
    core::Log::write(static_cast<core::Log::Level>(std::clamp<int>(level, HAYLEN_NATIVE_LOG_DEBUG, HAYLEN_NATIVE_LOG_ERROR)), text != nullptr ? text : "");
}

} // namespace haylen::platform
