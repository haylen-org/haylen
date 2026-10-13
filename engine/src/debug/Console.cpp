#include "haylen/debug/Console.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <exception>
#include <format>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <fast_float/fast_float.h>

namespace haylen::debug {

Console::Console() {
    helpCommand = addCommand({.name = "help", .help = "Lists the commands, or shows how to type one.", .parameters = {{.kind = Kind::String, .optional = true}}, .handler = [this](const core::Json& arguments) { return help(arguments); }});
}

void Console::Entry::disconnect() {
    if (console != nullptr) {
        std::erase_if(console->entries, [this](const std::shared_ptr<Entry>& entry) { return entry.get() == this; });
        console = nullptr;
    }
}

// Names are single words, so a line always starts with the name of its command.
core::Connection Console::addCommand(Command command) {
    if (command.name.empty() || std::ranges::any_of(command.name, [](char character) { return character == ' ' || character == '\t' || character == '"'; })) {
        throw std::invalid_argument(std::format("The command name \"{}\" is not one word. Name a command with letters, digits and symbols without spaces or quotes.", command.name));
    }
    const auto text = std::ranges::find(command.parameters, Kind::Text, &Parameter::kind);
    if (text != command.parameters.end() && text + 1 != command.parameters.end()) {
        throw std::invalid_argument(std::format("The command \"{}\" has an argument of the kind \"text\" before another argument. A text argument takes the rest of the line, so it comes last.", command.name));
    }
    const auto optional = std::ranges::find_if(command.parameters, &Parameter::optional);
    if (std::any_of(optional, command.parameters.end(), [](const Parameter& parameter) { return !parameter.optional; })) {
        throw std::invalid_argument(std::format("The command \"{}\" has a required argument after an optional one. Put the optional arguments last.", command.name));
    }

    removeCommand(command.name);
    auto entry = std::make_shared<Entry>();
    entry->console = this;
    entry->command = std::move(command);
    entries.insert(std::ranges::upper_bound(entries, entry->command.name, {}, [](const std::shared_ptr<Entry>& item) -> const std::string& { return item->command.name; }), entry);
    return core::Connection(std::weak_ptr<core::Connection::Link>(entry));
}

bool Console::removeCommand(std::string_view name) {
    const std::shared_ptr<Entry> entry = find(name);
    if (!entry) {
        return false;
    }
    entry->disconnect();
    return true;
}

bool Console::hasCommand(std::string_view name) const {
    return find(name) != nullptr;
}

std::shared_ptr<Console::Entry> Console::find(std::string_view name) const {
    const auto found = std::ranges::find(entries, name, [](const std::shared_ptr<Entry>& entry) -> std::string_view { return entry->command.name; });
    return found == entries.end() ? nullptr : *found;
}

std::vector<const Console::Command*> Console::getCommands() const {
    std::vector<const Command*> commands;
    commands.reserve(entries.size());
    for (const std::shared_ptr<Entry>& entry : entries) {
        commands.push_back(&entry->command);
    }
    return commands;
}

Console::Result Console::run(std::string_view line) {
    const std::size_t first = line.find_first_not_of(" \t");
    if (first == std::string_view::npos) {
        return {.ok = true, .output = {}};
    }
    const std::string_view trimmed = line.substr(first, line.find_last_not_of(" \t") - first + 1);
    if (history.empty() || history.back() != trimmed) {
        history.push(std::string(trimmed));
    }
    print(Line::Kind::Input, trimmed);

    Result result = execute(trimmed);
    if (!result.output.empty()) {
        print(result.ok ? Line::Kind::Output : Line::Kind::Error, result.output);
    }
    return result;
}

// The handler may remove its own command, so the entry stays alive until it returns.
Console::Result Console::execute(std::string_view line) {
    try {
        const std::vector<Token> tokens = split(line);
        const std::shared_ptr<Entry> entry = find(tokens.front().text);
        if (!entry || entry->blocked) {
            return {.ok = false, .output = std::format("There is no command \"{}\". Type \"help\" to list the commands.", tokens.front().text)};
        }
        const core::Json arguments = readArguments(entry->command, line, tokens);
        return {.ok = true, .output = entry->command.handler(arguments)};
    } catch (const std::exception& error) {
        return {.ok = false, .output = error.what()};
    }
}

void Console::print(Line::Kind kind, std::string_view text) {
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        output.push({.kind = kind, .text = std::string(text.substr(start, end - start))});
        start = end + 1;
    }
}

// Splits a line into words, where double quotes keep spaces inside one word and a backslash keeps the next character as it is.
std::vector<Console::Token> Console::split(std::string_view line) {
    std::vector<Token> tokens;
    std::size_t index = 0;
    while (index < line.size()) {
        if (line[index] == ' ' || line[index] == '\t') {
            ++index;
            continue;
        }

        Token token{.text = {}, .start = index};
        bool quoted = false;
        for (; index < line.size() && (quoted || (line[index] != ' ' && line[index] != '\t')); ++index) {
            if (line[index] == '\\' && index + 1 < line.size()) {
                token.text += line[++index];
            } else if (line[index] == '"') {
                quoted = !quoted;
            } else {
                token.text += line[index];
            }
        }
        if (quoted) {
            throw std::invalid_argument("The line has a quote that is not closed.");
        }
        tokens.push_back(std::move(token));
    }
    return tokens;
}

