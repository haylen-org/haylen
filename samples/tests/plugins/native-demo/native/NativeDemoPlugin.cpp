#include "NativeDemoPlugin.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <numbers>
#include <thread>

#include "NativeDemoPicture.hpp"
#include "NativeDemoPng.hpp"
#include "NativeDemoScreen.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <shellapi.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <crt_externs.h>
#endif

const std::array<std::pair<std::string_view, NativeDemoPlugin::Handler>, 20> NativeDemoPlugin::kHandlers{{
    {"echo", &NativeDemoPlugin::echo}, {"echoBytes", &NativeDemoPlugin::echoBytes}, {"generatedImage", &NativeDemoPlugin::generatedImage}, {"captureFrame", &NativeDemoPlugin::captureFrame}, {"checksumView", &NativeDemoPlugin::checksumView}, {"startVideo", &NativeDemoPlugin::startVideo}, {"stopVideo", &NativeDemoPlugin::stopVideo}, {"startTone", &NativeDemoPlugin::startTone}, {"stopTone", &NativeDemoPlugin::stopTone}, {"burst", &NativeDemoPlugin::burst}, {"compute", &NativeDemoPlugin::compute}, {"fail", &NativeDemoPlugin::failOnPurpose}, {"wait", &NativeDemoPlugin::wait}, {"ticks", &NativeDemoPlugin::ticks}, {"start", &NativeDemoPlugin::start}, {"products", &NativeDemoPlugin::products}, {"restorePurchases", &NativeDemoPlugin::restorePurchases}, {"currentUser", &NativeDemoPlugin::currentUser}, {"signOut", &NativeDemoPlugin::signOut}, {"openApp", &NativeDemoPlugin::openApp},
}};

const std::array<std::pair<std::string_view, std::string_view>, 15> NativeDemoPlugin::kUnsupported{{
    {"config", "Native libraries receive no plugin parameters, so the desktop part leaves them to the Lua API of the plugin."},
    {"showScreen", "The desktops give native libraries no view API, so the plugin cannot show a native screen over the app."},
    {"pickFile", "The desktop systems share no API for a file picker, so the desktop part of the plugin has none."},
    {"startCamera", "The desktop systems capture the camera through APIs of their own, AVFoundation, Media Foundation and V4L2, which the C++ library of the plugin leaves to the native parts of real plugins."},
    {"stopCamera", "The desktop systems capture the camera through APIs of their own, AVFoundation, Media Foundation and V4L2, which the C++ library of the plugin leaves to the native parts of real plugins."},
    {"takePhoto", "The desktop systems capture the camera through APIs of their own, AVFoundation, Media Foundation and V4L2, which the C++ library of the plugin leaves to the native parts of real plugins."},
    {"startMicrophone", "The desktop systems record the microphone through APIs of their own, which the C++ library of the plugin leaves to the native parts of real plugins."},
    {"stopMicrophone", "The desktop systems record the microphone through APIs of their own, which the C++ library of the plugin leaves to the native parts of real plugins."},
    {"location", "The desktop systems tell the location through services of their own, which the C++ library of the plugin leaves to the native parts of real plugins."},
    {"share", "The desktop systems share no API for a share sheet, so the desktop part of the plugin has none."},
    {"showMap", "The desktops give native libraries no view API, so the plugin cannot place a map over the app."},
    {"removeMap", "The desktops give native libraries no view API, so the plugin cannot place a map over the app."},
    {"showBanner", "The desktops give native libraries no view API, so the plugin cannot place a banner over the app."},
    {"setBannerVisible", "The desktops give native libraries no view API, so the plugin cannot place a banner over the app."},
    {"removeBanner", "The desktops give native libraries no view API, so the plugin cannot place a banner over the app."},
}};

