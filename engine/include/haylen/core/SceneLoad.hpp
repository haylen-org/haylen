#pragma once

#include <any>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/lua/Error.hpp"

namespace haylen::core {

class Engine;

// The load of a scene, which its load hook receives. The load finishes once the hook returned and every deferral it took and every group it preloads finished, and it fails at its first failure. Its progress is the mean of the progress the scene reports and the progress of each group it preloads.
class SceneLoad final : public std::enable_shared_from_this<SceneLoad> {
  public:
    struct Progress {
        float value = 0.0F;
        std::string message;
    };

    // Keeps the load open until it completes or fails. A deferral destroyed before either fails the load, so a load never waits for work that was dropped.
    class Deferral final {
      public:
        Deferral() = default;
        ~Deferral();

        Deferral(const Deferral&) = delete;
        Deferral& operator=(const Deferral&) = delete;
        Deferral(Deferral&& other) noexcept;
        Deferral& operator=(Deferral&& other) noexcept;

        void complete();
        void fail(const lua::Error& failure);

      private:
        friend class SceneLoad;

        explicit Deferral(std::weak_ptr<SceneLoad> value) : load(std::move(value)) {}

        std::weak_ptr<SceneLoad> load;
    };

    // Receives the failure of a preload, or nothing once its group loaded.
    using PreloadCompletion = std::function<void(const std::optional<lua::Error>& failure)>;

    SceneLoad(Engine& owner, std::any value);

    SceneLoad(const SceneLoad&) = delete;
    SceneLoad& operator=(const SceneLoad&) = delete;

    [[nodiscard]] const std::any& getParams() const noexcept {
        return params;
    }

    // Reports the progress of the work the scene does itself, from 0 to 1, with an optional message for a loading view.
    void setProgress(float value, std::string text = {});
    [[nodiscard]] Progress getProgress() const;

    [[nodiscard]] Deferral defer();

    // Loads an asset preload group through the asset manager and holds the load until the group finished. An asset of the group that fails fails the load.
    void preload(std::string_view group, PreloadCompletion completion = {});

    [[nodiscard]] bool isFinished() const noexcept {
        return pending == 0 && !error && !cancelled;
    }
    [[nodiscard]] const std::optional<lua::Error>& getError() const noexcept {
        return error;
    }

    // Returns whether the load finished, failed or was cancelled, after which nothing changes it anymore.
    [[nodiscard]] bool isOver() const noexcept {
        return pending == 0 || error || cancelled;
    }

  private:
    friend class SceneManager;

    // Releases one hold on the load: the load hook itself, which the scene manager releases once it returned, or a deferral.
    void release();
    void fail(const lua::Error& failure);
    void cancel() noexcept;

    Engine& engine;
    std::any params;
    std::optional<float> ownProgress;
    std::string message;
    std::vector<float> groups;
    std::size_t pending = 1;
    std::optional<lua::Error> error;
    bool cancelled = false;
};

} // namespace haylen::core
