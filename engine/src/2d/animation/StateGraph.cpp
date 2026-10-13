#include "haylen/2d/animation/StateGraph.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <numbers>
#include <stdexcept>

namespace haylen::animation2d {

const std::array<std::pair<std::string_view, StateGraph::Kind>, 4> StateGraph::kKindNames{{{"number", Kind::Number}, {"boolean", Kind::Boolean}, {"trigger", Kind::Trigger}, {"vector", Kind::Vector}}};
const std::array<std::pair<std::string_view, StateGraph::Direction>, 8> StateGraph::kDirectionNames{{
    {"right", Direction::Right},
    {"downRight", Direction::DownRight},
    {"down", Direction::Down},
    {"downLeft", Direction::DownLeft},
    {"left", Direction::Left},
    {"upLeft", Direction::UpLeft},
    {"up", Direction::Up},
    {"upRight", Direction::UpRight},
}};

std::optional<StateGraph::Kind> StateGraph::kindFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kKindNames, name, &std::pair<std::string_view, Kind>::first);
    return found != kKindNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view StateGraph::kindName(Kind value) noexcept {
    return std::ranges::find(kKindNames, value, &std::pair<std::string_view, Kind>::second)->first;
}

std::optional<StateGraph::Direction> StateGraph::directionFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kDirectionNames, name, &std::pair<std::string_view, Direction>::first);
    return found != kDirectionNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view StateGraph::directionName(Direction value) noexcept {
    return std::ranges::find(kDirectionNames, value, &std::pair<std::string_view, Direction>::second)->first;
}

// Parses `expression := disjunction`, `disjunction := conjunction ("or" conjunction)*`, `conjunction := negation ("and" negation)*`, `negation := "not" negation | comparison` and `comparison := operand (operator operand)?`, where an operand is a number, `true`, `false`, a parameter, `name.x`, `name.y` or an expression in parentheses.
class StateGraph::Parser final {
  public:
    Parser(StateGraph& owner, std::string_view source, std::string_view where) : graph(owner), text(source), context(where) {}

    std::size_t parse() {
        const std::size_t root = disjunction();
        skipSpaces();
        if (position < text.size()) {
            fail("has an unexpected \"" + std::string(text.substr(position)) + "\"");
        }
        return root;
    }

    std::vector<std::size_t> triggers;

  private:
    [[noreturn]] void fail(const std::string& problem) const {
        throw std::invalid_argument("The condition \"" + std::string(text) + "\" of " + std::string(context) + " " + problem + ".");
    }

