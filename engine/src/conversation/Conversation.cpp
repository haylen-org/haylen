#include "haylen/conversation/Conversation.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <unordered_set>

namespace haylen::conversation {

// The names come first, so steps and choices reach nodes that the document lists after them.
Conversation Conversation::parse(const core::Json& document) {
    if (!document.is_object() || !document.contains("nodes") || !document.at("nodes").is_object() || document.at("nodes").empty()) {
        throw std::invalid_argument("A conversation is an object with \"nodes\", an object of at least one node by name.");
    }
    requireKeys(document, std::array<std::string_view, 2>{"variables", "nodes"}, "The conversation");

    Conversation conversation;
    if (document.contains("variables")) {
        if (!document.at("variables").is_object()) {
            throw std::invalid_argument("The \"variables\" of a conversation are an object of values by name.");
        }
        conversation.variables = document.at("variables");
    }
    for (const auto& [name, node] : document.at("nodes").items()) {
        conversation.indices.emplace(name, conversation.nodes.size());
        conversation.nodes.push_back({.name = name, .steps = {}, .choices = {}, .next = std::nullopt});
    }

    for (Node& node : conversation.nodes) {
        const core::Json& value = document.at("nodes").at(node.name);
        const std::string where = std::format("The node \"{}\"", node.name);
        if (!value.is_object()) {
            throw std::invalid_argument(std::format("{} is not an object with \"steps\", \"choices\" and \"next\".", where));
        }
        requireKeys(value, kNodeKeys, where);
        if (value.contains("steps")) {
            if (!value.at("steps").is_array()) {
                throw std::invalid_argument(std::format("{} has \"steps\" that are not a list.", where));
            }
            for (std::size_t index = 0; index < value.at("steps").size(); ++index) {
                node.steps.push_back(conversation.readStep(value.at("steps").at(index), node.name, index));
            }
        }
        if (value.contains("choices")) {
            if (!value.at("choices").is_array()) {
                throw std::invalid_argument(std::format("{} has \"choices\" that are not a list.", where));
            }
            for (std::size_t index = 0; index < value.at("choices").size(); ++index) {
                node.choices.push_back(conversation.readChoice(value.at("choices").at(index), node.name, index));
            }
        }
        if (value.contains("next") && !value.at("next").is_null()) {
            node.next = conversation.readTarget(value.at("next"), where);
        }
    }
    return conversation;
}

Conversation::Step Conversation::readStep(const core::Json& value, const std::string& node, std::size_t index) const {
    const std::string where = std::format("The step {} of the node \"{}\"", index + 1, node);
    if (!value.is_object()) {
        throw std::invalid_argument(std::format("{} is not an object.", where));
    }
    Step step;
    step.condition = readCondition(value, where);

    if (value.contains("text")) {
        requireKeys(value, kLineKeys, where);
        step.kind = Step::Kind::Line;
        const core::Json& text = value.at("text");
        if (text.is_string()) {
            step.texts.push_back(text.get<std::string>());
        } else if (text.is_array() && !text.empty() && std::ranges::all_of(text, [](const core::Json& item) { return item.is_string(); })) {
            step.texts = text.get<std::vector<std::string>>();
        } else {
            throw std::invalid_argument(std::format("{} has a \"text\" that is not a text or a list of texts to pick from.", where));
        }
        if (value.contains("speaker")) {
            if (!value.at("speaker").is_string()) {
                throw std::invalid_argument(std::format("{} has a \"speaker\" that is not a text.", where));
            }
            step.speaker = value.at("speaker").get<std::string>();
        }
        if (value.contains("tags")) {
            if (!value.at("tags").is_array() || !std::ranges::all_of(value.at("tags"), [](const core::Json& item) { return item.is_string(); })) {
                throw std::invalid_argument(std::format("{} has \"tags\" that are not a list of texts.", where));
            }
            step.tags = value.at("tags").get<std::vector<std::string>>();
        }
        return step;
    }
    if (value.contains("command")) {
        requireKeys(value, kCommandKeys, where);
        step.kind = Step::Kind::Command;
        if (!value.at("command").is_string() || value.at("command").get_ref<const std::string&>().empty()) {
            throw std::invalid_argument(std::format("{} has a \"command\" that is not a name.", where));
        }
        step.command = value.at("command").get<std::string>();
        step.arguments = value.contains("arguments") ? value.at("arguments") : core::Json::object();
        return step;
    }
    if (value.contains("set")) {
        requireKeys(value, kSetKeys, where);
        step.kind = Step::Kind::Set;
        if (!value.at("set").is_object()) {
            throw std::invalid_argument(std::format("{} has a \"set\" that is not an object of values by variable.", where));
        }
        for (const auto& [name, assigned] : value.at("set").items()) {
            if (!assigned.is_string() && !assigned.is_number() && !assigned.is_boolean()) {
                throw std::invalid_argument(std::format("{} sets \"{}\" to something other than an expression, a number or a boolean.", where, name));
            }
            try {
                step.assignments.emplace_back(name, Expression::parse(assigned.is_string() ? assigned.get<std::string>() : assigned.dump()));
            } catch (const std::invalid_argument& error) {
                throw std::invalid_argument(std::format("{} sets \"{}\" badly. {}", where, name, error.what()));
            }
        }
        return step;
    }
    if (value.contains("jump")) {
        requireKeys(value, kJumpKeys, where);
        step.kind = Step::Kind::Jump;
        step.target = readTarget(value.at("jump"), where);
        return step;
    }
    throw std::invalid_argument(std::format("{} has none of \"text\", \"command\", \"set\" and \"jump\".", where));
}

Conversation::Choice Conversation::readChoice(const core::Json& value, const std::string& node, std::size_t index) const {
    const std::string where = std::format("The choice {} of the node \"{}\"", index + 1, node);
    if (!value.is_object() || !value.contains("text") || !value.at("text").is_string()) {
        throw std::invalid_argument(std::format("{} is not an object with a \"text\".", where));
    }
    requireKeys(value, kChoiceKeys, where);
    Choice choice{.text = value.at("text").get<std::string>(), .condition = readCondition(value, where), .next = std::nullopt, .once = false};
    if (value.contains("next") && !value.at("next").is_null()) {
        choice.next = readTarget(value.at("next"), where);
    }
    if (value.contains("once")) {
        if (!value.at("once").is_boolean()) {
            throw std::invalid_argument(std::format("{} has a \"once\" that is not a boolean.", where));
        }
        choice.once = value.at("once").get<bool>();
    }
    return choice;
}

std::size_t Conversation::readTarget(const core::Json& value, const std::string& where) const {
    if (!value.is_string()) {
        throw std::invalid_argument(std::format("{} leads to something other than the name of a node.", where));
    }
    const auto found = indices.find(value.get<std::string>());
    if (found == indices.end()) {
        throw std::invalid_argument(std::format("{} leads to the node \"{}\", which the conversation does not have.", where, value.get<std::string>()));
    }
    return found->second;
}

std::optional<Expression> Conversation::readCondition(const core::Json& value, const std::string& where) {
    if (!value.contains("if")) {
        return std::nullopt;
    }
    if (!value.at("if").is_string()) {
        throw std::invalid_argument(std::format("{} has an \"if\" that is not an expression.", where));
    }
    try {
        return Expression::parse(value.at("if").get<std::string>());
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(std::format("{} has a condition it cannot read. {}", where, error.what()));
    }
}

void Conversation::requireKeys(const core::Json& value, std::span<const std::string_view> keys, const std::string& where) {
    for (const auto& [key, item] : value.items()) {
        if (std::ranges::find(keys, key) == keys.end()) {
            throw std::invalid_argument(std::format("{} has the unknown key \"{}\".", where, key));
        }
    }
}

std::optional<std::size_t> Conversation::findNode(std::string_view name) const {
    const auto found = indices.find(std::string(name));
    return found == indices.end() ? std::nullopt : std::optional<std::size_t>(found->second);
}

std::vector<std::string> Conversation::getTexts() const {
    std::vector<std::string> texts;
    std::unordered_set<std::string> seen;
    // clang-format off
    const auto keep = [&texts, &seen](const std::string& text) {
        if (seen.insert(text).second) {
            texts.push_back(text);
        }
    };
    // clang-format on
    for (const Node& node : nodes) {
        for (const Step& step : node.steps) {
            std::ranges::for_each(step.texts, keep);
        }
        for (const Choice& choice : node.choices) {
            keep(choice.text);
        }
    }
    return texts;
}

} // namespace haylen::conversation
