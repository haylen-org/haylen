#pragma once

#include <memory>
#include <vector>

namespace haylen::plugins {

// The objects of one type that a plugin follows without owning them, such as the emitters that Lua creates, each for as long as something else holds it.
template <typename T> class LiveObjects final {
  public:
    // The objects that ended leave whenever the list would grow, so following stays cheap however many objects come and go.
    void add(const std::shared_ptr<T>& object) {
        if (objects.size() == objects.capacity()) {
            std::erase_if(objects, [](const std::weak_ptr<T>& known) { return known.expired(); });
        }

        objects.push_back(object);
    }

    // Whether an object that still lives answers the predicate.
    template <typename Predicate> [[nodiscard]] bool any(Predicate predicate) const {
        for (const std::weak_ptr<T>& known : objects) {
            if (const std::shared_ptr<T> object = known.lock(); object && predicate(*object)) {
                return true;
            }
        }

        return false;
    }

    void clear() noexcept {
        objects.clear();
    }

  private:
    std::vector<std::weak_ptr<T>> objects;
};

} // namespace haylen::plugins
