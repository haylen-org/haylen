#include "debug/Breakpoints.hpp"

#include <charconv>
#include <utility>

namespace haylen::debug {

std::optional<Breakpoints::HitCondition> Breakpoints::HitCondition::parse(std::string_view text) {
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    if (text.empty()) {
        return HitCondition{};
    }

    HitCondition condition{.op = Operator::Equal};
    for (const auto& [prefix, op] : {std::pair{std::string_view(">="), Operator::AtLeast}, std::pair{std::string_view("=="), Operator::Equal}, std::pair{std::string_view(">"), Operator::Greater}, std::pair{std::string_view("%"), Operator::Multiple}}) {
        if (text.starts_with(prefix)) {
            condition.op = op;
            text.remove_prefix(prefix.size());
            break;
        }
    }
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }

    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), condition.count);
    if (error != std::errc() || end != text.data() + text.size() || condition.count < 0 || (condition.op == Operator::Multiple && condition.count == 0)) {
        return std::nullopt;
    }
    return condition;
}

bool Breakpoints::HitCondition::passes(int hits) const noexcept {
    switch (op) {
    case Operator::Always:
        return true;
    case Operator::Equal:
        return hits == count;
    case Operator::AtLeast:
        return hits >= count;
    case Operator::Greater:
        return hits > count;
    case Operator::Multiple:
        return hits % count == 0;
    }
    return true;
}

bool Breakpoints::File::covers(int first, int last) const {
    const auto found = lines.lower_bound(first);
    return found != lines.end() && found->first <= last;
}

Breakpoints::Breakpoint* Breakpoints::File::find(int line) {
    const auto found = lines.find(line);
    return found == lines.end() ? nullptr : &found->second;
}

// A line asked for twice keeps the last request, and a request whose count of hits cannot be read never takes effect.
std::vector<Breakpoints::Result> Breakpoints::set(const std::string& path, const std::vector<Request>& requests) {
    if (const auto found = files.find(path); found != files.end()) {
        count(found->second, -1);
        files.erase(found);
    }

    File file;
    std::vector<Result> results;
    results.reserve(requests.size());
    for (const Request& request : requests) {
        const int id = nextId++;
        const std::optional<HitCondition> hit = HitCondition::parse(request.hitCondition);
        if (!hit) {
            results.push_back({.id = id, .line = request.line, .message = "The hit count \"" + request.hitCondition + "\" is not a count, which is a whole number alone or after \"==\", \">=\", \">\" or \"%\"."});
            continue;
        }
        if (request.line <= 0) {
            results.push_back({.id = id, .line = request.line, .message = "The line of a breakpoint starts from 1."});
            continue;
        }
        file.lines[request.line] = Breakpoint{.id = id, .line = request.line, .condition = request.condition, .hit = *hit, .logMessage = request.logMessage};
        results.push_back({.id = id, .line = request.line, .verified = true});
    }

    if (!file.empty()) {
        count(file, 1);
        files.emplace(path, std::move(file));
    }
    return results;
}

void Breakpoints::clear() {
    files.clear();
    lineCounts.clear();
}

Breakpoints::File* Breakpoints::find(std::string_view source) {
    if (!source.starts_with('@')) {
        return nullptr;
    }
    const auto found = files.find(source.substr(1));
    return found == files.end() ? nullptr : &found->second;
}

void Breakpoints::count(const File& file, int delta) {
    for (const auto& [line, breakpoint] : file.lines) {
        const auto index = static_cast<std::size_t>(line);
        if (index >= lineCounts.size()) {
            lineCounts.resize(index + 1, 0);
        }
        lineCounts[index] += delta;
    }
}

} // namespace haylen::debug
