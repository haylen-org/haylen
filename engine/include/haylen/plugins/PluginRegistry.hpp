#pragma once

#include <memory>
#include <string_view>
#include <typeindex>
#include <vector>

#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Owns the plugins of an engine in registration order and finds them by type or by name.
class PluginRegistry final {
  public:
    template <typename T> T& add(std::unique_ptr<T> plugin) {
        T& reference = *plugin;
        entries.push_back({std::type_index(typeid(T)), std::move(plugin)});
        return reference;
    }

    template <typename T> [[nodiscard]] T* find() const noexcept {
        for (const Entry& entry : entries) {
            if (entry.type == std::type_index(typeid(T))) {
                return static_cast<T*>(entry.plugin.get());
            }
        }
        return nullptr;
    }

    // Throws std::logic_error when no plugin of the type is registered.
    template <typename T> [[nodiscard]] T& get() const {
        T* plugin = find<T>();
        if (plugin == nullptr) {
            throwMissing(typeid(T).name());
        }
        return *plugin;
    }

    [[nodiscard]] Plugin* find(std::string_view name) const noexcept;
    [[nodiscard]] std::vector<Plugin*> getAll() const;
    [[nodiscard]] std::size_t size() const noexcept {
        return entries.size();
    }
    void clear() noexcept;

  private:
    struct Entry {
        std::type_index type;
        std::unique_ptr<Plugin> plugin;
    };

    [[noreturn]] static void throwMissing(std::string_view type);

    std::vector<Entry> entries;
};

} // namespace haylen::plugins
