#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

#include "haylen/audio/Tap.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<haylen::audio::Tap> {
    static constexpr const char* name = "haylen.AudioTap";
    using Storage = std::shared_ptr<haylen::audio::Tap>;
};

} // namespace haylen::lua

namespace haylen::lua::audio {

// Installs `audio.tapMaster` and the `AudioTap` class, which hands the app the mix of the master bus in the batches that arrived since its last read.
class TapBinding final {
  public:
    static constexpr float kDefaultSeconds = 1.0F;

    // The most seconds of mix that a tap of Lua holds between two reads.
    static constexpr float kMaxSeconds = 60.0F;

    static void install(lua_State* L);

    // Opens a tap with `tapMaster({seconds, owner})`, which holds `seconds` of mix between two reads and closes when its owner ends.
    static int tapMaster(lua_State* L);

  private:
    // Closes the tap when its owner ends.
    class OwnerLink final : public haylen::core::Connection::Link {
      public:
        explicit OwnerLink(std::weak_ptr<haylen::audio::Tap> value) noexcept : tap(std::move(value)) {}

        void disconnect() override;
        [[nodiscard]] bool isConnected() const noexcept override;
        void setBlocked(bool value) override {
            blocked = value;
        }
        [[nodiscard]] bool isBlocked() const noexcept override {
            return blocked;
        }

      private:
        std::weak_ptr<haylen::audio::Tap> tap;
        bool blocked = false;
    };

    static constexpr std::array<std::string_view, 2> kTapFields{"seconds", "owner"};

    // The floats that a read into a string copies at a time.
    static constexpr std::size_t kChunk = 1024;

    static int read(lua_State* L);
    static int close(lua_State* L);
    static int getAvailable(lua_State* L);
    static int getDropped(lua_State* L);
    static int getSampleRate(lua_State* L);
    static int getChannels(lua_State* L);
    static int isClosed(lua_State* L);
};

} // namespace haylen::lua::audio
