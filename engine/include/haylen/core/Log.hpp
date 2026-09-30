#pragma once

#include <atomic>
#include <cstdint>
#include <format>
#include <functional>
#include <string_view>
#include <utility>

namespace varn::log {
enum class Level;
}

namespace haylen::core {

// Engine logging routed through the Varn logger, so engine and script output share one console on every platform.
class Log final {
  public:
    enum class Level : std::uint8_t {
        Debug,
        Info,
        Warning,
        Error,
    };

    // Listeners receive every printed line, from engine code and scripts alike, on whatever thread wrote it. They must not log, add listeners or remove listeners themselves. Once `removeListener` returns, the listener is never called again.
    using Listener = std::function<void(Level level, std::string_view line)>;

    Log() = delete;

    static void setLevel(Level value) noexcept;
    [[nodiscard]] static Level getLevel() noexcept;
    static void write(Level level, std::string_view message);

    static std::uint64_t addListener(Listener listener);
    static void removeListener(std::uint64_t id);

    template <typename... Args> static void debug(std::format_string<Args...> format, Args&&... args) {
        emit(Level::Debug, format, std::forward<Args>(args)...);
    }

    template <typename... Args> static void info(std::format_string<Args...> format, Args&&... args) {
        emit(Level::Info, format, std::forward<Args>(args)...);
    }

    template <typename... Args> static void warning(std::format_string<Args...> format, Args&&... args) {
        emit(Level::Warning, format, std::forward<Args>(args)...);
    }

    template <typename... Args> static void error(std::format_string<Args...> format, Args&&... args) {
        emit(Level::Error, format, std::forward<Args>(args)...);
    }

  private:
    struct Listeners;

    static constexpr Level kDefaultLevel = Level::Info;

    // Varn's logger starts at `Debug`, so the engine default reaches it at startup and lines from Varn's own log module obey the same level.
    static std::atomic<Level> currentLevel;

    template <typename... Args> static void emit(Level messageLevel, std::format_string<Args...> format, Args&&... args) {
        if (messageLevel < getLevel()) {
            return;
        }
        write(messageLevel, std::format(format, std::forward<Args>(args)...));
    }

    [[nodiscard]] static Listeners& getListeners();
    [[nodiscard]] static Level applyDefaultLevel();
    [[nodiscard]] static Level fromVarnLevel(varn::log::Level value) noexcept;
    [[nodiscard]] static varn::log::Level toVarnLevel(Level value) noexcept;
    static void dispatch(varn::log::Level level, std::string_view line);
};

} // namespace haylen::core
