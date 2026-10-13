#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/conversation/Conversation.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::conversation {

// One walk through a conversation: the node and step it stands at, the line it shows or the choices it offers, the variables, how often each node was entered and the generator that picks variants. It runs steps until it reaches a line, choices or the end, and keeps the commands it ran until `deliverCommands` hands them to the app. Its state saves to JSON and restores into the same conversation, so a saved game resumes a conversation where it stood.
class ConversationState final {
  public:
    struct Line {
        std::string speaker;
        std::string text;
        std::vector<std::string> tags;
    };

    // A choice that its condition allows, with its place among the choices of the node.
    struct Choice {
        std::size_t index = 0;
        std::string text;
    };

    struct Command {
        std::string name;
        core::Json arguments;
    };

    core::Signal<std::string_view, const core::Json&> commanded;

    // The variables start from the defaults of the conversation, with the given values over them.
    ConversationState(std::shared_ptr<const Conversation> graph, const core::Json& values, std::uint64_t seed);

    ConversationState(const ConversationState&) = delete;
    ConversationState& operator=(const ConversationState&) = delete;

    // Enters the node and runs to its first line, its choices or the end. Throws `std::invalid_argument` for a node the conversation lacks.
    void start(std::string_view node);

    // Goes on from the line it shows. Throws `std::logic_error` while it offers choices or once it finished.
    void advance();

    // Takes the choice at a place of `getChoices`, from 0. Throws `std::logic_error` without choices and `std::out_of_range` for a place outside them.
    void choose(std::size_t place);

    [[nodiscard]] const std::optional<Line>& getLine() const noexcept {
        return line;
    }
    [[nodiscard]] const std::vector<Choice>& getChoices() const noexcept {
        return choices;
    }
    [[nodiscard]] bool isFinished() const noexcept {
        return finished;
    }
    [[nodiscard]] const core::Json& getVariables() const noexcept {
        return variables;
    }
    void setVariable(const std::string& name, core::Json value);
    [[nodiscard]] std::int64_t getVisits(std::string_view node) const;
    [[nodiscard]] std::string_view getNodeName() const noexcept;
    [[nodiscard]] const std::shared_ptr<const Conversation>& getConversation() const noexcept {
        return conversation;
    }

    // Emits the commands that ran on `commanded`, in order, once something listens, so the commands before the first listener wait for it. The state never emits while it runs steps, so a listener may advance it.
    void deliverCommands();

    [[nodiscard]] core::Json save() const;

    // Rebuilds a state from `save`, and throws `std::invalid_argument` when the save does not fit the conversation.
    [[nodiscard]] static std::shared_ptr<ConversationState> restore(std::shared_ptr<const Conversation> graph, const core::Json& saved);

  private:
    // Steps that run in a row without a line, such as jumps in a loop, end the walk with an error past this count.
    static constexpr std::size_t kMaxSteps = 10000;

    void enter(std::size_t node);
    void run();
    void offerChoices();
    [[nodiscard]] core::Json lookup(std::string_view name) const;
    [[nodiscard]] bool allows(const std::optional<Expression>& condition) const;
    [[nodiscard]] static core::Json readList(const core::Json& saved, const char* key);

    std::shared_ptr<const Conversation> conversation;
    core::Json variables;
    std::vector<std::int64_t> visits;
    std::vector<std::string> chosen;
    std::vector<Command> commands;
    std::optional<Line> line;
    std::vector<Choice> choices;
    math::Random random;
    std::size_t node = 0;
    std::size_t step = 0;
    bool started = false;
    bool finished = false;
};

} // namespace haylen::conversation
