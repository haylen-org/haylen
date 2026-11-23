#include "modules/CoreModule.hpp"

#include "ai/AiBinding.hpp"
#include "core/CollectionsBinding.hpp"
#include "core/CoreBinding.hpp"
#include "core/EventsBinding.hpp"
#include "core/SceneBinding.hpp"
#include "core/SignalBinding.hpp"
#include "core/TimerBinding.hpp"
#include "core/TweenBinding.hpp"
#include "math/MathBinding.hpp"
#include "platform/DisplayBinding.hpp"
#include "platform/WindowBinding.hpp"

namespace haylen::lua::modules {

void CoreModule::install(haylen::core::Engine&, lua_State* L) {
    lua::core::CoreBinding::install(L);
    lua::platform::WindowBinding::install(L);
    lua::platform::DisplayBinding::install(L);
    lua::core::CollectionsBinding::install(L);
    lua::core::TimerBinding::install(L);
    lua::core::SceneBinding::install(L);
    lua::core::SignalBinding::install(L);
    lua::core::EventsBinding::install(L);
    lua::math::MathBinding::install(L);
    lua::core::TweenBinding::install(L);
    lua::ai::AiBinding::install(L);
}

} // namespace haylen::lua::modules
