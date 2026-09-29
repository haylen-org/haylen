#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// Creates components by kind, so a kind nobody registered is refused by name rather than drawn as nothing.
class ComponentRegistry final {
  public:
    using Factory = std::function<std::unique_ptr<Component>()>;

    void add(std::string kind, Factory factory);
    [[nodiscard]] std::unique_ptr<Component> create(std::string_view kind) const;
    [[nodiscard]] bool contains(std::string_view kind) const;
    [[nodiscard]] std::vector<std::string> getKinds() const;

    template <typename T> void add(std::string kind) {
        add(std::move(kind), [] { return std::make_unique<T>(); });
    }

  private:
    std::map<std::string, Factory, std::less<>> factories;
};

} // namespace haylen::ui
