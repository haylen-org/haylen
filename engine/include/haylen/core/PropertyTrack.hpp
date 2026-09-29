#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/TweenProperty.hpp"
#include "haylen/core/TweenTrack.hpp"
#include "haylen/core/TweenValue.hpp"

namespace haylen::core {

// A tween track that animates one native value through a getter and a setter, such as the position of a sprite or the zoom of a camera, without running any script.
class PropertyTrack final : public TweenTrack {
  public:
    using Getter = std::function<TweenValue()>;
    using Setter = std::function<void(const TweenValue&)>;

    // The target and the field names identify the value for overwriting and killing by target, and a value spread over two fields, such as x and y, names both. The alive check, when given, stops the tween once the object is gone, and a weak pointer to the object is the usual way to write it.
    PropertyTrack(const void* owner, std::vector<std::string> names, Getter read, Setter write, TweenProperty value, std::function<bool()> alive = {});

    void begin() override;
    void render(float progress, int loops) override;
    [[nodiscard]] bool isAlive() const override;
    [[nodiscard]] float getDistance() const override;
    [[nodiscard]] const void* getTarget() const noexcept override {
        return target;
    }
    [[nodiscard]] std::vector<std::string> getFields() const override;
    bool release(std::string_view name) override;

  private:
    const void* target;
    std::vector<std::string> fields;
    Getter getter;
    Setter setter;
    TweenProperty property;
    std::function<bool()> aliveCheck;
    bool released = false;
};

} // namespace haylen::core
