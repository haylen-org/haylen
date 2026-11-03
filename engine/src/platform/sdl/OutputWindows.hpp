#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_metal.h>
#include <SDL3/SDL_video.h>

#include "haylen/platform/Display.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Output.hpp"
#include "platform/Host.hpp"
#include "platform/sdl/GlContext.hpp"

namespace haylen::platform::sdl {

class Window;

// The windows of the outputs of the app on the other displays of a desktop, which SDL opens there for the graphics API of the window of the app, with the context of the window of the app drawing into them under OpenGL. Each one covers its display or sits in the middle of it, follows its size and its scale, ends when the person closes it or its display disconnects, and hands the pointer over it to its output. A window that ended hides until the output closes, so the GPU device lets go of its swapchain before the window goes. Everything runs on the main thread, which is the frame thread of SDL.
class OutputWindows final {
  public:
    OutputWindows() = default;
    OutputWindows(const OutputWindows&) = delete;
    OutputWindows& operator=(const OutputWindows&) = delete;
    ~OutputWindows();

    void open(std::uint32_t number, const Display& display, const Output::Options& options, const Window& main);
    void close(std::uint32_t number) noexcept;
    void closeAll() noexcept;
    [[nodiscard]] Host::OutputReport getReport(std::uint32_t number) const;

    // Whether a window of an output holds the keyboard, which the window of the app then does not lose to another app.
    [[nodiscard]] bool holdsFocus() const noexcept;

    // Takes the events of the windows of outputs, which belong to their outputs, and returns whether the event was one of them. A display that disconnects ends the outputs on it, and the event goes on to the app.
    [[nodiscard]] bool handle(const SDL_Event& event);

  private:
    struct Entry {
        std::uint32_t number = 0;
        std::string display;
        SDL_Window* window = nullptr;
        SDL_MetalView view = nullptr;
        std::unique_ptr<GlContext> gl;
        Host::OutputReport report;
    };

    [[nodiscard]] Entry* findWindow(SDL_WindowID id) noexcept;
    [[nodiscard]] Entry* findNumber(std::uint32_t number) noexcept;
    static void destroy(Entry& entry) noexcept;
    static void refresh(Entry& entry);
    void end(Entry& entry, Output::Ending ending);
    static void deliver(const Entry& entry, platform::Event event, float x, float y);
    void handleWindow(Entry& entry, const SDL_Event& event);
    void handlePointer(const Entry& entry, const SDL_Event& event);

    std::vector<Entry> entries;
};

} // namespace haylen::platform::sdl
