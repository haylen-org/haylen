#include "NativeTestLibrary.hpp"

#include <algorithm>
#include <thread>

const std::array<std::pair<std::string_view, NativeTestLibrary::Handler>, 5> NativeTestLibrary::kHandlers{{
    {"native_test.echo", &NativeTestLibrary::echo},
    {"native_test.echoBytes", &NativeTestLibrary::echoBytes},
    {"native_test.burst", &NativeTestLibrary::burst},
    {"native_test.fail", &NativeTestLibrary::failOnPurpose},
    {"native_test.wait", &NativeTestLibrary::wait},
}};

int NativeTestLibrary::initialize(const HaylenNativeApi* engine) {
    if (engine->version != HAYLEN_NATIVE_API_VERSION) {
        return 1;
    }

    NativeTestLibrary& library = get();
    library.api = engine;

    for (const auto& entry : kHandlers) {
        const std::string method(entry.first);
        const HaylenNativeCancel cancel = method == "native_test.wait" ? &NativeTestLibrary::cancelCall : nullptr;
        engine->registerHandler(method.c_str(), &NativeTestLibrary::handleCall, cancel, &library);
    }

    engine->registerScreen("native-test", "panel", &NativeTestLibrary::openScreen, &NativeTestLibrary::cancelScreen, &library);
    engine->registerErrorHandler(&NativeTestLibrary::handleAppFailure, &library);
    engine->registerPlugin("native-test");
    engine->log(HAYLEN_NATIVE_LOG_INFO, "The native test library is ready.");

    std::thread worker(&NativeTestLibrary::announce, engine);
    worker.detach();
    return 0;
}

int NativeTestLibrary::failInitialization() {
    return 7;
}

// The library lives as long as the process, since the engine may call its handlers until the process ends.
NativeTestLibrary& NativeTestLibrary::get() {
    static auto* library = new NativeTestLibrary();
    return *library;
}

void NativeTestLibrary::pushFrameLater(std::int32_t width, std::int32_t height, std::int32_t seed) const {
    std::thread worker(&NativeTestLibrary::pushFrame, api, width, height, seed);
    worker.detach();
}

void NativeTestLibrary::pushToneLater(std::int32_t count, std::int32_t value) const {
    std::thread worker(&NativeTestLibrary::pushTone, api, count, value);
    worker.detach();
}

bool NativeTestLibrary::isReopenRefused() const {
    const bool video = api->openVideoStream("native-test", "pattern", HAYLEN_NATIVE_PIXELS_RGBA8, 0, 0) == nullptr;
    const bool audio = api->openAudioStream("native-test", "tone", 8000, 2, HAYLEN_NATIVE_SAMPLES_INT16, 4000) == nullptr;
    return video && audio;
}

std::int32_t NativeTestLibrary::getErrorCount() const {
    return errorCount;
}

const char* NativeTestLibrary::getLastError() const {
    return lastError.c_str();
}

void NativeTestLibrary::closeScreenLater() const {
    std::thread worker(&NativeTestLibrary::closeScreen, api, screen);
    worker.detach();
}

std::intptr_t NativeTestLibrary::getWindow() const {
    HaylenNativeWindow window{};
    if (api->getWindow(&window) == 0) {
        return -1;
    }

    return reinterpret_cast<std::intptr_t>(window.handle);
}

void NativeTestLibrary::setCovered(bool covered) const {
    if (covered) {
        api->coverApp();
        return;
    }

    api->uncoverApp();
}

void NativeTestLibrary::handleCall(void* user, std::uint64_t call, const char* method, const char* paramsJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount) {
    const auto& library = *static_cast<const NativeTestLibrary*>(user);
    const Json params = read(paramsJson);
    const auto handler = std::ranges::find(kHandlers, std::string_view(method), &std::pair<std::string_view, Handler>::first);
    (library.*(handler->second))(call, params, std::span(buffers, bufferCount));
}

// The call of `native_test.wait` never answers by itself, so its cancel reports that the app ended it.
void NativeTestLibrary::cancelCall(void* user, std::uint64_t call) {
    const auto& library = *static_cast<const NativeTestLibrary*>(user);
    const std::string payload = write({{"call", call}});
    library.api->emit("native_test.cancelled", payload.c_str(), nullptr, 0, 0);
}

// Opens the screen `panel` of `native-test`, which sends `screenOpened` with its parameters and their bytes, and stays open until the test ends it or the app gives it up.
void NativeTestLibrary::openScreen(void* user, std::uint64_t screenId, const char* paramsJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount) {
    auto& library = *static_cast<NativeTestLibrary*>(user);
    library.screen = screenId;

    const std::string payload = write({{"params", read(paramsJson)}, {"buffers", bufferCount}});
    library.api->emit("native_test.screenOpened", payload.c_str(), buffers, bufferCount, 0);
}

// Closes the panel that the app gave up, which ends it as `cancelled`.
void NativeTestLibrary::cancelScreen(void* user, std::uint64_t screenId) {
    const auto& library = *static_cast<const NativeTestLibrary*>(user);
    const std::string failure = write({{"message", "The panel closed."}, {"code", "cancelled"}});
    library.api->finishScreen(screenId, 0, failure.c_str(), nullptr, 0);
}

// Keeps the report of the last error that stopped an app and counts the reports, which the tests read back.
void NativeTestLibrary::handleAppFailure(void* user, const char* reportJson) {
    auto& library = *static_cast<NativeTestLibrary*>(user);
    library.lastError = reportJson;
    ++library.errorCount;
}

std::string NativeTestLibrary::write(const Json& value) {
    return value.dump(-1, ' ', false, Json::error_handler_t::replace);
}

