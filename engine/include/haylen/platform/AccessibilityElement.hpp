#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/core/Json.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::platform {

// An element of the interface as a screen reader sees it: what it is, what it says, the state it is in and what the reader can do with it. Elements form a tree through the id of their parent, where 0 is the root, and come in the order a reader moves through them.
struct AccessibilityElement {
    enum class Role : std::uint8_t {
        Group,
        Text,
        Heading,
        Image,
        Button,
        CheckBox,
        Switch,
        RadioGroup,
        RadioButton,
        TabList,
        Tab,
        Slider,
        TextField,
        ComboBox,
        ProgressBar,
        List,
        Dialog,
        Alert,
        PlayArea,
    };

    // The states and the actions are bits that combine.
    enum State : std::uint8_t {
        Disabled = 1U << 0U,
        Checked = 1U << 1U,
        Selected = 1U << 2U,
        Focused = 1U << 3U,
        Expanded = 1U << 4U,
        Busy = 1U << 5U,
    };

    enum Action : std::uint8_t {
        Focus = 1U << 0U,
        Activate = 1U << 1U,
        Increment = 1U << 2U,
        Decrement = 1U << 3U,
    };

    static constexpr std::array<std::pair<std::string_view, Role>, 19> kRoleNames{{
        {"group", Role::Group}, {"text", Role::Text}, {"heading", Role::Heading}, {"image", Role::Image}, {"button", Role::Button}, {"checkBox", Role::CheckBox}, {"switch", Role::Switch}, {"radioGroup", Role::RadioGroup}, {"radioButton", Role::RadioButton}, {"tabList", Role::TabList}, {"tab", Role::Tab}, {"slider", Role::Slider}, {"textField", Role::TextField}, {"comboBox", Role::ComboBox}, {"progressBar", Role::ProgressBar}, {"list", Role::List}, {"dialog", Role::Dialog}, {"alert", Role::Alert}, {"playArea", Role::PlayArea},
    }};
    static constexpr std::array<std::pair<std::string_view, State>, 6> kStateNames{{{"disabled", Disabled}, {"checked", Checked}, {"selected", Selected}, {"focused", Focused}, {"expanded", Expanded}, {"busy", Busy}}};
    static constexpr std::array<std::pair<std::string_view, Action>, 4> kActionNames{{{"focus", Focus}, {"activate", Activate}, {"increment", Increment}, {"decrement", Decrement}}};

    std::uint64_t id = 0;
    std::uint64_t parent = 0;

    // Where the element lies on the screen, in framebuffer pixels from the top left corner.
    math::Rect bounds;
    Role role = Role::Group;
    std::string label;
    std::string value;
    std::string hint;
    std::uint8_t states = 0;
    std::uint8_t actions = 0;

    [[nodiscard]] bool operator==(const AccessibilityElement&) const = default;

    [[nodiscard]] bool has(State state) const noexcept {
        return (states & state) != 0U;
    }
    [[nodiscard]] bool can(Action action) const noexcept {
        return (actions & action) != 0U;
    }

    // The element as the JSON that the screen readers of Android and the web receive it in, with its role by name, its bounds as `[x, y, width, height]` and its states and actions as the bits of `State` and `Action`.
    [[nodiscard]] core::Json toJson() const;

    // The tree as the JSON text that the screen readers of Android and the web receive it in, with the focused element, where the bytes of a text that is no UTF-8 become replacement characters, so no text of an app stops the tree.
    [[nodiscard]] static std::string writeTree(std::span<const AccessibilityElement> elements, std::uint64_t focused);

    [[nodiscard]] static std::string_view roleName(Role value) noexcept;
    [[nodiscard]] static std::optional<Role> roleFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::optional<Action> actionFromName(std::string_view name) noexcept;
};

} // namespace haylen::platform