    void skipSpaces() noexcept {
        while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])) != 0) {
            ++position;
        }
    }

    [[nodiscard]] std::string_view peekWord() {
        skipSpaces();
        std::size_t end = position;
        while (end < text.size() && (std::isalnum(static_cast<unsigned char>(text[end])) != 0 || text[end] == '_' || text[end] == '.')) {
            ++end;
        }
        return text.substr(position, end - position);
    }

    bool acceptWord(std::string_view word) {
        if (peekWord() != word) {
            return false;
        }
        position += word.size();
        return true;
    }

    std::size_t add(Term term) {
        graph.terms.push_back(term);
        return graph.terms.size() - 1;
    }

    std::size_t disjunction() {
        std::size_t left = conjunction();
        while (acceptWord("or")) {
            left = add({.op = Term::Op::Or, .left = left, .right = conjunction()});
        }
        return left;
    }

    std::size_t conjunction() {
        std::size_t left = negation();
        while (acceptWord("and")) {
            left = add({.op = Term::Op::And, .left = left, .right = negation()});
        }
        return left;
    }

    std::size_t negation() {
        if (acceptWord("not")) {
            return add({.op = Term::Op::Not, .left = negation()});
        }
        return comparison();
    }

    std::size_t comparison() {
        const std::size_t left = operand();
        skipSpaces();
        static constexpr std::array<std::pair<std::string_view, Term::Op>, 7> kOperators{{{"==", Term::Op::Equal}, {"~=", Term::Op::NotEqual}, {"!=", Term::Op::NotEqual}, {"<=", Term::Op::LessEqual}, {">=", Term::Op::GreaterEqual}, {"<", Term::Op::Less}, {">", Term::Op::Greater}}};
        for (const auto& [symbol, op] : kOperators) {
            if (text.substr(position, symbol.size()) == symbol) {
                position += symbol.size();
                return add({.op = op, .left = left, .right = operand()});
            }
        }
        return left;
    }

    std::size_t operand() {
        skipSpaces();
        if (position < text.size() && text[position] == '(') {
            ++position;
            const std::size_t inner = disjunction();
            skipSpaces();
            if (position >= text.size() || text[position] != ')') {
                fail("misses a closing parenthesis");
            }
            ++position;
            return inner;
        }

        const std::string_view word = peekWord();
        if (word.empty()) {
            fail(position < text.size() ? "has an unexpected \"" + std::string(text.substr(position, 1)) + "\"" : "ends too early");
        }
        position += word.size();
        if (word == "true" || word == "false") {
            return add({.op = Term::Op::Constant, .constant = word == "true" ? 1.0F : 0.0F});
        }
        if (std::isdigit(static_cast<unsigned char>(word.front())) != 0 || word.front() == '.') {
            const std::string digits(word);
            char* end = nullptr;
            const float number = std::strtof(digits.c_str(), &end);
            if (end != digits.c_str() + digits.size() || !std::isfinite(number)) {
                fail("has a number it cannot read, \"" + digits + "\"");
            }
            return add({.op = Term::Op::Constant, .constant = number});
        }

        // A vector takes its component after a dot, and a trigger the condition reads is consumed when its transition fires.
        const std::size_t dot = word.find('.');
        const std::string_view name = word.substr(0, dot);
        const Parameter* parameter = graph.findParameter(name);
        if (parameter == nullptr) {
            fail("names the unknown parameter \"" + std::string(name) + "\"");
        }
        std::size_t slot = parameter->slot;
        if (parameter->kind == Kind::Vector) {
            const std::string_view component = dot == std::string_view::npos ? std::string_view{} : word.substr(dot + 1);
            if (component != "x" && component != "y") {
                fail("reads the vector \"" + std::string(name) + "\" without \".x\" or \".y\"");
            }
            slot += component == "y" ? 1U : 0U;
        } else if (dot != std::string_view::npos) {
            fail("reads a component of \"" + std::string(name) + "\", which is no vector");
        }
        if (parameter->kind == Kind::Trigger && std::ranges::find(triggers, parameter->slot) == triggers.end()) {
            triggers.push_back(parameter->slot);
        }
        return add({.op = Term::Op::Value, .slot = slot});
    }

    StateGraph& graph;
    std::string_view text;
    std::string_view context;
    std::size_t position = 0;
};

