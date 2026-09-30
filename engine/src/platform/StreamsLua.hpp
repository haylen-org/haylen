#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>

#include "haylen/lua/Type.hpp"
#include "haylen/platform/AudioStream.hpp"
#include "haylen/platform/VideoStream.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<platform::VideoStream> {
    static constexpr const char* name = "haylen.VideoStream";
    using Storage = std::shared_ptr<platform::VideoStream>;
};

template <> struct Type<platform::AudioStream> {
    static constexpr const char* name = "haylen.AudioStream";
    using Storage = std::shared_ptr<platform::AudioStream>;
};

} // namespace haylen::lua

namespace haylen::platform {

// Installs the `VideoStream` and `AudioStream` classes that the handles of plugins return for the streams their native parts open. A video stream keeps its texture current and announces every frame, and an audio stream plays as a voice of `haylen.audio` and hands out its newest samples.
class StreamsLua final {
  public:
    static void install(lua_State* L);

    // Pushes the stream `name` of the plugin, or `nil` while its native part has not opened it. The engine keeps the texture of a video stream current from then on, until the app stops.
    static void pushVideoStream(lua_State* L, const std::string& plugin, const std::string& name);
    static void pushAudioStream(lua_State* L, const std::string& plugin, const std::string& name);

  private:
    // Streams play as they arrive, so their voices take no loop, start time or pitch.
    static constexpr std::array<std::string_view, 8> kPlayFields{"bus", "volume", "pan", "fadeIn", "x", "y", "processMode", "effects"};
    static constexpr std::array<std::string_view, 1> kListenerOptions{"owner"};

    static int getTexture(lua_State* L);
    static int getWidth(lua_State* L);
    static int getHeight(lua_State* L);
    static int getFrameCount(lua_State* L);
    static int getTimestamp(lua_State* L);
    static int on(lua_State* L);

    static int play(lua_State* L);
    static int read(lua_State* L);
    static int getSampleRate(lua_State* L);
    static int getChannels(lua_State* L);
    static int getUnderruns(lua_State* L);
};

} // namespace haylen::platform
