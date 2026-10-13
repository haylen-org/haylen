#include "haylen/conversation/ConversationState.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <utility>

namespace haylen::conversation {

ConversationState::ConversationState(std::shared_ptr<const Conversation> graph, const core::Json& values, std::uint64_t seed) : conversation(std::move(graph)), variables(conversation->getVariables()), visits(conversation->getNodes().size(), 0), random(seed) {
    if (!values.is_null() && !values.is_object()) {
        throw std::invalid_argument("The variables of a conversation are an object of values by name.");
    }
    if (values.is_object()) {
        variables.update(values);
    }
}

void ConversationState::start(std::string_view name) {
    const std::optional<std::size_t> found = conversation->findNode(name);
    if (!found) {
        throw std::invalid_argument(std::format("The conversation has no node \"{}\".", name));
    }
    started = true;
    finished = false;
    enter(*found);
    run();
}

void ConversationState::advance() {
    if (finished || !started) {
        throw std::logic_error("The conversation has finished. Start it again to go on.");
    }
    if (!choices.empty()) {
        throw std::logic_error("The conversation waits for a choice. Call \"choose\" with one of its choices.");
    }
    run();
}

void ConversationState::choose(std::size_t place) {
    if (choices.empty()) {
        throw std::logic_error("The conversation offers no choices now.");
    }
    if (place >= choices.size()) {
        throw std::out_of_range(std::format("The conversation offers {} choices, so there is no choice {}.", choices.size(), place + 1));
    }
    const Conversation::Choice& choice = conversation->getNode(node).choices[choices[place].index];
    if (choice.once) {
        chosen.push_back(std::format("{}:{}", conversation->getNode(node).name, choices[place].index));
    }
    choices.clear();
    line.reset();
    if (!choice.next) {
        finished = true;
        return;
    }
    enter(*choice.next);
    run();
}

void ConversationState::enter(std::size_t target) {
    node = target;
    step = 0;
    ++visits[target];
}

// Steps run until a line shows, and a node without steps left offers its choices, goes to its next node or ends the walk.
void ConversationState::run() {
    line.reset();
    for (std::size_t count = 0; count < kMaxSteps; ++count) {
        const Conversation::Node& current = conversation->getNode(node);
        if (step >= current.steps.size()) {
            offerChoices();
            if (!choices.empty()) {
                return;
            }
            if (!current.next) {
                finished = true;
                return;
            }
            enter(*current.next);
            continue;
        }

        const Conversation::Step& action = current.steps[step++];
        if (!allows(action.condition)) {
            continue;
        }
        switch (action.kind) {
        case Conversation::Step::Kind::Line: {
            const std::size_t pick = action.texts.size() == 1 ? 0 : static_cast<std::size_t>(random.range(0, static_cast<int>(action.texts.size()) - 1));
            line = Line{.speaker = action.speaker, .text = action.texts[pick], .tags = action.tags};
            return;
        }
        case Conversation::Step::Kind::Command:
            commands.push_back({.name = action.command, .arguments = action.arguments});
            break;
        case Conversation::Step::Kind::Set:
            for (const auto& [name, expression] : action.assignments) {
                variables[name] = expression.evaluate([this](std::string_view key) { return lookup(key); });
            }
            break;
        case Conversation::Step::Kind::Jump:
            enter(action.target);
            break;
        }
    }
    finished = true;
    throw std::logic_error(std::format("The conversation ran {} steps without a line in the node \"{}\", which looks like a loop of jumps.", kMaxSteps, conversation->getNode(node).name));
}

// The line before the choices stays, so a question shows with its answers.
void ConversationState::offerChoices() {
    const Conversation::Node& current = conversation->getNode(node);
    for (std::size_t index = 0; index < current.choices.size(); ++index) {
        const Conversation::Choice& choice = current.choices[index];
        const bool spent = choice.once && std::ranges::find(chosen, std::format("{}:{}", current.name, index)) != chosen.end();
        if (!spent && allows(choice.condition)) {
            choices.push_back({.index = index, .text = choice.text});
        }
    }
}

core::Json ConversationState::lookup(std::string_view name) const {
    if (name.starts_with("visits.")) {
        return getVisits(name.substr(7));
    }
    const auto found = variables.find(name);
    return found == variables.end() ? core::Json() : *found;
}

bool ConversationState::allows(const std::optional<Expression>& condition) const {
    return !condition || condition->test([this](std::string_view key) { return lookup(key); });
}

void ConversationState::setVariable(const std::string& name, core::Json value) {
    variables[name] = std::move(value);
}

std::int64_t ConversationState::getVisits(std::string_view name) const {
    const std::optional<std::size_t> found = conversation->findNode(name);
    return found ? visits[*found] : 0;
}

std::string_view ConversationState::getNodeName() const noexcept {
    return conversation->getNode(node).name;
}

void ConversationState::deliverCommands() {
    if (commanded.empty() || commands.empty()) {
        return;
    }
    for (const Command& command : std::exchange(commands, {})) {
        commanded.emit(command.name, command.arguments);
    }
}

core::Json ConversationState::save() const {
    core::Json visited = core::Json::object();
    for (std::size_t index = 0; index < visits.size(); ++index) {
        if (visits[index] > 0) {
            visited[conversation->getNode(index).name] = visits[index];
        }
    }
    core::Json shown = nullptr;
    if (line) {
        shown = {{"speaker", line->speaker}, {"text", line->text}, {"tags", line->tags}};
    }
    core::Json offered = core::Json::array();
    for (const Choice& choice : choices) {
        offered.push_back(choice.index);
    }
    const std::array<std::uint64_t, 4>& state = random.getState();
    return {{"node", conversation->getNode(node).name}, {"step", step}, {"started", started}, {"finished", finished}, {"variables", variables}, {"visits", std::move(visited)}, {"chosen", chosen}, {"line", std::move(shown)}, {"choices", std::move(offered)}, {"random", std::vector<std::string>{std::to_string(state[0]), std::to_string(state[1]), std::to_string(state[2]), std::to_string(state[3])}}};
}

std::shared_ptr<ConversationState> ConversationState::restore(std::shared_ptr<const Conversation> graph, const core::Json& saved) {
    try {
        auto restored = std::make_shared<ConversationState>(std::move(graph), core::Json(), 0);
        ConversationState& state = *restored;
        const std::optional<std::size_t> found = state.conversation->findNode(saved.at("node").get<std::string>());
        if (!found) {
            throw std::invalid_argument(std::format("the conversation has no node \"{}\"", saved.at("node").get<std::string>()));
        }
        state.node = *found;
        state.step = std::min(saved.at("step").get<std::size_t>(), state.conversation->getNode(state.node).steps.size());
        state.started = saved.at("started").get<bool>();
        state.finished = saved.at("finished").get<bool>();
        state.variables = saved.at("variables");
        for (const auto& [name, count] : saved.at("visits").items()) {
            if (const std::optional<std::size_t> visited = state.conversation->findNode(name)) {
                state.visits[*visited] = count.get<std::int64_t>();
            }
        }
        state.chosen = readList(saved, "chosen").get<std::vector<std::string>>();
        if (saved.contains("line") && saved.at("line").is_object()) {
            const core::Json& shown = saved.at("line");
            state.line = Line{.speaker = shown.at("speaker").get<std::string>(), .text = shown.at("text").get<std::string>(), .tags = readList(shown, "tags").get<std::vector<std::string>>()};
        }
        const std::vector<Conversation::Choice>& offered = state.conversation->getNode(state.node).choices;
        for (const core::Json& index : readList(saved, "choices")) {
            const std::size_t place = index.get<std::size_t>();
            if (place >= offered.size()) {
                throw std::invalid_argument(std::format("the node \"{}\" has no choice {}", saved.at("node").get<std::string>(), place + 1));
            }
            state.choices.push_back({.index = place, .text = offered[place].text});
        }
        const std::vector<std::string> numbers = saved.at("random").get<std::vector<std::string>>();
        if (numbers.size() != 4) {
            throw std::invalid_argument("the state of its generator has the wrong size");
        }
        state.random.setState({std::stoull(numbers[0]), std::stoull(numbers[1]), std::stoull(numbers[2]), std::stoull(numbers[3])});
        if (!state.variables.is_object()) {
            throw std::invalid_argument("its variables are not an object");
        }
        return restored;
    } catch (const std::exception& error) {
        throw std::invalid_argument(std::format("The saved conversation does not fit this conversation: {}.", error.what()));
    }
}

// A save that went through Lua holds its empty lists as empty tables, which come back as empty objects, and leaves out what was nil.
core::Json ConversationState::readList(const core::Json& saved, const char* key) {
    if (!saved.contains(key) || (saved.at(key).is_object() && saved.at(key).empty())) {
        return core::Json::array();
    }
    return saved.at(key);
}

} // namespace haylen::conversation
