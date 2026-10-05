#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// Lays children out in a line, down a column or across a row, sharing extra space among children that grow within their size bounds and placing the rest by `justify`. A row that wraps breaks its children into lines.
class Linear : public Component {
  public:
    explicit Linear(bool isHorizontal) : horizontal(isHorizontal) {}

    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return kUnlimitedChildren;
    }

  protected:
    void readProperties(PropertyReader& reader) override;
    virtual void readMore(PropertyReader&) {}
    virtual void paint(Context&, const math::Rect&) {}
    [[nodiscard]] virtual math::Insets getPadding(Context&) const;
    [[nodiscard]] float getGap(Context& context) const;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    enum class Justify : std::uint8_t {
        Start,
        Center,
        End,
        SpaceBetween,
        SpaceAround,
        SpaceEvenly,
    };

    // A child on its way to its place: the length it starts from along the main axis, the length it takes and whether it still grows.
    struct Slot {
        Component* child = nullptr;
        float base = 0.0F;
        float length = 0.0F;
        bool growing = false;
    };

    static constexpr std::array<std::pair<std::string_view, Justify>, 6> kJustify{{
        {"start", Justify::Start},
        {"center", Justify::Center},
        {"end", Justify::End},
        {"spaceBetween", Justify::SpaceBetween},
        {"spaceAround", Justify::SpaceAround},
        {"spaceEvenly", Justify::SpaceEvenly},
    }};
    static constexpr std::array<std::pair<std::string_view, Alignment>, 4> kAlignments{{
        {"start", Alignment::Start},
        {"center", Alignment::Center},
        {"end", Alignment::End},
        {"stretch", Alignment::Stretch},
    }};

    [[nodiscard]] float getLineGap(Context& context) const;
    [[nodiscard]] Alignment getChildAlignment(const Component& child) const noexcept;
    [[nodiscard]] float measureLength(Context& context, Component& child, float crossLength) const;
    [[nodiscard]] float boundLength(const Component& child, float length) const noexcept;
    [[nodiscard]] math::Vec2 measureWrapped(Context& context, float inner);
    void breakLines(Context& context, float inner);
    void grow(std::size_t first, std::size_t last, float available, float gap);
    void place(Context& context, const math::Rect& line, std::size_t first, std::size_t last, float gap);

    bool horizontal;
    bool wrap = false;
    std::optional<float> gap;
    std::optional<float> lineGap;
    math::Insets padding;
    Justify justify = Justify::Start;
    std::optional<Alignment> alignItems;

    // The children of the last layout and where each wrapped line starts, kept so a frame reuses their storage.
    std::vector<Slot> slots;
    std::vector<std::size_t> lineStarts;
};

} // namespace haylen::ui