int NativeDemoPlugin::initialize(const HaylenNativeApi* api) {
    if (api->version != HAYLEN_NATIVE_API_VERSION) {
        return 1;
    }
    auto* plugin = new NativeDemoPlugin(*api);

    for (const auto& [name, handler] : kHandlers) {
        const std::string method = std::string(kPrefix) + std::string(name);
        api->registerHandler(method.c_str(), &NativeDemoPlugin::handleCall, name == "wait" ? &NativeDemoPlugin::cancelCall : nullptr, plugin);
    }
    for (const auto& [name, message] : kUnsupported) {
        const std::string method = std::string(kPrefix) + std::string(name);
        api->registerHandler(method.c_str(), &NativeDemoPlugin::handleCall, nullptr, plugin);
    }
    api->registerScreen("native-demo", "confirm", &NativeDemoPlugin::openConfirm, &NativeDemoPlugin::cancelScreen, plugin);
    api->registerScreen("native-demo", "purchase", &NativeDemoPlugin::openPurchase, &NativeDemoPlugin::cancelScreen, plugin);
    api->registerScreen("native-demo", "interstitial", &NativeDemoPlugin::openInterstitial, &NativeDemoPlugin::cancelScreen, plugin);
    api->registerScreen("native-demo", "rewarded", &NativeDemoPlugin::openRewarded, &NativeDemoPlugin::cancelScreen, plugin);
    api->registerScreen("native-demo", "signIn", &NativeDemoPlugin::openSignIn, &NativeDemoPlugin::cancelScreen, plugin);
    api->registerErrorHandler(&NativeDemoPlugin::handleAppFailure, plugin);
    api->registerPlugin("native-demo");

    plugin->emit("native-demo.loaded", {{"language", kLanguage}, {"platform", kPlatform}}, HAYLEN_NATIVE_EMIT_RETAIN);
    return 0;
}

NativeDemoPlugin::NativeDemoPlugin(const HaylenNativeApi& engine) : api(engine) {}

// The engine hands JSON text, which the library reads without exceptions, since none may cross the C interface of the engine.
void NativeDemoPlugin::handleCall(void* user, std::uint64_t call, const char* method, const char* paramsJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount) {
    auto& plugin = *static_cast<NativeDemoPlugin*>(user);
    const std::string_view name = std::string_view(method).substr(kPrefix.size());
    const Json params = Json::parse(paramsJson, nullptr, false);
    const Call request{.id = call, .params = params, .buffers = std::span(buffers, bufferCount)};

    const auto handler = std::ranges::find(kHandlers, name, &std::pair<std::string_view, Handler>::first);
    if (handler != kHandlers.end()) {
        (plugin.*(handler->second))(request);
        return;
    }
    const auto unsupported = std::ranges::find(kUnsupported, name, &std::pair<std::string_view, std::string_view>::first);
    plugin.failUnsupported(call, unsupported->second);
}

// The call of `wait` never answers by itself, so its cancel reports that the app ended it.
void NativeDemoPlugin::cancelCall(void* user, std::uint64_t call) {
    auto& plugin = *static_cast<NativeDemoPlugin*>(user);
    const auto found = std::ranges::find(plugin.waits, call, &Wait::call);
    if (found == plugin.waits.end()) {
        return;
    }
    plugin.emit("native-demo.waitCancelled", {{"token", found->token}, {"language", kLanguage}});
    plugin.waits.erase(found);
}

// Opens the confirm screen, a native window over the window of the app, with the title and the question of the parameters.
void NativeDemoPlugin::openConfirm(void* user, std::uint64_t screen, const char* paramsJson, const HaylenNativeBuffer*, std::size_t) {
    const auto& plugin = *static_cast<const NativeDemoPlugin*>(user);
    const Json params = Json::parse(paramsJson, nullptr, false);
    const std::string_view via = "a window over the window of the app";
    NativeDemoScreen::Content content{
        .title = readText(params, "title", "Native Demo"),
        .question = readText(params, "question", ""),
        .confirmLabel = "Confirm",
        .declineLabel = "Decline",
        .confirmed = write({{"confirmed", true}, {"via", via}, {"language", kLanguage}}),
        .declined = write({{"confirmed", false}, {"via", via}, {"language", kLanguage}}),
        .closed = write({{"message", "The person closed the confirm screen."}, {"code", "cancelled"}}),
        .onConfirm = {},
    };
    NativeDemoScreen::open(plugin.api, screen, std::move(content));
}

