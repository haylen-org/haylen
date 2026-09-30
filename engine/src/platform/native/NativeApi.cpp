#include "platform/native/NativeApi.hpp"

#include <algorithm>
#include <exception>
#include <span>
#include <stdexcept>
#include <type_traits>

#include "haylen/core/Log.hpp"
#include "haylen/platform/PluginStreams.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/ScreenRelay.hpp"

namespace haylen::platform {

const HaylenNativeApi NativeApi::api{.version = HAYLEN_NATIVE_API_VERSION, .emit = &emit, .resolve = &resolve, .registerHandler = &registerHandler, .log = &log, .registerPlugin = &registerPlugin, .registerErrorHandler = &registerErrorHandler, .openVideoStream = &openVideoStream, .pushVideoFrame = &pushVideoFrame, .openAudioStream = &openAudioStream, .pushAudioFrames = &pushAudioFrames, .registerScreen = &registerScreen, .finishScreen = &finishScreen, .getWindow = &getWindow, .coverApp = &coverApp, .uncoverApp = &uncoverApp};
std::mutex& NativeApi::mutex = *new std::mutex();
std::unordered_map<std::string, NativeApi::Handler>& NativeApi::handlers = *new std::unordered_map<std::string, Handler>();
std::unordered_map<std::string, NativeApi::ScreenHandler>& NativeApi::screens = *new std::unordered_map<std::string, ScreenHandler>();
std::set<std::string, std::less<>>& NativeApi::plugins = *new std::set<std::string, std::less<>>();
std::vector<NativeApi::ErrorHandler>& NativeApi::errorHandlers = *new std::vector<ErrorHandler>();
std::optional<HaylenNativeWindow>& NativeApi::window = *new std::optional<HaylenNativeWindow>();
int NativeApi::covers = 0;

const HaylenNativeApi& NativeApi::get() noexcept {
    return api;
}

// The handler runs outside the lock, so it may register handlers or answer at once.
bool NativeApi::dispatch(std::uint64_t call, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers) {
    const std::optional<Handler> found = find(method);
    if (!found) {
        return false;
    }
    std::vector<HaylenNativeBuffer> views;
    views.reserve(buffers.size());
    for (const std::vector<std::byte>& buffer : buffers) {
        views.push_back({.data = buffer.data(), .size = buffer.size()});
    }
    found->handler(found->user, call, std::string(method).c_str(), std::string(paramsJson).c_str(), views.data(), views.size());
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

// The opener runs outside the lock, so it may register screens or end the screen at once.
bool NativeApi::openScreen(const ScreenRequest& request) {
    const std::optional<ScreenHandler> found = findScreen(request.plugin, request.screen);
    if (!found) {
        return false;
    }
    std::vector<HaylenNativeBuffer> views;
    views.reserve(request.params.buffers.size());
    for (const std::vector<std::byte>& buffer : request.params.buffers) {
        views.push_back({.data = buffer.data(), .size = buffer.size()});
    }
    found->open(found->user, request.id, request.params.json.dump().c_str(), views.data(), views.size());
    return true;
}

bool NativeApi::cancelScreen(std::string_view plugin, std::string_view name, std::uint64_t screen) {
    const std::optional<ScreenHandler> found = findScreen(plugin, name);
    if (!found) {
        return false;
    }
    if (found->cancel != nullptr) {
        found->cancel(found->user, screen);
    }
    return true;
}

std::optional<NativeApi::ScreenHandler> NativeApi::findScreen(std::string_view plugin, std::string_view name) {
    const std::scoped_lock lock(mutex);
    const auto found = screens.find(getScreenKey(plugin, name));
    if (found == screens.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::string NativeApi::getScreenKey(std::string_view plugin, std::string_view name) {
    return std::string(plugin) + "." + std::string(name);
}

bool NativeApi::isAppCovered() {
    const std::scoped_lock lock(mutex);
    return covers > 0;
}

void NativeApi::setWindow(const HaylenNativeWindow& value) {
    const std::scoped_lock lock(mutex);
    window = value;
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

template <typename Body> auto NativeApi::guard(const char* entry, Body&& body) noexcept -> decltype(body()) {
    try {
        return body();
    } catch (const std::exception& error) {
        core::Log::error("The native library call {} failed: {}", entry, error.what());
    }
    if constexpr (!std::is_void_v<decltype(body())>) {
        return {};
    }
}

std::vector<std::vector<std::byte>> NativeApi::copyBuffers(const HaylenNativeBuffer* buffers, std::size_t count) {
    std::vector<std::vector<std::byte>> copies;
    copies.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto* bytes = static_cast<const std::byte*>(buffers[index].data);
        copies.emplace_back(bytes, bytes + buffers[index].size);
    }
    return copies;
}

void NativeApi::emit(const char* event, const char* payloadJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount, int flags) {
    // clang-format off
    guard("emit", [&] {
        BridgeRelay::emit(event, payloadJson != nullptr ? payloadJson : "null", copyBuffers(buffers, bufferCount), {.retain = (flags & HAYLEN_NATIVE_EMIT_RETAIN) != 0, .batched = (flags & HAYLEN_NATIVE_EMIT_BATCHED) != 0});
    });
    // clang-format on
}

void NativeApi::resolve(std::uint64_t call, int ok, const char* resultJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount) {
    guard("resolve", [&] { BridgeRelay::resolve(call, ok != 0, resultJson != nullptr ? resultJson : "null", copyBuffers(buffers, bufferCount)); });
}

// A stream is owned by the registry for the life of the process, so its address is the handle that the library keeps.
HaylenNativeVideoStream* NativeApi::openVideoStream(const char* plugin, const char* name, int format, int width, int height) {
    // clang-format off
    return guard("openVideoStream", [&] {
        if (format != HAYLEN_NATIVE_PIXELS_RGBA8 && format != HAYLEN_NATIVE_PIXELS_BGRA8) {
            throw std::invalid_argument("A video stream takes the pixel format HAYLEN_NATIVE_PIXELS_RGBA8 or HAYLEN_NATIVE_PIXELS_BGRA8.");
        }
        const VideoStream::Format pixels = format == HAYLEN_NATIVE_PIXELS_RGBA8 ? VideoStream::Format::Rgba8 : VideoStream::Format::Bgra8;
        return reinterpret_cast<HaylenNativeVideoStream*>(PluginStreams::openVideo(plugin != nullptr ? plugin : "", name != nullptr ? name : "", pixels, width, height).get());
    });
    // clang-format on
}

void NativeApi::pushVideoFrame(HaylenNativeVideoStream* stream, const void* pixels, int width, int height, int stride, double timestamp) {
    // clang-format off
    guard("pushVideoFrame", [&] {
        if (stream == nullptr || pixels == nullptr || stride < 0) {
            throw std::invalid_argument("A video frame needs a stream, pixels and a stride of 0 or more.");
        }
        reinterpret_cast<VideoStream*>(stream)->push(static_cast<const std::byte*>(pixels), width, height, static_cast<std::size_t>(stride), timestamp);
    });
    // clang-format on
}

HaylenNativeAudioStream* NativeApi::openAudioStream(const char* plugin, const char* name, int sampleRate, int channels, int format, int capacityFrames) {
    // clang-format off
    return guard("openAudioStream", [&] {
        if (format != HAYLEN_NATIVE_SAMPLES_FLOAT32 && format != HAYLEN_NATIVE_SAMPLES_INT16) {
            throw std::invalid_argument("An audio stream takes the sample format HAYLEN_NATIVE_SAMPLES_FLOAT32 or HAYLEN_NATIVE_SAMPLES_INT16.");
        }
        if (sampleRate <= 0 || channels <= 0 || capacityFrames <= 0) {
            throw std::invalid_argument("An audio stream needs a sample rate, channels and room for at least one frame.");
        }
        const AudioStream::Format samples = format == HAYLEN_NATIVE_SAMPLES_FLOAT32 ? AudioStream::Format::Float32 : AudioStream::Format::Int16;
        return reinterpret_cast<HaylenNativeAudioStream*>(PluginStreams::openAudio(plugin != nullptr ? plugin : "", name != nullptr ? name : "", static_cast<std::uint32_t>(sampleRate), static_cast<std::uint32_t>(channels), samples, static_cast<std::size_t>(capacityFrames)).get());
    });
    // clang-format on
}

std::size_t NativeApi::pushAudioFrames(HaylenNativeAudioStream* stream, const void* samples, std::size_t frames) {
    // clang-format off
    return guard("pushAudioFrames", [&] {
        if (stream == nullptr || (samples == nullptr && frames > 0)) {
            throw std::invalid_argument("Audio frames need a stream and samples.");
        }
        AudioStream& target = *reinterpret_cast<AudioStream*>(stream);
        const std::size_t count = frames * target.getChannels();
        if (target.getFormat() == AudioStream::Format::Float32) {
            return target.push(std::span(static_cast<const float*>(samples), count));
        }
        return target.push(std::span(static_cast<const std::int16_t*>(samples), count));
    });
    // clang-format on
}

void NativeApi::registerHandler(const char* method, HaylenNativeHandler handler, HaylenNativeCancel cancel, void* user) {
    const std::scoped_lock lock(mutex);
    if (handler == nullptr) {
        handlers.erase(method);
        return;
    }
    handlers.insert_or_assign(method, Handler{.handler = handler, .cancel = cancel, .user = user});
}

void NativeApi::registerScreen(const char* plugin, const char* name, HaylenNativeScreenOpener open, HaylenNativeScreenCancel cancel, void* user) {
    if (plugin == nullptr || *plugin == '\0' || name == nullptr || *name == '\0') {
        core::Log::error("A native library registered a screen without its plugin or its name.");
        return;
    }
    const std::scoped_lock lock(mutex);
    if (open == nullptr) {
        screens.erase(getScreenKey(plugin, name));
        return;
    }
    screens.insert_or_assign(getScreenKey(plugin, name), ScreenHandler{.open = open, .cancel = cancel, .user = user});
}

void NativeApi::finishScreen(std::uint64_t screen, int ok, const char* resultJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount) {
    guard("finishScreen", [&] { ScreenRelay::finish(screen, ok != 0, resultJson != nullptr ? resultJson : "null", copyBuffers(buffers, bufferCount)); });
}

int NativeApi::getWindow(HaylenNativeWindow* target) {
    const std::scoped_lock lock(mutex);
    if (target == nullptr || !window) {
        return 0;
    }
    *target = *window;
    return 1;
}

void NativeApi::coverApp() {
    const std::scoped_lock lock(mutex);
    ++covers;
}

void NativeApi::uncoverApp() {
    {
        const std::scoped_lock lock(mutex);
        if (covers > 0) {
            --covers;
            return;
        }
    }
    core::Log::error("A native library uncovered the app without covering it first.");
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
