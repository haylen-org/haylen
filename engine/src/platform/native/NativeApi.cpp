#include "platform/native/NativeApi.hpp"

#include <algorithm>

#include "haylen/core/Log.hpp"
#include "platform/BridgeRelay.hpp"

namespace haylen::platform {

const HaylenNativeApi NativeApi::api{.version = HAYLEN_NATIVE_API_VERSION, .emit = &emit, .resolve = &resolve, .registerHandler = &registerHandler, .log = &log, .registerPlugin = &registerPlugin};
std::mutex& NativeApi::mutex = *new std::mutex();
std::unordered_map<std::string, NativeApi::Handler>& NativeApi::handlers = *new std::unordered_map<std::string, Handler>();
std::set<std::string, std::less<>>& NativeApi::plugins = *new std::set<std::string, std::less<>>();

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

void NativeApi::log(int level, const char* text) {
    core::Log::write(static_cast<core::Log::Level>(std::clamp<int>(level, HAYLEN_NATIVE_LOG_DEBUG, HAYLEN_NATIVE_LOG_ERROR)), text != nullptr ? text : "");
}

} // namespace haylen::platform
