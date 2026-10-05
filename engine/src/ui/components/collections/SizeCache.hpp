#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace haylen::ui {

class CollectionLayout;

// The lengths of the items of a collection along its axis and of the lines its layout puts them in. An item that was never laid out takes the estimate of its type, which the measured items of the type teach unless the type declares one, and a measured item keeps its length through inserts, removals and moves. The lines keep their lengths and the gap after each one in a Fenwick tree, so the offset of a line and the line at an offset cost a logarithm of the line count.
class SizeCache final {
  public:
    // The type of the items that have none, such as the items of pages that did not load yet in a collection without a placeholder type.
    static constexpr std::uint16_t kNoType = std::numeric_limits<std::uint16_t>::max();

    // The index `remap` receives for an item that is new.
    static constexpr std::size_t kNew = std::numeric_limits<std::size_t>::max();

    // Sets the estimate each type declares, where zero learns it from the measured items of the type, starting from `fallback`, which items without a type take too. Learning starts again.
    void setEstimates(std::span<const float> declared, float fallback);

    // Gives the items of a type a length that needs no measuring, or makes them measured again with nothing.
    void setFixedLength(std::uint16_t type, std::optional<float> value);

    void assign(std::span<const std::uint16_t> types);
    void insert(std::size_t index, std::span<const std::uint16_t> types);
    void erase(std::size_t index, std::size_t count);
    void move(std::size_t from, std::size_t to);

    // Builds the items from the items before, where `previous` holds for every item its index before or `kNew`.
    void remap(std::span<const std::size_t> previous, std::span<const std::uint16_t> types);

    // Gives an item another type, which forgets its measured length.
    void setType(std::size_t index, std::uint16_t type);

    // Records the length of an item that was laid out and returns whether the length of its line changed.
    bool measure(std::size_t index, float length);

    // Keeps every measured length as an estimate until its item is measured again, such as after the cross length changed.
    void markStale() noexcept;

    [[nodiscard]] std::size_t size() const noexcept {
        return items.size();
    }
    [[nodiscard]] std::uint16_t getType(std::size_t index) const noexcept {
        return items[index].type;
    }
    [[nodiscard]] float getItemLength(std::size_t index) const noexcept;
    [[nodiscard]] bool isMeasured(std::size_t index) const noexcept;

    // Whether the type of the item gives it a length that needs no measuring.
    [[nodiscard]] bool isFixed(std::size_t index) const noexcept;
    [[nodiscard]] float getEstimate(std::uint16_t type) const noexcept;

    // Computes the length of every line of the layout, the longest of its items plus the gap after it.
    void arrange(const CollectionLayout& layout, float gap);

    // Whether the items changed or an estimate moved since the lines were arranged.
    [[nodiscard]] bool needsArrange() const noexcept {
        return !arranged;
    }
    [[nodiscard]] std::size_t getLineCount() const noexcept {
        return lines.size();
    }
    [[nodiscard]] double getLineOffset(std::size_t line) const noexcept;
    [[nodiscard]] float getLineLength(std::size_t line) const noexcept;

    // Returns the line at an offset from the start of the first line, the first or last line for offsets before or after them.
    [[nodiscard]] std::size_t findLine(double offset) const noexcept;
    [[nodiscard]] double getTotal() const noexcept;

  private:
    enum class State : std::uint8_t {
        Unmeasured,
        Stale,
        Measured,
    };

    struct Item {
        float length = 0.0F;
        std::uint16_t type = kNoType;
        State state = State::Unmeasured;
    };

    struct Estimate {
        float declared = 0.0F;
        std::optional<float> fixed;
        double sum = 0.0;
        std::size_t count = 0;
        float learned = 0.0F;
    };

    // An estimate that moves by less than this keeps the lines as they are, so learning settles instead of arranging every frame.
    static constexpr float kLearningStep = 1.0F;

    [[nodiscard]] float measureLine(std::size_t line) const noexcept;
    void build();
    void add(std::size_t line, double delta) noexcept;
    void learn(Item& item, float length);

    std::vector<Item> items;
    std::vector<Estimate> estimates;
    float fallback = 0.0F;

    const CollectionLayout* layout = nullptr;
    float gap = 0.0F;
    bool arranged = false;
    std::vector<float> lines;
    std::vector<double> tree;
};

} // namespace haylen::ui