core::Json Console::readArguments(const Command& command, std::string_view line, const std::vector<Token>& tokens) {
    core::Json arguments = core::Json::array();
    const std::size_t given = tokens.size() - 1;
    for (std::size_t index = 0; index < command.parameters.size(); ++index) {
        const Parameter& parameter = command.parameters[index];
        if (index >= given) {
            if (!parameter.optional) {
                throw std::invalid_argument(std::format("The command \"{}\" needs {} as its {} argument. Type it as \"{}\".", command.name, describeKind(parameter.kind), describePosition(index), describeUsage(command)));
            }
            break;
        }
        if (parameter.kind == Kind::Text) {
            arguments.push_back(std::string(line.substr(tokens[index + 1].start)));
            return arguments;
        }
        arguments.push_back(readArgument(command, index, tokens[index + 1].text));
    }
    if (given > command.parameters.size()) {
        throw std::invalid_argument(std::format("The command \"{}\" takes {} {}. Type it as \"{}\".", command.name, command.parameters.size(), command.parameters.size() == 1 ? "argument" : "arguments", describeUsage(command)));
    }
    return arguments;
}

core::Json Console::readArgument(const Command& command, std::size_t index, const std::string& text) {
    const Parameter& parameter = command.parameters[index];
    const char* last = text.data() + text.size();
    switch (parameter.kind) {
    case Kind::Integer: {
        std::int64_t value = 0;
        if (const auto [end, error] = std::from_chars(text.data(), last, value); error == std::errc{} && end == last) {
            return value;
        }
        break;
    }
    case Kind::Number: {
        double value = 0.0;
        if (const auto [end, error] = fast_float::from_chars(text.data(), last, value); error == std::errc{} && end == last && std::isfinite(value)) {
            return value;
        }
        break;
    }
    case Kind::Boolean:
        if (text == "true" || text == "false") {
            return text == "true";
        }
        break;
    case Kind::String:
    case Kind::Text:
        return text;
    }
    throw std::invalid_argument(std::format("The command \"{}\" needs {} as its {} argument, not \"{}\". Type it as \"{}\".", command.name, describeKind(parameter.kind), describePosition(index), text, describeUsage(command)));
}

std::vector<std::string> Console::complete(std::string_view line) const {
    std::vector<std::string> candidates;
    std::vector<Token> tokens;
    try {
        tokens = split(line);
    } catch (const std::invalid_argument&) {
        return candidates;
    }
    const bool open = !line.empty() && line.back() != ' ' && line.back() != '\t';
    if (tokens.empty() || (tokens.size() == 1 && open)) {
        const std::string_view prefix = tokens.empty() ? std::string_view() : std::string_view(tokens.front().text);
        for (const std::shared_ptr<Entry>& entry : entries) {
            if (entry->command.name.starts_with(prefix)) {
                candidates.push_back(entry->command.name + (entry->command.parameters.empty() ? "" : " "));
            }
        }
        return candidates;
    }

    const std::shared_ptr<Entry> entry = find(tokens.front().text);
    const std::size_t index = tokens.size() - (open ? 2 : 1);
    if (!entry || index >= entry->command.parameters.size() || entry->command.parameters[index].kind != Kind::Boolean) {
        return candidates;
    }
    const std::string_view typed = open ? std::string_view(tokens.back().text) : std::string_view();
    const std::string_view kept = line.substr(0, open ? tokens.back().start : line.size());
    for (const std::string_view value : {std::string_view("false"), std::string_view("true")}) {
        if (value.starts_with(typed)) {
            candidates.push_back(std::string(kept) + std::string(value));
        }
    }
    return candidates;
}

std::string Console::describeUsage(const Command& command) {
    std::string usage = command.name;
    for (const Parameter& parameter : command.parameters) {
        const std::string_view kind = describeKind(parameter.kind).substr(describeKind(parameter.kind).find(' ') + 1);
        usage += parameter.optional ? std::format(" [{}]", kind) : std::format(" <{}>", kind);
    }
    return usage;
}

std::string_view Console::describeKind(Kind kind) noexcept {
    switch (kind) {
    case Kind::Integer:
        return "an integer";
    case Kind::Number:
        return "a number";
    case Kind::String:
        return "a word";
    case Kind::Boolean:
        return "a boolean";
    case Kind::Text:
        return "a text";
    }
    return "a word";
}

std::string_view Console::describePosition(std::size_t index) noexcept {
    static constexpr std::string_view kPositions[] = {"first", "second", "third", "fourth", "fifth", "sixth", "seventh", "eighth"};
    return index < std::size(kPositions) ? kPositions[index] : "next";
}

std::string Console::help(const core::Json& arguments) const {
    if (!arguments.empty()) {
        const std::string& name = arguments[0].get_ref<const std::string&>();
        const std::shared_ptr<Entry> entry = find(name);
        if (!entry) {
            throw std::invalid_argument(std::format("There is no command \"{}\". Type \"help\" to list the commands.", name));
        }
        return std::format("{}\n{}", describeUsage(entry->command), entry->command.help);
    }
    std::string text;
    for (const std::shared_ptr<Entry>& entry : entries) {
        text += std::format("{}{}  {}", text.empty() ? "" : "\n", describeUsage(entry->command), entry->command.help);
    }
    return text;
}

} // namespace haylen::debug
