#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::debug {

// Counts the objects of one type that the process created and destroyed, with the memory they hold. Every userdata type exported to Lua has one through the binding toolkit, and engine resources such as textures, fonts, bodies and tweens keep their own. Counting takes no lock, so objects may come and go on any thread. A counter of a type is created with `new` and never destroyed, because an `exit()` on another thread runs the destructors of static objects while other threads still count.
class ObjectCounter final {
  public:
    // The kind `Userdata` counts the Lua values of a bound type, and `Native` counts engine objects and resources.
    enum class Kind : std::uint8_t {
        Userdata,
        Native,
    };

    struct Snapshot {
        std::string name;
        Kind kind = Kind::Native;
        std::uint64_t created = 0;
        std::uint64_t destroyed = 0;
        std::uint64_t alive = 0;
        std::int64_t bytes = 0;
    };

    // Receives every creation and destruction while object events are on, on the thread that counted it. It must not throw, because objects are also counted while they are destroyed.
    using Observer = std::function<void(std::string_view name, bool created, std::size_t count)>;

    ObjectCounter(std::string_view counterName, Kind counterKind);
    ~ObjectCounter();

    ObjectCounter(const ObjectCounter&) = delete;
    ObjectCounter& operator=(const ObjectCounter&) = delete;

    void add(std::size_t count = 1) noexcept;
    void remove(std::size_t count = 1) noexcept;
    void addBytes(std::int64_t delta) noexcept;

    [[nodiscard]] const std::string& getName() const noexcept {
        return name;
    }
    [[nodiscard]] Snapshot getSnapshot() const noexcept;

    // Lists every counter that exists, sorted by name.
    [[nodiscard]] static std::vector<Snapshot> list();
    [[nodiscard]] static std::optional<Snapshot> find(std::string_view counterName);

    // Turns object events on with an observer, or off with an empty one.
    static void setObserver(Observer value);
    [[nodiscard]] static bool isObserved() noexcept {
        return observed.load(std::memory_order_relaxed);
    }

  private:
    struct Registry;

    [[nodiscard]] static Registry& getRegistry();
    void notify(bool wasCreated, std::size_t count) const;

    static std::atomic<bool> observed;

    std::string name;
    Kind kind;
    std::atomic<std::uint64_t> created{0};
    std::atomic<std::uint64_t> destroyed{0};
    std::atomic<std::int64_t> bytes{0};
};

} // namespace haylen::debug