StateGraph StateGraph::parse(const core::Json& description) {
    if (!description.is_object()) {
        throw std::invalid_argument("A state graph description must be an object with \"parameters\", \"states\" and \"transitions\".");
    }
    for (const auto& [key, value] : description.items()) {
        if (key != "parameters" && key != "states" && key != "transitions" && key != "initial") {
            throw std::invalid_argument("Unknown option \"" + key + "\".");
        }
    }

    StateGraph graph;
    // Parameters and states keep the order of their names, so slots and indices never depend on the order of a table.
    const std::map<std::string, core::Json> parameterList = description.value("parameters", core::Json::object()).get<std::map<std::string, core::Json>>();
    for (const auto& [name, value] : parameterList) {
        Parameter parameter{.name = name, .slot = graph.valueCount};
        if (value.is_number()) {
            parameter.value[0] = value.get<float>();
        } else if (value.is_boolean()) {
            parameter.kind = Kind::Boolean;
            parameter.value[0] = value.get<bool>() ? 1.0F : 0.0F;
        } else if (value.is_string() && value.get<std::string>() == "trigger") {
            parameter.kind = Kind::Trigger;
        } else if (value.is_array() && value.size() == 2 && value[0].is_number() && value[1].is_number()) {
            parameter.kind = Kind::Vector;
            parameter.value = {value[0].get<float>(), value[1].get<float>()};
        } else {
            throw std::invalid_argument("The parameter \"" + name + "\" must start as a number, a boolean, \"trigger\" or a vector of two numbers.");
        }
        graph.valueCount += parameter.kind == Kind::Vector ? 2 : 1;
        graph.parameters.push_back(std::move(parameter));
    }

    const core::Json stateList = description.value("states", core::Json::object());
    if (!stateList.is_object() || stateList.empty()) {
        throw std::invalid_argument("A state graph needs at least one state.");
    }
    for (const auto& [name, value] : stateList.get<std::map<std::string, core::Json>>()) {
        State state{.name = name};
        if (!value.is_object()) {
            throw std::invalid_argument("The state \"" + name + "\" must be an object with an \"animation\" or \"directions\".");
        }
        for (const auto& [key, field] : value.items()) {
            if (key != "animation" && key != "directions" && key != "by") {
                throw std::invalid_argument("The state \"" + name + "\" has the unknown key \"" + key + "\".");
            }
        }
        if (value.contains("animation") == value.contains("directions")) {
            throw std::invalid_argument("The state \"" + name + "\" plays an \"animation\" or a set of \"directions\", one of the two.");
        }
        if (value.contains("animation")) {
            state.animation = value.at("animation").get<std::string>();
        } else {
            const core::Json& set = value.at("directions");
            const Parameter* by = graph.findParameter(value.value("by", std::string()));
            if (by == nullptr || by->kind != Kind::Vector) {
                throw std::invalid_argument("The directions of the state \"" + name + "\" need \"by\", the name of a vector parameter.");
            }
            state.by = by->slot;
            for (const auto& [direction, animation] : set.items()) {
                const std::optional<Direction> found = directionFromName(direction);
                if (!found || !animation.is_string()) {
                    throw std::invalid_argument("The state \"" + name + "\" names the direction \"" + direction + "\". Directions are \"right\", \"down\", \"left\" and \"up\", with \"downRight\", \"downLeft\", \"upLeft\" and \"upRight\" for eight, each with the name of an animation.");
                }
                state.directions[static_cast<std::size_t>(*found)] = animation.get<std::string>();
            }
            std::size_t straight = 0;
            std::size_t diagonals = 0;
            for (std::size_t index = 0; index < state.directions.size(); ++index) {
                const bool named = !state.directions[index].empty();
                straight += named && index % 2 == 0 ? 1 : 0;
                diagonals += named && index % 2 == 1 ? 1 : 0;
            }
            if (straight != 4 || (diagonals != 0 && diagonals != 4)) {
                throw std::invalid_argument("The directions of the state \"" + name + "\" name the four straight directions, or all eight.");
            }
            state.eightWays = diagonals == 4;
        }
        graph.states.push_back(std::move(state));
    }

    const std::string initial = description.value("initial", graph.states.front().name);
    const std::optional<std::size_t> start = graph.findState(initial);
    if (!start) {
        throw std::invalid_argument("The initial state \"" + initial + "\" does not exist.");
    }
    graph.initial = *start;

    std::size_t number = 0;
    for (const core::Json& entry : description.value("transitions", core::Json::array())) {
        ++number;
        const std::string where = "the transition " + std::to_string(number);
        if (!entry.is_object()) {
            throw std::invalid_argument("The transition " + std::to_string(number) + " must be an object with \"from\" and \"to\".");
        }
        for (const auto& [key, field] : entry.items()) {
            if (key != "from" && key != "to" && key != "when" && key != "exitTime" && key != "atEnd" && key != "fade") {
                throw std::invalid_argument("The transition " + std::to_string(number) + " has the unknown key \"" + key + "\".");
            }
        }
        Transition transition;
        const std::string from = entry.value("from", std::string("any"));
        if (from != "any") {
            transition.from = graph.findState(from);
            if (!transition.from) {
                throw std::invalid_argument("The transition " + std::to_string(number) + " starts from the unknown state \"" + from + "\".");
            }
        }
        const std::string to = entry.value("to", std::string());
        const std::optional<std::size_t> target = graph.findState(to);
        if (!target) {
            throw std::invalid_argument("The transition " + std::to_string(number) + " goes to the unknown state \"" + to + "\".");
        }
        transition.to = *target;
        if (entry.contains("when")) {
            const std::string condition = entry.at("when").get<std::string>();
            Parser parser(graph, condition, where);
            transition.condition = parser.parse();
            transition.triggers = std::move(parser.triggers);
        }
        transition.exitTime = entry.value("atEnd", false) ? 1.0F : entry.value("exitTime", 0.0F);
        transition.fade = entry.value("fade", 0.0F);
        if (!(transition.exitTime >= 0.0F && transition.fade >= 0.0F)) {
            throw std::invalid_argument("The transition " + std::to_string(number) + " needs an exit time and a fade of at least 0.");
        }
        graph.transitions.push_back(std::move(transition));
    }
    return graph;
}