// The purchase screen of the fake store, which asks the person to buy the product and charges nothing.
void NativeDemoPlugin::openPurchase(void* user, std::uint64_t screen, const char* paramsJson, const HaylenNativeBuffer*, std::size_t) {
    auto& plugin = *static_cast<NativeDemoPlugin*>(user);
    const Json params = Json::parse(paramsJson, nullptr, false);
    const std::string id = readText(params, "productId", "");
    const NativeDemoStore::Product* product = NativeDemoStore::findProduct(id);
    if (product == nullptr) {
        const std::string failure = write({{"message", "The store has no product \"" + id + "\"."}, {"code", "unknownProduct"}});
        plugin.api.finishScreen(screen, 0, failure.c_str(), nullptr, 0);
        return;
    }

    const std::string transaction = plugin.store.newTransaction();
    plugin.store.beginPurchase(*product, transaction);
    // clang-format off
    NativeDemoScreen::Content content{
        .title = std::string(product->title),
        .question = std::string(product->description) + " This purchase is a simulation, and nothing is charged.",
        .confirmLabel = "Buy for " + std::string(product->price) + " USD",
        .declineLabel = "Cancel",
        .confirmed = write({{"productId", product->id}, {"transactionId", transaction}, {"receipt", "receipt-" + transaction}, {"language", kLanguage}}),
        .declined = std::nullopt,
        .closed = write({{"message", "The person cancelled the purchase."}, {"code", "cancelled"}}),
        .onConfirm = [&plugin] { plugin.purchased(); },
    };
    // clang-format on
    NativeDemoScreen::open(plugin.api, screen, std::move(content));
}

void NativeDemoPlugin::openInterstitial(void* user, std::uint64_t screen, const char*, const HaylenNativeBuffer*, std::size_t) {
    const auto& plugin = *static_cast<const NativeDemoPlugin*>(user);
    NativeDemoScreen::Content content{
        .title = "Demo ad",
        .question = "A fake full-screen ad of the demo plugin, which covers the app until it closes.",
        .confirmLabel = "Continue to the app",
        .declineLabel = std::nullopt,
        .confirmed = write({{"closed", true}, {"language", kLanguage}}),
        .declined = std::nullopt,
        .closed = write({{"message", "The person closed the ad."}, {"code", "cancelled"}}),
        .onConfirm = {},
    };
    NativeDemoScreen::open(plugin.api, screen, std::move(content));
}

void NativeDemoPlugin::openRewarded(void* user, std::uint64_t screen, const char*, const HaylenNativeBuffer*, std::size_t) {
    const auto& plugin = *static_cast<const NativeDemoPlugin*>(user);
    // clang-format off
    NativeDemoScreen::Content content{
        .title = "Demo rewarded ad",
        .question = "Watch this fake ad to the end to earn 50 coins.",
        .confirmLabel = "Watch to the end",
        .declineLabel = "Skip",
        .confirmed = write({{"rewarded", true}, {"amount", 50}, {"currency", "coins"}, {"language", kLanguage}}),
        .declined = write({{"rewarded", false}, {"amount", 0}, {"currency", "coins"}, {"language", kLanguage}}),
        .closed = write({{"message", "The person closed the ad."}, {"code", "cancelled"}}),
        .onConfirm = [&plugin] { plugin.rewarded(); },
    };
    // clang-format on
    NativeDemoScreen::open(plugin.api, screen, std::move(content));
}

// The sign-in screen of the fake account, which signs in one account without a password or the network.
void NativeDemoPlugin::openSignIn(void* user, std::uint64_t screen, const char*, const HaylenNativeBuffer*, std::size_t) {
    auto& plugin = *static_cast<NativeDemoPlugin*>(user);
    const std::string token = plugin.store.newTransaction();
    plugin.store.beginSignIn(token);
    // clang-format off
    NativeDemoScreen::Content content{
        .title = "Sign in to the demo",
        .question = "Sign in with a fake account. No password and no network take part.",
        .confirmLabel = "Sign in as Ana Souza",
        .declineLabel = "Cancel",
        .confirmed = write(describeAccount(token)),
        .declined = std::nullopt,
        .closed = write({{"message", "The person cancelled the sign-in."}, {"code", "cancelled"}}),
        .onConfirm = [&plugin] { plugin.signedIn(); },
    };
    // clang-format on
    NativeDemoScreen::open(plugin.api, screen, std::move(content));
}

