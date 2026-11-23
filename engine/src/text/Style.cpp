#include "haylen/text/Style.hpp"

#include <algorithm>

namespace haylen::text {

const std::array<std::pair<std::string_view, Alignment>, 6> Style::kAlignmentNames{{{"start", Alignment::Start}, {"end", Alignment::End}, {"left", Alignment::Left}, {"center", Alignment::Center}, {"right", Alignment::Right}, {"fill", Alignment::Fill}}};
const std::array<std::pair<std::string_view, Direction>, 3> Style::kDirectionNames{{{"auto", Direction::Auto}, {"leftToRight", Direction::LeftToRight}, {"rightToLeft", Direction::RightToLeft}}};
const std::array<std::pair<std::string_view, Balance>, 3> Style::kBalanceNames{{{"none", Balance::None}, {"lastLine", Balance::LastLine}, {"allLines", Balance::AllLines}}};
const std::array<std::pair<std::string_view, LineHeight>, 2> Style::kLineHeightNames{{{"natural", LineHeight::Natural}, {"uniform", LineHeight::Uniform}}};

std::optional<Alignment> Style::alignmentFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kAlignmentNames, name, &std::pair<std::string_view, Alignment>::first);
    return found != kAlignmentNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Style::alignmentName(Alignment value) noexcept {
    return std::ranges::find(kAlignmentNames, value, &std::pair<std::string_view, Alignment>::second)->first;
}

std::optional<Direction> Style::directionFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kDirectionNames, name, &std::pair<std::string_view, Direction>::first);
    return found != kDirectionNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Style::directionName(Direction value) noexcept {
    return std::ranges::find(kDirectionNames, value, &std::pair<std::string_view, Direction>::second)->first;
}

std::optional<Balance> Style::balanceFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kBalanceNames, name, &std::pair<std::string_view, Balance>::first);
    return found != kBalanceNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Style::balanceName(Balance value) noexcept {
    return std::ranges::find(kBalanceNames, value, &std::pair<std::string_view, Balance>::second)->first;
}

std::optional<LineHeight> Style::lineHeightFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kLineHeightNames, name, &std::pair<std::string_view, LineHeight>::first);
    return found != kLineHeightNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Style::lineHeightName(LineHeight value) noexcept {
    return std::ranges::find(kLineHeightNames, value, &std::pair<std::string_view, LineHeight>::second)->first;
}

} // namespace haylen::text
