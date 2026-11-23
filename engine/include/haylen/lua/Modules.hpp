#pragma once

#include <cstddef>
#include <memory>
#include <string_view>
#include <typeindex>
#include <vector>

#include "haylen/lua/Module.hpp"

namespace haylen::lua {

// Owns the Lua modules of an app in the order they install, which puts every module after the modules it builds on, and finds them by type.
class Modules final {
  public:
    // Throws `std::invalid_argument` when a module of the same name is already registered.
    template <typename T> T& add(std::unique_ptr<T> module) {
        checkName(module->getName());
        T& reference = *module;
        entries.push_back({std::type_index(typeid(T)), std::move(module)});
        return reference;
    }

    template <typename T> [[nodiscard]] T* find() const noexcept {
        for (const Entry& entry : entries) {
            if (entry.type == std::type_index(typeid(T))) {
                return static_cast<T*>(entry.module.get());
            }
        }

        return nullptr;
    }

    // Throws `std::logic_error` when no module of the type is registered.
    template <typename T> [[nodiscard]] T& get() const {
        T* module = find<T>();
        if (module == nullptr) {
            throwMissing();
        }

        return *module;
    }

    [[nodiscard]] Module& operator[](std::size_t index) const noexcept {
        return *entries[index].module;
    }
    [[nodiscard]] std::size_t size() const noexcept {
        return entries.size();
    }

    // Releases the modules in reverse order, so later modules can still rely on the ones they were built on.
    void clear() noexcept;

  private:
    struct Entry {
        std::type_index type;
        std::unique_ptr<Module> module;
    };

    void checkName(std::string_view name) const;
    [[noreturn]] static void throwMissing();

    std::vector<Entry> entries;
};

} // namespace haylen::lua