void NativeDemoPlugin::cancelScreen(void*, std::uint64_t screen) {
    NativeDemoScreen::cancel(screen);
}

// Keeps the message, the file and the line of the error that stopped the app, which the next app receives when it sends `start`.
void NativeDemoPlugin::handleAppFailure(void* user, const char* reportJson) {
    auto& plugin = *static_cast<NativeDemoPlugin*>(user);
    const Json report = Json::parse(reportJson, nullptr, false);
    Json error = {{"message", ""}, {"file", ""}, {"line", 0}, {"language", kLanguage}};
    for (const char* key : {"message", "file", "line"}) {
        if (report.is_object() && report.contains(key)) {
            error[key] = report.at(key);
        }
    }
    plugin.lastError = write(error);
}

std::string NativeDemoPlugin::write(const Json& value) {
    return value.dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::optional<double> NativeDemoPlugin::readNumber(const Json& params, const char* key) {
    const auto found = params.find(key);
    if (!params.is_object() || found == params.end() || !found->is_number()) {
        return std::nullopt;
    }
    return found->get<double>();
}

std::string NativeDemoPlugin::readText(const Json& params, const char* key, std::string_view fallback) {
    const auto found = params.find(key);
    if (!params.is_object() || found == params.end() || !found->is_string()) {
        return std::string(fallback);
    }
    return found->get<std::string>();
}

void NativeDemoPlugin::succeed(std::uint64_t call, const Json& result, std::span<const HaylenNativeBuffer> buffers) const {
    const std::string text = write(result);
    api.resolve(call, 1, text.c_str(), buffers.data(), buffers.size());
}

void NativeDemoPlugin::fail(std::uint64_t call, std::string_view message, std::string_view code) const {
    const std::string text = write({{"message", message}, {"code", code}});
    api.resolve(call, 0, text.c_str(), nullptr, 0);
}

void NativeDemoPlugin::failUnsupported(std::uint64_t call, std::string_view message) const {
    const std::string text = write({{"message", message}, {"code", "unsupported"}, {"data", {{"language", kLanguage}}}});
    api.resolve(call, 0, text.c_str(), nullptr, 0);
}

void NativeDemoPlugin::emit(std::string_view event, const Json& payload, int flags) const {
    const std::string name(event);
    const std::string text = write(payload);
    api.emit(name.c_str(), text.c_str(), nullptr, 0, flags);
}

unsigned NativeDemoPlugin::restart(Work work) {
    return generations[static_cast<std::size_t>(work)].fetch_add(1) + 1;
}

bool NativeDemoPlugin::isCurrent(Work work, unsigned generation) const {
    return generations[static_cast<std::size_t>(work)].load() == generation;
}

void NativeDemoPlugin::echo(const Call& call) {
    const auto value = call.params.find("value");
    const bool given = call.params.is_object() && value != call.params.end();
    succeed(call.id, {{"echo", given ? *value : Json(nullptr)}, {"thread", "frame"}, {"language", kLanguage}});
}

// Answers with the bytes it received, which stay valid while the handler runs, since `resolve` copies them.
void NativeDemoPlugin::echoBytes(const Call& call) {
    if (call.buffers.empty()) {
        fail(call.id, "The method \"echoBytes\" needs bytes.", "invalidParams");
        return;
    }
    succeed(call.id, {{"data", {{"$bytes", 0}}}, {"size", call.buffers[0].size}, {"thread", "frame"}, {"language", kLanguage}}, call.buffers.first(1));
}

// Draws the picture natively and answers with it as the bytes of a PNG file.
void NativeDemoPlugin::generatedImage(const Call& call) {
    const double width = readNumber(call.params, "width").value_or(0);
    const double height = readNumber(call.params, "height").value_or(0);
    if (width < 1 || height < 1 || width > kMaxFrameSide || height > kMaxFrameSide) {
        fail(call.id, "The method \"generatedImage\" needs a width and a height from 1 to 2048.", "invalidParams");
        return;
    }

    const auto columns = static_cast<int>(width);
    const auto rows = static_cast<int>(height);
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows) * 4);
    NativeDemoPicture::drawIsland(pixels, columns, rows, 0, false);
    const std::vector<std::uint8_t> png = NativeDemoPng::encode(pixels, columns, rows);
    const HaylenNativeBuffer buffer{.data = png.data(), .size = png.size()};
    succeed(call.id, {{"png", {{"$bytes", 0}}}, {"width", columns}, {"height", rows}, {"drawnWith", "a PNG encoder in C++"}, {"language", kLanguage}}, std::span(&buffer, 1));
}

