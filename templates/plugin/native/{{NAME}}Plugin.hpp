#pragma once

#include <cstddef>
#include <cstdint>

#include <nlohmann/json.hpp>

#include "haylen/platform/native/HaylenNative.h"

// The desktop part of the {{TITLE}} plugin, a C++ library for macOS, Windows and Linux. The Lua API loads it with `native.load`, which hands its init function the `HaylenNativeApi` of the engine, through which the library answers the methods of the plugin and sends its events. Its handlers run on the frame thread, and the library may answer and send events from any thread.
class {{NAME}}Plugin final {
  public:
    // Creates the plugin, which lives as long as the process, since the engine may call its handlers until the process ends, registers its methods and declares the library the native part of `{{ID}}`. Returns 1 when the engine hands a native API of another version.
    [[nodiscard]] static int initialize(const HaylenNativeApi* api);

  private:
    explicit {{NAME}}Plugin(const HaylenNativeApi& engine);

    static void handleEcho(void* user, std::uint64_t call, const char* method, const char* paramsJson, const HaylenNativeBuffer* buffers, std::size_t bufferCount);

    // Answers `{{ID}}.echo` with the message it receives, and sends it to the app again as the `{{ID}}.echoed` event.
    void echo(std::uint64_t call, const nlohmann::json& params) const;

    const HaylenNativeApi& api;
};
