#pragma once

#include <android/native_window.h>
#include <game-activity/GameActivity.h>

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <variant>
#include <vector>

namespace haylen::platform {

// Carries what the UI thread of an Android activity hears, its lifecycle, its window and its input, to the frame thread in the order it happened. The frame thread polls the descriptor of the queue, which wakes it when the queue fills, and the UI thread waits for the messages that the frame thread must handle before the callback of the activity returns, such as the end of the window. The queue grows when a slow frame lets it fill up, so no release of a key or a touch is ever dropped.
class AndroidMessageQueue final {
  public:
    enum class Signal : std::uint8_t {
        Resumed,
        Paused,
        FocusGained,
        FocusLost,
        WindowDestroyed,
        Destroyed,
    };

    // The window of the activity, a new one or the one that changed its size, in pixels of the window.
    struct Window {
        ANativeWindow* window = nullptr;
        int width = 0;
        int height = 0;
    };

    using Payload = std::variant<Signal, Window, GameActivityMotionEvent, GameActivityKeyEvent>;

    struct Message {
        std::uint64_t sequence = 0;
        Payload payload;
    };

    AndroidMessageQueue();
    AndroidMessageQueue(const AndroidMessageQueue&) = delete;
    AndroidMessageQueue& operator=(const AndroidMessageQueue&) = delete;
    ~AndroidMessageQueue();

    // The descriptor that becomes readable when messages wait, which the looper of the frame thread polls.
    [[nodiscard]] int getDescriptor() const noexcept {
        return descriptor;
    }

    // Queues a message from the UI thread and returns its sequence, which `waitHandled` waits for.
    std::uint64_t push(const Payload& payload);

    // Quiets the descriptor that woke the frame thread, which then takes every waiting message.
    void quiet() noexcept;
    [[nodiscard]] bool pop(Message& message);

    // Tells the UI thread that the frame thread handled the messages up to the sequence.
    void markHandled(std::uint64_t sequence);
    void waitHandled(std::uint64_t sequence);

  private:
    static constexpr std::size_t kInitialCapacity = 64;

    void grow();

    std::mutex mutex;
    std::condition_variable handledChanged;
    std::vector<Message> items;
    std::size_t head = 0;
    std::size_t count = 0;
    std::uint64_t pushed = 0;
    std::uint64_t handled = 0;
    int descriptor = -1;
};

} // namespace haylen::platform
