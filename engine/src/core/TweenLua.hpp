#pragma once

#include <array>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/PropertyTween.hpp"
#include "haylen/core/Timeline.hpp"
#include "haylen/core/Tween.hpp"
#include "haylen/core/TweenProperty.hpp"
#include "haylen/core/TweenValue.hpp"
#include "haylen/lua/NativeProperty.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "lua/Owners.hpp"

struct lua_State;

namespace haylen::core {

class ScriptedTrack;

// Installs haylen.tween: tweens of table fields and object properties, ready-made tweens, timelines and staggers, and the Tween and Timeline handles that control them. Properties of engine objects that their binding records as native tween without running Lua every frame.
class TweenLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 17> kCommonFields{"delay", "repeatCount", "loopMode", "repeatDelay", "timeScale", "tag", "owner", "processMode", "unscaled", "fixedStep", "autoKill", "paused", "onStart", "onUpdate", "onLoop", "onComplete", "onKill"};
    static constexpr std::array<std::string_view, 6> kTweenFields{"ease", "overwrite", "speedBased", "angles", "integers", "colorSpace"};
    static constexpr std::array<std::string_view, 1> kTimelineFields{"onStep"};
    static constexpr std::array<std::string_view, 1> kStaggerFields{"origin"};
    static constexpr std::array<std::string_view, 1> kFieldFields{"field"};
    static constexpr std::array<std::string_view, 2> kJumpFields{"power", "jumps"};
    static constexpr std::array<std::string_view, 4> kPathFields{"curved", "closed", "orient", "orientField"};
    static constexpr std::array<std::string_view, 3> kShakeFields{"vibrato", "randomness", "seed"};
    static constexpr std::array<std::string_view, 2> kPunchFields{"vibrato", "elasticity"};
    static constexpr std::array<std::string_view, 1> kBlinkFields{"hidden"};

    // The tween being built from Lua, the scripted track that collects its fields without a native property, and the values to add after the main one.
    struct Builder {
        lua_State* state = nullptr;
        int target = 0;
        int options = 0;
        PropertyTween* tween = nullptr;
        std::unique_ptr<ScriptedTrack> scripted;
        std::vector<std::pair<std::vector<std::string>, TweenProperty>> pending;
    };

    // A native property of a userdata target, or one component of a Vec2 or Color property.
    struct NativeField {
        void* storage = nullptr;
        const lua::NativeProperty* property = nullptr;
        int component = -1;
    };

    [[nodiscard]] static std::shared_ptr<Tween> check(lua_State* L, int index);
    [[nodiscard]] static Timeline& checkTimeline(lua_State* L, int index);
    static void pushHandle(lua_State* L, const std::shared_ptr<Tween>& tween);

    [[nodiscard]] static int readOwner(lua_State* L, int options);
    static void configure(lua_State* L, int options, int owner, Tween& tween);
    static void readCallbacks(lua_State* L, int options, int owner, Tween::Callbacks& callbacks);
    static void call(const lua::Owners::Function& function, lua_State* main);

    // Calls the function with one number, an integer for loop and step counts.
    template <typename Value> static void call(const lua::Owners::Function& function, lua_State* main, Value argument) {
        if (function.push(main)) {
            lua::Stack::push(main, argument);
            lua::Runtime::protectedCall(main, 1, 0);
        }
    }
    static void start(lua_State* L, const std::shared_ptr<Tween>& tween, int options, int owner);

    [[nodiscard]] static std::shared_ptr<PropertyTween> createTween(lua_State* L, float length, int options, int owner);
    [[nodiscard]] static bool isListed(lua_State* L, int options, const char* list, std::string_view name);
    [[nodiscard]] static TweenValue::Interpolation getInterpolation(lua_State* L, int options, std::string_view name, TweenValue::Kind kind);
    [[nodiscard]] static TweenValue readCurrent(Builder& builder, const std::vector<std::string>& names);
    [[nodiscard]] static TweenValue readLike(lua_State* L, int index, const TweenValue& current, std::string_view name);
    static void addProperty(Builder& builder, const std::vector<std::string>& names, TweenProperty property);
    static void finishTracks(Builder& builder);
    [[nodiscard]] static std::optional<std::vector<NativeField>> findNativeFields(lua_State* L, int target, const std::vector<std::string>& names);
    [[nodiscard]] static TweenValue readNative(const std::vector<NativeField>& fields);
    static void writeNative(const std::vector<NativeField>& fields, const TweenValue& value);
    static void checkTarget(lua_State* L);
    static int startValues(lua_State* L, TweenProperty::Mode mode);
    static void requireVector(lua_State* L, const TweenValue& current, const char* kind);
    [[nodiscard]] static std::function<void()> makeCall(lua_State* L, int index);

    [[nodiscard]] static std::vector<std::string> readFieldNames(lua_State* L, int options, std::vector<std::string> defaults);
    [[nodiscard]] static std::vector<math::Vec2> readPoints(lua_State* L, int index);
    static int startReadyMade(lua_State* L, std::initializer_list<lua::Table::FieldNames> extra, std::vector<std::string> defaults, const std::function<TweenProperty(Builder&, const TweenValue& current)>& make);

    static int to(lua_State* L);
    static int from(lua_State* L);
    static int by(lua_State* L);
    static int fromTo(lua_State* L);
    static int move(lua_State* L);
    static int scale(lua_State* L);
    static int rotate(lua_State* L);
    static int fade(lua_State* L);
    static int tint(lua_State* L);
    static int jump(lua_State* L);
    static int path(lua_State* L);
    static int bezier(lua_State* L);
    static int blink(lua_State* L);
    static int shake(lua_State* L);
    static int punch(lua_State* L);
    static int timeline(lua_State* L);
    static int stagger(lua_State* L);
    static int killTag(lua_State* L);
    static int completeTag(lua_State* L);
    static int pauseTag(lua_State* L);
    static int resumeTag(lua_State* L);
    static int setTimeScale(lua_State* L);
    static int getTimeScale(lua_State* L);
    static int killTarget(lua_State* L);
    static int killAll(lua_State* L);
    static int size(lua_State* L);
    static int open(lua_State* L);

    static int handlePlay(lua_State* L);
    static int handlePause(lua_State* L);
    static int handleResume(lua_State* L);
    static int handleRestart(lua_State* L);
    static int handleReverse(lua_State* L);
    static int handleSeek(lua_State* L);
    static int handleComplete(lua_State* L);
    static int handleKill(lua_State* L);
    static int handleWait(lua_State* L);
    static int handleAlive(lua_State* L);
    static int handlePlaying(lua_State* L);
    static int handlePaused(lua_State* L);
    static int handleReversed(lua_State* L);
    static int handleCompleted(lua_State* L);
    static int handleProgress(lua_State* L);
    static int handleSetProgress(lua_State* L);
    static int handleTime(lua_State* L);
    static int handleSetTime(lua_State* L);
    static int handleTimeScale(lua_State* L);
    static int handleSetTimeScale(lua_State* L);
    static int handleDuration(lua_State* L);
    static int handleTotalDuration(lua_State* L);
    static int handleDelay(lua_State* L);
    static int handleTag(lua_State* L);

    static int timelineAppend(lua_State* L);
    static int timelineJoin(lua_State* L);
    static int timelineInsert(lua_State* L);
    static int timelineAddLabel(lua_State* L);
    static int timelineSize(lua_State* L);

    template <typename T> static void installHandle(lua_State* L, bool timeline);
};

} // namespace haylen::core