// Answers with a frame of RGBA pixels in a data view that the library allocates, so the app uploads it into a texture without a copy. The reference of the library goes back once the engine took its own from the answer.
void NativeDemoPlugin::captureFrame(const Call& call) {
    const double width = readNumber(call.params, "width").value_or(0);
    const double height = readNumber(call.params, "height").value_or(0);
    if (width < 1 || height < 1 || width > kMaxFrameSide || height > kMaxFrameSide) {
        fail(call.id, "The method \"captureFrame\" needs a width and a height from 1 to 2048.", "invalidParams");
        return;
    }

    const auto columns = static_cast<int>(width);
    const auto rows = static_cast<int>(height);
    const std::size_t size = static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows) * 4;
    varn_dataview* frame = api.newView(size);
    if (frame == nullptr) {
        fail(call.id, "The frame could not be allocated.", "outOfMemory");
        return;
    }
    const unsigned index = ++capturedFrames;
    NativeDemoPicture::fillCameraFrame(std::span(static_cast<std::uint8_t*>(api.viewData(frame)), size), columns, rows, index);

    std::array<char, 128> token{};
    api.viewToken(frame, token.data(), token.size());
    succeed(call.id, {{"frame", Json::parse(token.data(), nullptr, false)}, {"width", columns}, {"height", rows}, {"index", index}, {"language", kLanguage}});
    api.releaseView(frame);
}

// Reads the view of the parameters in place, answers with the sum of its bytes, and writes a mark into its first bytes that the app reads back from the same memory.
void NativeDemoPlugin::checksumView(const Call& call) {
    const auto token = call.params.find("view");
    const bool given = call.params.is_object() && token != call.params.end() && token->is_object();
    const std::optional<double> id = given ? readNumber(*token, "$view") : std::nullopt;
    const std::optional<double> size = given ? readNumber(*token, "size") : std::nullopt;
    const std::string pointerText = given ? readText(*token, "pointer", "") : "";
    std::uintptr_t pointer = 0;
    const bool hexadecimal = pointerText.starts_with("0x") && std::from_chars(pointerText.data() + 2, pointerText.data() + pointerText.size(), pointer, 16).ec == std::errc();
    varn_dataview* view = id && size && *size >= 0 && hexadecimal ? api.acquireView(static_cast<unsigned long long>(*id), reinterpret_cast<const void*>(pointer), static_cast<std::size_t>(*size)) : nullptr;
    if (view == nullptr) {
        fail(call.id, "The method \"checksumView\" needs a data view.", "invalidParams");
        return;
    }

    const std::span bytes(static_cast<std::uint8_t*>(api.viewData(view)), api.viewSize(view));
    std::uint32_t checksum = 0;
    for (const std::uint8_t byte : bytes) {
        checksum += byte;
    }
    constexpr std::string_view mark = "NATIVE";
    if (bytes.size() >= mark.size()) {
        std::ranges::copy(mark, bytes.begin());
    }
    succeed(call.id, {{"checksum", checksum}, {"size", bytes.size()}, {"mark", mark}, {"language", kLanguage}});
    api.releaseView(view);
}

void NativeDemoPlugin::startVideo(const Call& call) {
    const unsigned generation = restart(Work::Video);
    HaylenNativeVideoStream* stream = api.openVideoStream("native-demo", "island", HAYLEN_NATIVE_PIXELS_BGRA8, kVideoWidth, kVideoHeight);
    // clang-format off
    std::thread([this, generation, stream] { playVideo(generation, stream); }).detach();
    // clang-format on
    succeed(call.id, {{"width", kVideoWidth}, {"height", kVideoHeight}, {"fps", NativeDemoPicture::kFramesPerSecond}, {"format", "BGRA"}, {"thread", "a thread of the library"}, {"language", kLanguage}});
}

