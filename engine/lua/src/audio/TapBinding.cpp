#include "audio/TapBinding.hpp"

#include <lua.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <span>
#include <stdexcept>

#include "core/FloatBufferBinding.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FloatBuffer.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"
#include "runtime/Owners.hpp"

namespace haylen::lua::audio {

void TapBinding::OwnerLink::disconnect() {
    if (const std::shared_ptr<haylen::audio::Tap> locked = tap.lock()) {
        locked->close();
    }

    tap.reset();
}

bool TapBinding::OwnerLink::isConnected() const noexcept {
    const std::shared_ptr<haylen::audio::Tap> locked = tap.lock();
    return locked && locked->isOpen();
}

int TapBinding::tapMaster(lua_State* L) {
    float seconds = kDefaultSeconds;
    int owner = 0;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kTapFields});
        lua::Table::readField(L, 1, "seconds", seconds);
        if (lua_getfield(L, 1, "owner") != LUA_TNIL) {
            lua::Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        }
    }

    if (!(seconds > 0.0F && seconds <= kMaxSeconds)) {
        throw std::invalid_argument("A tap of the audio holds more than 0 and at most 60 seconds of mix between two reads.");
    }

    if (owner == 0) {
        owner = lua::Owners::pushDefault(L);
    }

    haylen::audio::Mixer& mixer = lua::Runtime::getEngine(L).getAudio();
    const auto frames = static_cast<std::size_t>(std::ceil(static_cast<double>(seconds) * static_cast<double>(mixer.getSampleRate())));
    std::shared_ptr<haylen::audio::Tap> tap = mixer.tapMaster(frames);
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<OwnerLink>(tap));
    }

    lua::Userdata::emplace<haylen::audio::Tap>(L, std::move(tap));
    return 1;
}

// Moves the frames that arrived into a float buffer of `haylen.collections` with `read(buffer)`, interleaved from the first value, as many as fit, and returns how many values it wrote, or returns them as a string of float samples, the raw samples that `audio.newSound` takes, with `read()`.
int TapBinding::read(lua_State* L) {
    haylen::audio::Tap& tap = lua::Userdata::check<haylen::audio::Tap>(L, 1);
    if (!lua_isnoneornil(L, 2)) {
        const std::size_t frames = tap.read(lua::Userdata::check<haylen::core::FloatBuffer>(L, 2).getValues());
        lua::Stack::push(L, frames * tap.getChannels());
        return 1;
    }

    // The frames that wait now are the frames of this read, and the ones that arrive meanwhile wait for the next one.
    const std::size_t channels = tap.getChannels();
    const std::size_t wanted = tap.getAvailableFrames();
    luaL_Buffer buffer;
    char* bytes = luaL_buffinitsize(L, &buffer, wanted * channels * sizeof(float));
    std::array<float, kChunk> chunk{};
    const std::size_t chunkFrames = kChunk / channels;
    std::size_t done = 0;
    while (done < wanted) {
        const std::size_t moved = tap.read(std::span<float>(chunk.data(), std::min(chunkFrames, wanted - done) * channels));
        std::memcpy(bytes + done * channels * sizeof(float), chunk.data(), moved * channels * sizeof(float));
        done += moved;
    }

    luaL_pushresultsize(&buffer, done * channels * sizeof(float));
    return 1;
}

int TapBinding::close(lua_State* L) {
    lua::Userdata::check<haylen::audio::Tap>(L, 1).close();
    return 0;
}

int TapBinding::getAvailable(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::audio::Tap>(L, 1).getAvailableFrames());
    return 1;
}

int TapBinding::getDropped(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::audio::Tap>(L, 1).getDroppedFrames());
    return 1;
}

int TapBinding::getSampleRate(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::audio::Tap>(L, 1).getSampleRate());
    return 1;
}

int TapBinding::getChannels(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::audio::Tap>(L, 1).getChannels());
    return 1;
}

int TapBinding::isClosed(lua_State* L) {
    lua::Stack::push(L, !lua::Userdata::check<haylen::audio::Tap>(L, 1).isOpen());
    return 1;
}

void TapBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::audio::Tap>(L).function("read", &lua::Binding::native<&read>).function("close", &lua::Binding::native<&close>).property("available", &getAvailable).property("dropped", &getDropped).property("sampleRate", &getSampleRate).property("channels", &getChannels).property("closed", &isClosed).meta("__eq", &lua::Userdata::equal<haylen::audio::Tap>).install();
}

} // namespace haylen::lua::audio