NativeTestLibrary::Json NativeTestLibrary::read(const char* text) {
    return Json::parse(text, nullptr, false);
}

// Answers from a thread of the library with the parameters it received.
void NativeTestLibrary::echo(std::uint64_t call, const Json& params, std::span<const HaylenNativeBuffer>) const {
    std::thread worker(&NativeTestLibrary::answerEcho, api, call, params);
    worker.detach();
}

// Copies the first buffer of the call for a thread of the library, which answers with those bytes and the same bytes reversed.
void NativeTestLibrary::echoBytes(std::uint64_t call, const Json&, std::span<const HaylenNativeBuffer> buffers) const {
    std::vector<std::uint8_t> bytes;
    if (!buffers.empty()) {
        const auto* first = static_cast<const std::uint8_t*>(buffers.front().data);
        bytes.assign(first, first + buffers.front().size);
    }

    std::thread worker(&NativeTestLibrary::answerBytes, api, call, std::move(bytes));
    worker.detach();
}

// Sends `count` batched events at once, each with one byte that counts them, which reach the app in one list.
void NativeTestLibrary::burst(std::uint64_t call, const Json& params, std::span<const HaylenNativeBuffer>) const {
    const auto found = params.find("count");
    const bool hasCount = params.is_object() && found != params.end() && found->is_number_integer();
    const std::int32_t count = hasCount ? found->get<std::int32_t>() : 0;

    for (std::int32_t index = 0; index < count; ++index) {
        const auto value = static_cast<std::uint8_t>(index);
        const HaylenNativeBuffer buffer{.data = &value, .size = 1};
        const std::string payload = write({{"index", index}, {"byte", {{"$bytes", 0}}}});
        api->emit("native_test.burst", payload.c_str(), &buffer, 1, HAYLEN_NATIVE_EMIT_BATCHED);
    }

    api->resolve(call, 1, "null", nullptr, 0);
}

void NativeTestLibrary::failOnPurpose(std::uint64_t call, const Json&, std::span<const HaylenNativeBuffer>) const {
    const std::string failure = write({{"message", "The native test failed on purpose."}, {"code", "native_test_failure"}, {"data", {{"reason", "requested"}}}});
    api->resolve(call, 0, failure.c_str(), nullptr, 0);
}

// Leaves the call pending until the app gives it up.
void NativeTestLibrary::wait(std::uint64_t, const Json&, std::span<const HaylenNativeBuffer>) const {}

void NativeTestLibrary::announce(const HaylenNativeApi* engine) {
    const std::string payload = write({{"version", engine->version}, {"origin", kOrigin}});
    engine->emit("native_test.ready", payload.c_str(), nullptr, 0, HAYLEN_NATIVE_EMIT_RETAIN);
}

void NativeTestLibrary::answerEcho(const HaylenNativeApi* engine, std::uint64_t call, const Json& params) {
    const std::string answer = write({{"echo", params}, {"thread", true}});
    engine->resolve(call, 1, answer.c_str(), nullptr, 0);
}

// Refers to the bytes it received and to the reversed bytes in the opposite order of the buffers.
void NativeTestLibrary::answerBytes(const HaylenNativeApi* engine, std::uint64_t call, const std::vector<std::uint8_t>& bytes) {
    const std::vector<std::uint8_t> reversed(bytes.rbegin(), bytes.rend());
    const std::array<HaylenNativeBuffer, 2> buffers{{{.data = reversed.data(), .size = reversed.size()}, {.data = bytes.data(), .size = bytes.size()}}};
    const std::string answer = write({{"same", {{"$bytes", 1}}}, {"reversed", {{"$bytes", 0}}}, {"size", bytes.size()}});
    engine->resolve(call, 1, answer.c_str(), buffers.data(), buffers.size());
}

void NativeTestLibrary::pushFrame(const HaylenNativeApi* engine, std::int32_t width, std::int32_t height, std::int32_t seed) {
    HaylenNativeVideoStream* stream = engine->openVideoStream("native-test", "pattern", HAYLEN_NATIVE_PIXELS_BGRA8, 0, 0);
    const auto columns = static_cast<std::size_t>(width);
    const auto rows = static_cast<std::size_t>(height);
    const std::size_t stride = columns * 4 + 4;
    std::vector<std::uint8_t> pixels(stride * rows);

    for (std::size_t row = 0; row < rows; ++row) {
        for (std::size_t column = 0; column < columns; ++column) {
            const std::size_t pixel = row * stride + column * 4;
            pixels[pixel] = static_cast<std::uint8_t>(seed);
            pixels[pixel + 1] = static_cast<std::uint8_t>(seed + 1);
            pixels[pixel + 2] = static_cast<std::uint8_t>(seed + 2);
            pixels[pixel + 3] = 255;
        }
    }

    engine->pushVideoFrame(stream, pixels.data(), width, height, static_cast<int>(stride), seed / 10.0);
}

void NativeTestLibrary::pushTone(const HaylenNativeApi* engine, std::int32_t count, std::int32_t value) {
    HaylenNativeAudioStream* stream = engine->openAudioStream("native-test", "tone", 8000, 1, HAYLEN_NATIVE_SAMPLES_INT16, 4000);
    const std::vector<std::int16_t> samples(static_cast<std::size_t>(count), static_cast<std::int16_t>(value));
    engine->pushAudioFrames(stream, samples.data(), samples.size());
}

void NativeTestLibrary::closeScreen(const HaylenNativeApi* engine, std::uint64_t screenId) {
    const std::string result = write({{"closed", true}, {"thread", true}});
    engine->finishScreen(screenId, 1, result.c_str(), nullptr, 0);
}