void NativeDemoPlugin::stopVideo(const Call& call) {
    restart(Work::Video);
    succeed(call.id, nullptr);
}

void NativeDemoPlugin::startTone(const Call& call) {
    const unsigned generation = restart(Work::Tone);
    const double frequency = readNumber(call.params, "frequency").value_or(440);
    HaylenNativeAudioStream* stream = api.openAudioStream("native-demo", "tone", kToneRate, 1, HAYLEN_NATIVE_SAMPLES_INT16, kToneRate);
    // clang-format off
    std::thread([this, generation, stream, frequency] { playTone(generation, stream, frequency); }).detach();
    // clang-format on
    succeed(call.id, {{"frequency", frequency}, {"sampleRate", kToneRate}, {"channels", 1}, {"format", "int16"}, {"language", kLanguage}});
}

void NativeDemoPlugin::stopTone(const Call& call) {
    restart(Work::Tone);
    succeed(call.id, nullptr);
}

void NativeDemoPlugin::burst(const Call& call) {
    const unsigned generation = restart(Work::Burst);
    const auto count = static_cast<int>(readNumber(call.params, "count").value_or(100));
    const auto limit = static_cast<int>(readNumber(call.params, "ticks").value_or(30));
    // clang-format off
    std::thread([this, generation, count, limit] { sendBursts(generation, count, limit); }).detach();
    // clang-format on
    succeed(call.id, {{"count", count}, {"ticks", limit}, {"language", kLanguage}});
}

void NativeDemoPlugin::compute(const Call& call) {
    const std::optional<double> limit = readNumber(call.params, "limit");
    if (!limit) {
        fail(call.id, "The method \"compute\" needs a limit.", "invalidParams");
        return;
    }
    const auto bound = static_cast<long long>(*limit);
    const std::uint64_t id = call.id;
    // clang-format off
    std::thread([this, id, bound] { countPrimes(id, bound); }).detach();
    // clang-format on
}

void NativeDemoPlugin::failOnPurpose(const Call& call) {
    const std::string text = write({{"message", "The native demo failed on purpose."}, {"code", "demoFailure"}, {"data", {{"reason", "requested"}, {"language", kLanguage}}}});
    api.resolve(call.id, 0, text.c_str(), nullptr, 0);
}

// The call never answers by itself, so only a cancel or a timeout of the app ends it, which `cancelCall` reports.
void NativeDemoPlugin::wait(const Call& call) {
    if (waits.size() >= kMaxWaits) {
        fail(call.id, "Too many waits are pending.", "busy");
        return;
    }
    const auto token = call.params.find("token");
    const bool given = call.params.is_object() && token != call.params.end();
    waits.push_back({.call = call.id, .token = given ? *token : Json(nullptr)});
}

void NativeDemoPlugin::ticks(const Call& call) {
    const auto enabled = call.params.find("enabled");
    const std::optional<double> interval = readNumber(call.params, "interval");
    if (!call.params.is_object() || enabled == call.params.end() || !enabled->is_boolean() || !interval) {
        fail(call.id, "The method \"ticks\" needs \"enabled\" and \"interval\".", "invalidParams");
        return;
    }

    const unsigned generation = restart(Work::Ticks);
    const bool starts = enabled->get<bool>();
    const double seconds = *interval;
    if (starts) {
        // clang-format off
        std::thread([this, generation, seconds] { sendTicks(generation, seconds); }).detach();
        // clang-format on
    }
    succeed(call.id, {{"enabled", starts}, {"interval", seconds}});
}

// Every app that loads the Lua API sends `start`. The library stops the ticks, the streams and the bursts that an earlier app of the process left running and hands the new app the error that stopped the earlier one.
void NativeDemoPlugin::start(const Call& call) {
    for (const Work work : {Work::Ticks, Work::Video, Work::Tone, Work::Burst}) {
        restart(work);
    }
    if (lastError) {
        api.emit("native-demo.lastError", lastError->c_str(), nullptr, 0, HAYLEN_NATIVE_EMIT_RETAIN);
        lastError.reset();
    }
    succeed(call.id, nullptr);
}

