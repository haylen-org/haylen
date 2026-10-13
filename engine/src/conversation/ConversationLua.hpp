#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "haylen/conversation/Conversation.hpp"
#include "haylen/conversation/ConversationState.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<conversation::Conversation> {
    static constexpr const char* name = "haylen.Conversation";
    using Storage = std::shared_ptr<conversation::Conversation>;
};

template <> struct Type<conversation::ConversationState> {
    static constexpr const char* name = "haylen.ConversationState";
    using Storage = std::shared_ptr<conversation::ConversationState>;
};

} // namespace haylen::lua

namespace haylen::conversation {

// Installs `haylen.conversation` with the `Conversation` and `ConversationState` classes. Lines and choices resolve their text through the localization catalog, with the variables as the arguments of the text, and keep the key they came from.
class ConversationLua final {
  public:
    static void install(lua_State* L);
    static void pushConversation(lua_State* L, std::shared_ptr<Conversation> conversation);

  private:
    static constexpr std::array<std::string_view, 2> kStartFields{"variables", "seed"};
    static constexpr std::array<std::string_view, 1> kListenerFields{"owner"};

    static void pushState(lua_State* L, std::shared_ptr<ConversationState> state);
    static void pushText(lua_State* L, const std::string& key, const core::Json& variables);
    static void deliver(ConversationState& state);

    static int open(lua_State* L);
    static int create(lua_State* L);
    static int start(lua_State* L);
    static int restore(lua_State* L);
    static int texts(lua_State* L);
    static int nodes(lua_State* L);
    static int line(lua_State* L);
    static int choices(lua_State* L);
    static int finished(lua_State* L);
    static int variables(lua_State* L);
    static int node(lua_State* L);
    static int advance(lua_State* L);
    static int choose(lua_State* L);
    static int setVariable(lua_State* L);
    static int visits(lua_State* L);
    static int save(lua_State* L);
    static int on(lua_State* L);
};

} // namespace haylen::conversation
