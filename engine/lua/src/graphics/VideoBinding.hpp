#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "haylen/core/Connection.hpp"
#include "haylen/graphics/Video.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<haylen::graphics::Video> {
    static constexpr const char* name = "haylen.Video";
    using Storage = std::shared_ptr<haylen::graphics::Video>;
};

} // namespace haylen::lua

namespace haylen::lua::graphics {

// Installs `haylen.video`, which plays video files of the package into textures, and the `Video` class.
class VideoBinding final {
  public:
    static void install(lua_State* L);

  private:
    // Releases the video when its owner ends.
    class Release final : public haylen::core::Connection::Link {
      public:
        Release(lua_State* state, std::weak_ptr<haylen::graphics::Video> target) : main(state), video(std::move(target)) {}
        void disconnect() override;
        [[nodiscard]] bool isConnected() const noexcept override {
            return !video.expired();
        }
        void setBlocked(bool) override {}
        [[nodiscard]] bool isBlocked() const noexcept override {
            return false;
        }

      private:
        lua_State* main;
        std::weak_ptr<haylen::graphics::Video> video;
    };

    static constexpr std::array<std::string_view, 8> kLoadOptions{"bus", "volume", "muted", "loop", "rate", "autoplay", "processMode", "owner"};
    static constexpr std::array<std::string_view, 1> kListenerOptions{"owner"};
    static constexpr std::array<std::string_view, 3> kStateNames{"loading", "ready", "failed"};

    [[nodiscard]] static std::shared_ptr<haylen::graphics::Video> create(lua_State* L);

    static int load(lua_State* L);
    static int loadAsync(lua_State* L);
    static int play(lua_State* L);
    static int pause(lua_State* L);
    static int seek(lua_State* L);
    static int release(lua_State* L);
    static int on(lua_State* L);
    static int getTexture(lua_State* L);
    static int getWidth(lua_State* L);
    static int getHeight(lua_State* L);
    static int getDuration(lua_State* L);
    static int getTime(lua_State* L);
    static int setTime(lua_State* L);
    static int isPlaying(lua_State* L);
    static int isEnded(lua_State* L);
    static int getState(lua_State* L);
    static int getError(lua_State* L);
    static int getFrameCount(lua_State* L);
    static int isLooping(lua_State* L);
    static int setLoop(lua_State* L);
    static int getRate(lua_State* L);
    static int setRate(lua_State* L);
    static int getVolume(lua_State* L);
    static int setVolume(lua_State* L);
    static int isMuted(lua_State* L);
    static int setMuted(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::lua::graphics