void NativeDemoPlugin::products(const Call& call) {
    Json listed = Json::array();
    for (const NativeDemoStore::Product& product : NativeDemoStore::kProducts) {
        listed.push_back({{"id", product.id}, {"title", product.title}, {"description", product.description}, {"price", product.price}, {"currency", "USD"}, {"kind", product.keeps ? "nonConsumable" : "consumable"}});
    }
    succeed(call.id, listed);
}

// Answers the products that stay bought and tells the app of each one as restored.
void NativeDemoPlugin::restorePurchases(const Call& call) {
    Json restored = Json::array();
    for (const NativeDemoStore::Purchase& purchase : store.getKeptPurchases()) {
        emit("native-demo.purchaseUpdated", {{"productId", purchase.productId}, {"transactionId", purchase.transactionId}, {"state", "restored"}, {"language", kLanguage}});
        restored.push_back({{"productId", purchase.productId}, {"transactionId", purchase.transactionId}});
    }
    succeed(call.id, restored);
}

void NativeDemoPlugin::currentUser(const Call& call) {
    const std::optional<std::string> token = store.getToken();
    succeed(call.id, token ? describeAccount(*token) : Json(nullptr));
}

void NativeDemoPlugin::signOut(const Call& call) {
    store.signOut();
    emit("native-demo.userChanged", nullptr);
    succeed(call.id, nullptr);
}

// Opens another app through the shell of the system: the settings where the system has a link to them, and a new message of the mail app. The desktops have no map app that every system knows, besides the one of macOS.
void NativeDemoPlugin::openApp(const Call& call) {
    const std::string kind = readText(call.params, "kind", "");
    std::string url;
    if (kind == "mail") {
        url = "mailto:demo@example.com?subject=Native%20Demo";
    }
#if defined(_WIN32)
    if (kind == "settings") {
        url = "ms-settings:";
    }
#elif defined(__APPLE__)
    if (kind == "settings") {
        url = "x-apple.systempreferences:";
    }
    if (kind == "maps") {
        url = "maps://?ll=-22.9519,-43.2105";
    }
#endif
    if (url.empty()) {
        failUnsupported(call.id, "This desktop has no app that every system opens for \"" + kind + "\".");
        return;
    }

    // The shell of Windows takes the url at once, while the other systems wait for their opener on a thread of the library.
#if defined(_WIN32)
    runOpener(call.id, url);
#else
    const std::uint64_t id = call.id;
    // clang-format off
    std::thread([this, id, url] { runOpener(id, url); }).detach();
    // clang-format on
#endif
}

// Pushes a frame of the picture into the video stream 30 times per second, with the seconds since the start as its timestamp.
void NativeDemoPlugin::playVideo(unsigned generation, HaylenNativeVideoStream* stream) const {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kVideoWidth) * kVideoHeight * 4);
    const auto started = std::chrono::steady_clock::now();
    for (int frame = 0; isCurrent(Work::Video, generation); ++frame) {
        NativeDemoPicture::drawIsland(pixels, kVideoWidth, kVideoHeight, frame, true);
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - started;
        api.pushVideoFrame(stream, pixels.data(), kVideoWidth, kVideoHeight, kVideoWidth * 4, elapsed.count());
        std::this_thread::sleep_until(started + std::chrono::duration<double>(static_cast<double>(frame + 1) / NativeDemoPicture::kFramesPerSecond));
    }
}

