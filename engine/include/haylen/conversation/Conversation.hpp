#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/conversation/Expression.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::conversation {

// The graph of a branching conversation, read once from JSON and never changed: named nodes, each with steps that show lines, run commands of the app, set variables or jump to other nodes, followed by choices or the next node. Every step and choice may have a condition. A line names its text by a key of the localization catalog, or holds the text itself, and a list of texts makes a variant that a seeded generator picks. `ConversationState` walks the graph.
class Conversation final {
  public:
    struct Step {
        enum class Kind : std::uint8_t {
            Line,
            Command,
            Set,
            Jump,
        };

        Kind kind = Kind::Line;
        std::optional<Expression> condition;
        std::string speaker;
        std::vector<std::string> texts;
        std::vector<std::string> tags;
        std::string command;
        core::Json arguments;
        std::vector<std::pair<std::string, Expression>> assignments;
        std::size_t target = 0;
    };

    struct Choice {
        std::string text;
        std::optional<Expression> condition;
        std::optional<std::size_t> next;
        bool once = false;
    };

    struct Node {
        std::string name;
        std::vector<Step> steps;
        std::vector<Choice> choices;
        std::optional<std::size_t> next;
    };

    // Reads a document `{"variables": {...}, "nodes": {...}}`, and throws `std::invalid_argument` that names the node and the field of a mistake.
    [[nodiscard]] static Conversation parse(const core::Json& document);

    [[nodiscard]] std::optional<std::size_t> findNode(std::string_view name) const;
    [[nodiscard]] const Node& getNode(std::size_t index) const noexcept {
        return nodes[index];
    }
    [[nodiscard]] const std::vector<Node>& getNodes() const noexcept {
        return nodes;
    }

    // The values that variables have before a conversation sets them.
    [[nodiscard]] const core::Json& getVariables() const noexcept {
        return variables;
    }

    // Every text of the lines and choices once, in order, which translators turn into the keys of a catalog.
    [[nodiscard]] std::vector<std::string> getTexts() const;

  private:
    static constexpr std::string_view kLineKeys[] = {"if", "speaker", "text", "tags"};
    static constexpr std::string_view kCommandKeys[] = {"if", "command", "arguments"};
    static constexpr std::string_view kSetKeys[] = {"if", "set"};
    static constexpr std::string_view kJumpKeys[] = {"if", "jump"};
    static constexpr std::string_view kChoiceKeys[] = {"if", "text", "next", "once"};
    static constexpr std::string_view kNodeKeys[] = {"steps", "choices", "next"};

    [[nodiscard]] Step readStep(const core::Json& value, const std::string& node, std::size_t index) const;
    [[nodiscard]] Choice readChoice(const core::Json& value, const std::string& node, std::size_t index) const;
    [[nodiscard]] std::size_t readTarget(const core::Json& value, const std::string& where) const;
    [[nodiscard]] static std::optional<Expression> readCondition(const core::Json& value, const std::string& where);
    static void requireKeys(const core::Json& value, std::span<const std::string_view> keys, const std::string& where);

    std::vector<Node> nodes;
    std::unordered_map<std::string, std::size_t> indices;
    core::Json variables = core::Json::object();
};

} // namespace haylen::conversation
