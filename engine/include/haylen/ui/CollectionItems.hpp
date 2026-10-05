#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/ui/CollectionSource.hpp"

namespace haylen::ui {

// A collection source over items kept in C++, for apps written in C++ and for tests. Every change reports itself, so the collection showing the items follows it.
class CollectionItems : public CollectionSource {
  public:
    struct Item {
        std::string id;
        std::string type;
        core::Json fields = core::Json::object();
    };

    // Replaces every item and compares the ids with the items before, so the items that stay keep their sizes, selection and focus.
    void assign(std::vector<Item> value);
    void insert(std::size_t index, std::vector<Item> added);
    void remove(std::size_t index, std::size_t count = 1);
    void move(std::size_t from, std::size_t to);

    // Puts another item at an index, which keeps the cell of the item when the id stays.
    void replace(std::size_t index, Item value);

    // Changes one field of an item, which binds the item again.
    void update(std::size_t index, std::string_view field, core::Json value);

    [[nodiscard]] const Item& getItem(std::size_t index) const;

    [[nodiscard]] std::size_t getCount() const override {
        return items.size();
    }
    [[nodiscard]] std::string_view getId(std::size_t index) const override;
    [[nodiscard]] std::string_view getType(std::size_t index) const override;
    [[nodiscard]] core::Json getValue(std::size_t index, std::string_view field) const override;
    void setValue(std::size_t index, std::string_view field, const core::Json& value) override;

  private:
    // Throws for an index past `limit`, which is the item count or one more for the place after the last item.
    void checkIndex(std::size_t index, std::size_t limit) const;

    // Throws for an id the items would hold twice once `added` takes the place of the items from `first` to `last`.
    void checkIds(const std::vector<Item>& added, std::size_t first, std::size_t last) const;

    std::vector<Item> items;
};

} // namespace haylen::ui