// Synthesizes a sine wave a tenth of a second ahead of the clock, so the voice that plays it never runs dry, while the ring of the stream drops what does not fit when no voice plays it.
void NativeDemoPlugin::playTone(unsigned generation, HaylenNativeAudioStream* stream, double frequency) const {
    std::array<std::int16_t, kToneBlock> samples{};
    const double step = 2.0 * std::numbers::pi * frequency / kToneRate;
    const auto started = std::chrono::steady_clock::now();
    double phase = 0.0;
    double written = 0.0;
    while (isCurrent(Work::Tone, generation)) {
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - started;
        while (written < (elapsed.count() + 0.1) * kToneRate) {
            for (std::int16_t& sample : samples) {
                sample = static_cast<std::int16_t>(std::sin(phase) * 0.3 * 32767.0);
                phase = std::fmod(phase + step, 2.0 * std::numbers::pi);
            }
            api.pushAudioFrames(stream, samples.data(), samples.size());
            written += kToneBlock;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// Sends `count` batched events 30 times per second for `limit` ticks, which reach the app as one list per frame, and then `burstDone`.
void NativeDemoPlugin::sendBursts(unsigned generation, int count, int limit) const {
    const auto started = std::chrono::steady_clock::now();
    for (int tick = 0; tick < limit; ++tick) {
        if (!isCurrent(Work::Burst, generation)) {
            return;
        }
        for (int index = 0; index < count; ++index) {
            emit("native-demo.burst", {{"tick", tick}, {"index", index}, {"language", kLanguage}}, HAYLEN_NATIVE_EMIT_BATCHED);
        }
        std::this_thread::sleep_until(started + std::chrono::duration<double>(static_cast<double>(tick + 1) / kBurstsPerSecond));
    }
    emit("native-demo.burstDone", {{"events", count * limit}, {"ticks", limit}, {"language", kLanguage}});
}

void NativeDemoPlugin::sendTicks(unsigned generation, double seconds) const {
    const auto interval = std::chrono::duration<double>(seconds);
    for (int count = 1;; ++count) {
        std::this_thread::sleep_for(interval);
        if (!isCurrent(Work::Ticks, generation)) {
            return;
        }
        emit("native-demo.tick", {{"count", count}, {"thread", "background"}, {"language", kLanguage}});
    }
}

void NativeDemoPlugin::countPrimes(std::uint64_t call, long long limit) const {
    std::vector<bool> composite(static_cast<std::size_t>(std::max(limit, 0LL)));
    long long count = 0;
    for (long long number = 2; number < limit; ++number) {
        if (composite[static_cast<std::size_t>(number)]) {
            continue;
        }
        ++count;
        for (long long multiple = number * number; multiple < limit; multiple += number) {
            composite[static_cast<std::size_t>(multiple)] = true;
        }
    }
    succeed(call, {{"primes", count}, {"thread", "background"}, {"detail", "a thread of the library"}, {"language", kLanguage}});
}

// Hands the url to the opener of the system, waits for it to take the url over, and answers whether it did.
void NativeDemoPlugin::runOpener(std::uint64_t call, const std::string& url) const {
#if defined(_WIN32)
    const bool opened = reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
    succeed(call, {{"opened", opened}, {"language", kLanguage}});
#else
#if defined(__APPLE__)
    std::string opener = "open";
    char** environment = *_NSGetEnviron();
#else
    std::string opener = "xdg-open";
    char** environment = environ;
#endif
    std::string address = url;
    std::array<char*, 3> arguments{opener.data(), address.data(), nullptr};
    pid_t child = 0;
    int status = 1;
    const bool spawned = posix_spawnp(&child, opener.c_str(), nullptr, nullptr, arguments.data(), environment) == 0;
    const bool opened = spawned && waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    succeed(call, {{"opened", opened}, {"language", kLanguage}});
#endif
}

void NativeDemoPlugin::purchased() {
    const NativeDemoStore::Purchase purchase = store.completePurchase();
    emit("native-demo.purchaseUpdated", {{"productId", purchase.productId}, {"transactionId", purchase.transactionId}, {"state", "purchased"}, {"language", kLanguage}});
}

void NativeDemoPlugin::signedIn() {
    const std::string token = store.completeSignIn();
    emit("native-demo.userChanged", describeAccount(token));
}

void NativeDemoPlugin::rewarded() const {
    emit("native-demo.adRewarded", {{"amount", 50}, {"currency", "coins"}, {"language", kLanguage}});
}

NativeDemoPlugin::Json NativeDemoPlugin::describeAccount(std::string_view token) {
    return {{"userId", "demo-ana"}, {"name", "Ana Souza"}, {"email", "ana@example.com"}, {"token", token}, {"language", kLanguage}};
}
