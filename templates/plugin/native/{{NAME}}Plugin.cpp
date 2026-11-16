#include "{{NAME}}Plugin.hpp"

#include <string>

int {{NAME}}Plugin::initialize(const HaylenNativeApi* api) {
    if (api->version != HAYLEN_NATIVE_API_VERSION) {
        return 1;
    }
    auto* plugin = new {{NAME}}Plugin(*api);
    api->registerHandler("{{ID}}.echo", &{{NAME}}Plugin::handleEcho, nullptr, plugin);
    api->registerPlugin("{{ID}}");
    return 0;
}

{{NAME}}Plugin::{{NAME}}Plugin(const HaylenNativeApi& engine) : api(engine) {}

// The engine always hands JSON text, which the handler reads without exceptions, since none may cross the C interface of the engine.
void {{NAME}}Plugin::handleEcho(void* user, std::uint64_t call, const char*, const char* paramsJson, const HaylenNativeBuffer*, std::size_t) {
    const auto* plugin = static_cast<const {{NAME}}Plugin*>(user);
    const nlohmann::json params = nlohmann::json::parse(paramsJson, nullptr, false);
    plugin->echo(call, params);
}

void {{NAME}}Plugin::echo(std::uint64_t call, const nlohmann::json& params) const {
    const auto message = params.find("message");
    if (!params.is_object() || message == params.end() || !message->is_string()) {
        const nlohmann::json failure = {{"message", "The method \"{{ID}}.echo\" needs a \"message\" text."}, {"code", "invalidParams"}};
        api.resolve(call, 0, failure.dump().c_str(), nullptr, 0);
        return;
    }

    const nlohmann::json echoed = {{"message", *message}};
    const std::string text = echoed.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    api.emit("{{ID}}.echoed", text.c_str(), nullptr, 0, 0);
    api.resolve(call, 1, text.c_str(), nullptr, 0);
}