const StateGraph::Parameter* StateGraph::findParameter(std::string_view name) const noexcept {
    const auto found = std::ranges::find(parameters, name, &Parameter::name);
    return found != parameters.end() ? &*found : nullptr;
}

std::optional<std::size_t> StateGraph::findState(std::string_view name) const noexcept {
    const auto found = std::ranges::find(states, name, &State::name);
    return found != states.end() ? std::optional(static_cast<std::size_t>(found - states.begin())) : std::nullopt;
}

float StateGraph::valueOf(std::size_t term, const std::vector<float>& values) const noexcept {
    const Term& node = terms[term];
    return node.op == Term::Op::Constant ? node.constant : node.op == Term::Op::Value ? values[node.slot] : (evaluate(term, values) ? 1.0F : 0.0F);
}

bool StateGraph::evaluate(std::size_t condition, const std::vector<float>& values) const noexcept {
    const Term& node = terms[condition];
    switch (node.op) {
    case Term::Op::Constant:
        return node.constant != 0.0F;
    case Term::Op::Value:
        return values[node.slot] != 0.0F;
    case Term::Op::Not:
        return !evaluate(node.left, values);
    case Term::Op::And:
        return evaluate(node.left, values) && evaluate(node.right, values);
    case Term::Op::Or:
        return evaluate(node.left, values) || evaluate(node.right, values);
    case Term::Op::Equal:
        return valueOf(node.left, values) == valueOf(node.right, values);
    case Term::Op::NotEqual:
        return valueOf(node.left, values) != valueOf(node.right, values);
    case Term::Op::Less:
        return valueOf(node.left, values) < valueOf(node.right, values);
    case Term::Op::LessEqual:
        return valueOf(node.left, values) <= valueOf(node.right, values);
    case Term::Op::Greater:
        return valueOf(node.left, values) > valueOf(node.right, values);
    case Term::Op::GreaterEqual:
        break;
    }
    return valueOf(node.left, values) >= valueOf(node.right, values);
}

// Directions follow the screen, where y grows downward, so a vector of (0, 1) points down.
std::size_t StateGraph::pickDirection(const State& state, const std::vector<float>& values, std::size_t current) const noexcept {
    const std::size_t slot = *state.by;
    const float x = values[slot];
    const float y = values[slot + 1];
    if (x == 0.0F && y == 0.0F) {
        return current;
    }
    const std::size_t sectors = state.eightWays ? 8 : 4;
    const float width = std::numbers::pi_v<float> * 2.0F / static_cast<float>(sectors);
    float angle = std::atan2(y, x);
    if (angle < 0.0F) {
        angle += std::numbers::pi_v<float> * 2.0F;
    }
    const std::size_t step = 8 / sectors;
    if (current % step == 0 && !state.directions[current].empty()) {
        float distance = std::fabs(angle - static_cast<float>(current / step) * width);
        distance = std::min(distance, std::numbers::pi_v<float> * 2.0F - distance);
        if (distance <= width * 0.5F * (1.0F + kHysteresis)) {
            return current;
        }
    }
    return (static_cast<std::size_t>(std::lround(angle / width)) % sectors) * step;
}

std::vector<std::string> StateGraph::getAnimationNames() const {
    std::vector<std::string> names;
    for (const State& state : states) {
        for (const std::string& name : state.directions) {
            if (!name.empty() && std::ranges::find(names, name) == names.end()) {
                names.push_back(name);
            }
        }
        if (!state.animation.empty() && std::ranges::find(names, state.animation) == names.end()) {
            names.push_back(state.animation);
        }
    }
    return names;
}

} // namespace haylen::animation2d
